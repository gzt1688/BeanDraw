//! 图像下采样：源图 → 像素网格。
//!
//! 提供三种「一格一色」归并策略（参考 Cstitch 的 Mode/Median 与 KXStitch 的取样思想）：
//! - `MEAN`   盒式平均：过渡平滑，但会产生原图与色卡中不存在的中间色
//! - `MODE`   众数：取格内出现最多的颜色桶均值，保留实色、轮廓锐利
//! - `MEDOID` 中心色：取与格内所有像素总距离最小的候选色，兼顾平滑与实色
//!
//! 三者在有透明通道时行为一致：平均 alpha < 50% 的格留空（`None`）。

use crate::common::error::{CoreError, CoreResult};
use crate::common::ffi_types::{sample, MAX_GRID_SIDE};

/// 颜色桶位深（每通道取高 5 位 = 32 级），用于众数 / 中心色归并的去噪分桶。
const BIN_SHIFT: u8 = 3;
/// 中心色归并的候选上限（超过则按出现次数截断）。
const MAX_MEDOID_CANDIDATES: usize = 64;

/// 下采样结果：None 表示该格全透明（留空）。
pub fn downscale(
    rgba: &[u8],
    src_w: u32,
    src_h: u32,
    grid_w: u32,
    grid_h: u32,
    mode: u32,
) -> CoreResult<Vec<Option<[u8; 3]>>> {
    match mode {
        sample::MODE => downscale_mode(rgba, src_w, src_h, grid_w, grid_h),
        sample::MEDOID => downscale_medoid(rgba, src_w, src_h, grid_w, grid_h),
        _ => downscale_mean(rgba, src_w, src_h, grid_w, grid_h),
    }
}

/// 盒式平均（默认）。
pub fn downscale_mean(
    rgba: &[u8],
    src_w: u32,
    src_h: u32,
    grid_w: u32,
    grid_h: u32,
) -> CoreResult<Vec<Option<[u8; 3]>>> {
    check_bounds(rgba, src_w, src_h, grid_w, grid_h)?;

    let mut out = Vec::with_capacity((grid_w * grid_h) as usize);
    for gy in 0..grid_h {
        for gx in 0..grid_w {
            let (x0, x1, y0, y1) = cell_span(gx, gy, grid_w, grid_h, src_w, src_h);

            let (mut r, mut g, mut b, mut a_sum, mut count) = (0u64, 0u64, 0u64, 0u64, 0u64);
            for y in y0..y1 {
                for x in x0..x1 {
                    let i = ((y * src_w + x) * 4) as usize;
                    let a = rgba[i + 3] as u64;
                    r += rgba[i] as u64 * a;
                    g += rgba[i + 1] as u64 * a;
                    b += rgba[i + 2] as u64 * a;
                    a_sum += a;
                    count += 1;
                }
            }
            if a_sum < (count * 128) {
                out.push(None); // 基本透明 → 留空
            } else {
                out.push(Some([
                    (r / a_sum) as u8,
                    (g / a_sum) as u8,
                    (b / a_sum) as u8,
                ]));
            }
        }
    }
    Ok(out)
}

/// 众数：出现次数最多的颜色桶均值（保留实色，轮廓锐利）。
pub fn downscale_mode(
    rgba: &[u8],
    src_w: u32,
    src_h: u32,
    grid_w: u32,
    grid_h: u32,
) -> CoreResult<Vec<Option<[u8; 3]>>> {
    check_bounds(rgba, src_w, src_h, grid_w, grid_h)?;

    let mut out = Vec::with_capacity((grid_w * grid_h) as usize);
    for gy in 0..grid_h {
        for gx in 0..grid_w {
            let (x0, x1, y0, y1) = cell_span(gx, gy, grid_w, grid_h, src_w, src_h);
            let mut bins: Vec<ColorBin> = Vec::new();
            let mut opaque = 0u64;
            let mut total = 0u64;

            for y in y0..y1 {
                for x in x0..x1 {
                    let i = ((y * src_w + x) * 4) as usize;
                    total += 1;
                    if rgba[i + 3] < 128 {
                        continue;
                    }
                    opaque += 1;
                    let px = [rgba[i], rgba[i + 1], rgba[i + 2]];
                    accumulate(&mut bins, bin_key(px[0], px[1], px[2]), px);
                }
            }

            if opaque * 2 < total {
                out.push(None); // 半格以上透明 → 留空
                continue;
            }
            out.push(dominant_bin_color(&bins));
        }
    }
    Ok(out)
}

/// 中心色（medoid）：与格内所有像素距离和最小的候选色。
///
/// 候选集为格内实际出现的颜色桶均值，因此输出一定是图纸里真实存在的颜色，
/// 不会像盒式平均那样凭空造出中间色。
pub fn downscale_medoid(
    rgba: &[u8],
    src_w: u32,
    src_h: u32,
    grid_w: u32,
    grid_h: u32,
) -> CoreResult<Vec<Option<[u8; 3]>>> {
    check_bounds(rgba, src_w, src_h, grid_w, grid_h)?;

    let mut out = Vec::with_capacity((grid_w * grid_h) as usize);
    for gy in 0..grid_h {
        for gx in 0..grid_w {
            let (x0, x1, y0, y1) = cell_span(gx, gy, grid_w, grid_h, src_w, src_h);

            // 第一遍：收集不透明像素 + 分桶统计（候选集）
            let mut pixels: Vec<[u8; 3]> = Vec::new();
            let mut bins: Vec<ColorBin> = Vec::new();
            let mut opaque = 0u64;
            let mut total = 0u64;
            for y in y0..y1 {
                for x in x0..x1 {
                    let i = ((y * src_w + x) * 4) as usize;
                    total += 1;
                    if rgba[i + 3] < 128 {
                        continue;
                    }
                    opaque += 1;
                    let px = [rgba[i], rgba[i + 1], rgba[i + 2]];
                    pixels.push(px);
                    accumulate(&mut bins, bin_key(px[0], px[1], px[2]), px);
                }
            }

            if opaque * 2 < total || pixels.is_empty() {
                out.push(None);
                continue;
            }

            // 候选取出现次数最多的若干桶均值
            let mut order: Vec<usize> = (0..bins.len()).collect();
            order.sort_by(|&a, &b| bins[b].2.cmp(&bins[a].2));
            order.truncate(MAX_MEDOID_CANDIDATES);

            let mut best = pixels[0];
            let mut best_sum = u64::MAX;
            for &bi in &order {
                let (_, sums, count) = bins[bi];
                if count == 0 {
                    continue;
                }
                let cand = bin_color(sums, count);
                let mut sum = 0u64;
                for p in &pixels {
                    sum += channel_dist_sum(*p, cand);
                    if sum >= best_sum {
                        break; // 提前退出（参考 Cstitch median 的剪枝）
                    }
                }
                if sum < best_sum {
                    best_sum = sum;
                    best = cand;
                }
            }
            out.push(Some(best));
        }
    }
    Ok(out)
}

/// 兼容旧签名：等价于 `downscale(..., sample::MEAN)`。
pub fn downscale_to_grid(
    rgba: &[u8],
    src_w: u32,
    src_h: u32,
    grid_w: u32,
    grid_h: u32,
) -> CoreResult<Vec<Option<[u8; 3]>>> {
    downscale_mean(rgba, src_w, src_h, grid_w, grid_h)
}

/// 网格线性索引。
#[inline]
pub fn cell_index(x: u32, y: u32, grid_w: u32) -> usize {
    (y * grid_w + x) as usize
}

// ------------------------------------------------------------
// 内部工具
// ------------------------------------------------------------

fn check_bounds(rgba: &[u8], src_w: u32, src_h: u32, grid_w: u32, grid_h: u32) -> CoreResult<()> {
    if src_w == 0 || src_h == 0 {
        return Err(CoreError::InvalidArg("source image is empty".into()));
    }
    if grid_w == 0 || grid_h == 0 || grid_w > MAX_GRID_SIDE || grid_h > MAX_GRID_SIDE {
        return Err(CoreError::InvalidArg(format!(
            "grid size {grid_w}x{grid_h} out of range 1..={MAX_GRID_SIDE}"
        )));
    }
    let expected = (src_w as usize) * (src_h as usize) * 4;
    if rgba.len() < expected {
        return Err(CoreError::InvalidArg(format!(
            "rgba buffer too small: {} < {}",
            rgba.len(),
            expected
        )));
    }
    Ok(())
}

/// 格 (gx,gy) 覆盖的源像素区间（已裁剪到图像范围，至少含一个像素）。
fn cell_span(
    gx: u32,
    gy: u32,
    grid_w: u32,
    grid_h: u32,
    src_w: u32,
    src_h: u32,
) -> (u32, u32, u32, u32) {
    let x0 = (gx as u64 * src_w as u64 / grid_w as u64) as u32;
    let mut x1 = ((gx as u64 + 1) * src_w as u64 / grid_w as u64) as u32;
    if x1 <= x0 {
        x1 = x0 + 1;
    }
    if x1 > src_w {
        x1 = src_w;
    }
    let y0 = (gy as u64 * src_h as u64 / grid_h as u64) as u32;
    let mut y1 = ((gy as u64 + 1) * src_h as u64 / grid_h as u64) as u32;
    if y1 <= y0 {
        y1 = y0 + 1;
    }
    if y1 > src_h {
        y1 = src_h;
    }
    (x0, x1, y0, y1)
}

#[inline]
fn bin_key(r: u8, g: u8, b: u8) -> (u8, u8, u8) {
    (r >> BIN_SHIFT, g >> BIN_SHIFT, b >> BIN_SHIFT)
}

/// 颜色桶：(分桶键, RGB 累加和, 计数)。
///
/// 格内像素先按 `BIN_SHIFT` 量化分桶，再在桶上做众数 / medoid，
/// 桶数通常 < 32，线性查找快于 HashMap。
type ColorBin = ((u8, u8, u8), [u64; 3], u64);

/// 分桶累加：桶 = (键, RGB 累加和, 计数)。桶数通常 < 32，线性查找快于 HashMap。
fn accumulate(bins: &mut Vec<ColorBin>, key: (u8, u8, u8), px: [u8; 3]) {
    for (k, sums, count) in bins.iter_mut() {
        if *k == key {
            sums[0] += px[0] as u64;
            sums[1] += px[1] as u64;
            sums[2] += px[2] as u64;
            *count += 1;
            return;
        }
    }
    bins.push((key, [px[0] as u64, px[1] as u64, px[2] as u64], 1));
}

/// 桶均值。
#[inline]
fn bin_color(sums: [u64; 3], count: u64) -> [u8; 3] {
    debug_assert!(count > 0);
    [
        (sums[0] / count) as u8,
        (sums[1] / count) as u8,
        (sums[2] / count) as u8,
    ]
}

/// 出现次数最多的桶均值（平局时保留先出现者，保证结果确定）。
fn dominant_bin_color(bins: &[ColorBin]) -> Option<[u8; 3]> {
    let mut best: Option<([u8; 3], u64)> = None;
    for (_, sums, count) in bins {
        if *count == 0 {
            continue;
        }
        let color = bin_color(*sums, *count);
        match best {
            None => best = Some((color, *count)),
            Some((_, n)) if *count > n => best = Some((color, *count)),
            Some(_) => {}
        }
    }
    best.map(|(c, _)| c)
}

/// RGB 曼哈顿距离（中心色评价用；整数运算，跨平台结果一致）。
#[inline]
fn channel_dist_sum(a: [u8; 3], b: [u8; 3]) -> u64 {
    (a[0].abs_diff(b[0]) as u64) + (a[1].abs_diff(b[1]) as u64) + (a[2].abs_diff(b[2]) as u64)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn downscale_uniform_color() {
        // 4x4 纯红 → 2x2 网格应全红
        let mut rgba = Vec::new();
        for _ in 0..16 {
            rgba.extend_from_slice(&[255, 0, 0, 255]);
        }
        let grid = downscale_to_grid(&rgba, 4, 4, 2, 2).unwrap();
        assert_eq!(grid.len(), 4);
        assert!(grid.iter().all(|c| *c == Some([255, 0, 0])));
    }

    #[test]
    fn transparent_cells_are_none() {
        let mut rgba = vec![0u8; 2 * 2 * 4];
        rgba[3] = 255; // 左上不透明
        rgba[15] = 255; // 右下不透明
        let grid = downscale_to_grid(&rgba, 2, 2, 1, 1).unwrap();
        // 2 个不透明 2 个透明 → 平均 alpha 128，阈值判定为透明
        assert_eq!(grid, vec![None]);
    }

    #[test]
    fn invalid_grid_rejected() {
        let rgba = vec![0u8; 16];
        assert!(downscale_to_grid(&rgba, 2, 2, 0, 2).is_err());
        assert!(downscale_to_grid(&rgba, 2, 2, MAX_GRID_SIDE + 1, 2).is_err());
        assert!(downscale_to_grid(&[0u8; 4], 2, 2, 2, 2).is_err());
    }

    /// 造一块 2×2 源像素：3 个红 + 1 个蓝 → 众数应取红，盒式平均会造出紫。
    fn mixed_block() -> Vec<u8> {
        let colors = [
            [220, 20, 20, 255u8],
            [220, 20, 20, 255],
            [220, 20, 20, 255],
            [20, 20, 220, 255],
        ];
        let mut v = Vec::new();
        for c in colors {
            v.extend_from_slice(&c);
        }
        v
    }

    #[test]
    fn mode_keeps_dominant_real_color() {
        let rgba = mixed_block();
        let grid = downscale_mode(&rgba, 2, 2, 1, 1).unwrap();
        let c = grid[0].expect("opaque cell");
        // 众数：红占 3/4，结果必须偏向红而不是蓝
        assert!(c[0] > c[2], "mode should pick red, got {c:?}");
    }

    #[test]
    fn medoid_returns_color_present_in_block() {
        let rgba = mixed_block();
        let grid = downscale_medoid(&rgba, 2, 2, 1, 1).unwrap();
        let c = grid[0].expect("opaque cell");
        // 中心色一定落在某个真实像素的取值域（红或蓝），不会是二者的紫色均值
        let is_red = c[0] > 150 && c[2] < 80;
        let is_blue = c[2] > 150 && c[0] < 80;
        assert!(is_red || is_blue, "medoid must be a real color, got {c:?}");
    }

    #[test]
    fn mean_blends_while_mode_does_not() {
        let rgba = mixed_block();
        let mean = downscale_mean(&rgba, 2, 2, 1, 1).unwrap()[0].unwrap();
        let mode = downscale_mode(&rgba, 2, 2, 1, 1).unwrap()[0].unwrap();
        // 均值一定被蓝拉低红通道；众数不受少数蓝像素影响
        assert!(mode[0] > mean[0], "mode {mode:?} vs mean {mean:?}");
    }

    #[test]
    fn downscale_dispatch_matches_mode() {
        let rgba = mixed_block();
        let direct = downscale_mode(&rgba, 2, 2, 1, 1).unwrap();
        let via_dispatch = downscale(&rgba, 2, 2, 1, 1, sample::MODE).unwrap();
        assert_eq!(direct, via_dispatch);
        assert_eq!(
            downscale(&rgba, 2, 2, 1, 1, sample::MEAN).unwrap(),
            downscale_mean(&rgba, 2, 2, 1, 1).unwrap()
        );
    }

    #[test]
    fn upscale_grid_one_to_one_is_exact() {
        // 网格比原图更密：每格至少取一个像素，结果应等于最近邻采样
        let rgba: Vec<u8> = [[10u8, 20, 30, 255], [200, 210, 220, 255]]
            .into_iter()
            .flatten()
            .collect();
        let grid = downscale(&rgba, 2, 1, 2, 1, sample::MEDOID).unwrap();
        assert_eq!(grid.len(), 2);
        assert_eq!(grid[0], Some([10, 20, 30]));
        assert_eq!(grid[1], Some([200, 210, 220]));
    }
}
