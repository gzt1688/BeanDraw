//! 颜色量化：KMeans 聚类 + DMC 色卡 Lab 最近匹配 + 可选 Floyd–Steinberg 抖动。
//!
//! 算法要点：
//! - 聚类初始化用 **k-means++**（确定性 LCG，跨平台结果一致），优于原亮度分层取样
//! - 大网格先**采样聚类**再全量归类，避免 O(n·k·iters) 在大图上超时（性能预算第 17 章）
//! - DMC 匹配在 **L\* 有序表上做窗口剪枝**，等价于全表扫描但只访问少量候选

use super::dmc_data::DMC_COLORS;
use super::perler_data::PERLER_COLORS;
use super::DmcColor;
use crate::common::error::{CoreError, CoreResult};
use crate::common::util::{lab_dist_sq, rgb_dist_sq, rgb_to_lab, Lcg};
use std::sync::OnceLock;

/// 聚类采样上限：超过则在均匀采样子集上求质心，再全量归类。
/// 4000×4000 源图 → 200×200 网格时保证量化 ≤ 5s（开发大纲第 17 章）。
pub const CLUSTER_SAMPLE_CAP: usize = 20_000;

/// 确定性随机种子（跨平台一致）。
const KMEANS_SEED: u64 = 0x5EED_1234;

// ------------------------------------------------------------
// 色卡索引（DMC 绣线 / Artkal 拼豆）
// ------------------------------------------------------------

/// 色卡来源：决定质心匹配哪套标准色。
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Card {
    /// DMC 绣线 / 珠子（十字绣、珠绣）
    Dmc,
    /// Artkal S 系列 5mm 拼豆（拼豆）
    Perler,
}

/// 按 L\* 升序排列的色卡索引，用于最近色的窗口剪枝。
struct CardIndex {
    /// (L\*, Lab, 色卡条目)，按 L\* 升序
    sorted: Vec<(f64, [f64; 3], DmcColor)>,
}

impl CardIndex {
    fn build(colors: &[DmcColor]) -> Self {
        let mut sorted: Vec<(f64, [f64; 3], DmcColor)> = colors
            .iter()
            .map(|c| {
                let lab = rgb_to_lab(c.rgb);
                (lab[0], lab, *c)
            })
            .collect();
        sorted.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap_or(std::cmp::Ordering::Equal));
        Self { sorted }
    }
}

fn dmc_index() -> &'static CardIndex {
    static INDEX: OnceLock<CardIndex> = OnceLock::new();
    INDEX.get_or_init(|| CardIndex::build(DMC_COLORS))
}

fn perler_index() -> &'static CardIndex {
    static INDEX: OnceLock<CardIndex> = OnceLock::new();
    INDEX.get_or_init(|| CardIndex::build(PERLER_COLORS))
}

fn card_index(card: Card) -> &'static CardIndex {
    match card {
        Card::Dmc => dmc_index(),
        Card::Perler => perler_index(),
    }
}

/// Lab 空间最近色（窗口剪枝，结果与全表扫描一致）。
pub fn nearest_card(rgb: [u8; 3], card: Card) -> DmcColor {
    nearest_card_lab(rgb_to_lab(rgb), card)
}

/// 已知 Lab 的最近色。
pub fn nearest_card_lab(target: [f64; 3], card: Card) -> DmcColor {
    let idx = card_index(card);
    let n = idx.sorted.len();
    debug_assert!(n > 0);

    // 起点：第一个 L* >= target.L 的位置
    let mut lo = idx.sorted.partition_point(|e| e.0 < target[0]);
    let mut hi = lo;

    let mut best = idx.sorted[lo.min(n - 1)].2;
    let mut best_d = f64::MAX;

    let probe = |i: usize, best_d: &mut f64, best: &mut DmcColor| {
        let (_, lab, color) = idx.sorted[i];
        let d = lab_dist_sq(target, lab);
        if d < *best_d {
            *best_d = d;
            *best = color;
        }
    };

    // 以 L* 距离为半径逐步向两侧扩展：
    // 一旦最近候选的 ΔL 已超过当前最优距离，剩余候选不可能更优（三角不等式）。
    loop {
        let left_dl = if lo > 0 {
            target[0] - idx.sorted[lo - 1].0
        } else {
            f64::INFINITY
        };
        let right_dl = if hi < n {
            idx.sorted[hi].0 - target[0]
        } else {
            f64::INFINITY
        };
        let next_dl = left_dl.min(right_dl);
        if next_dl.is_infinite() || next_dl * next_dl > best_d {
            break;
        }
        if left_dl <= right_dl {
            lo -= 1;
            probe(lo, &mut best_d, &mut best);
        } else {
            probe(hi, &mut best_d, &mut best);
            hi += 1;
        }
    }
    best
}

/// 在给定调色板内找 Lab 最近色（稀有色合并 / 全局换色用）。
/// palette 为空时返回 None。
pub fn nearest_in_palette(rgb: [u8; 3], palette: &[DmcColor]) -> Option<usize> {
    if palette.is_empty() {
        return None;
    }
    let target = rgb_to_lab(rgb);
    let mut best = 0usize;
    let mut best_d = f64::MAX;
    for (i, c) in palette.iter().enumerate() {
        let d = lab_dist_sq(target, rgb_to_lab(c.rgb));
        if d < best_d {
            best_d = d;
            best = i;
        }
    }
    Some(best)
}

// ------------------------------------------------------------
// KMeans
// ------------------------------------------------------------

/// KMeans 聚类（k-means++ 确定性初始化）。
/// 返回 (质心, 每样本所属质心索引)。
pub fn kmeans(samples: &[[u8; 3]], k: usize, iters: u32) -> (Vec<[u8; 3]>, Vec<usize>) {
    kmeans_sampled(samples, k, iters, usize::MAX)
}

/// KMeans 聚类（可限制参与求质心的样本数）。
///
/// `sample_cap` 小于样本量时，先均匀取 `sample_cap` 个样本迭代求质心，
/// 再把全部样本归类到最近质心——聚类质量几乎无损，大图耗时显著下降。
pub fn kmeans_sampled(
    samples: &[[u8; 3]],
    k: usize,
    iters: u32,
    sample_cap: usize,
) -> (Vec<[u8; 3]>, Vec<usize>) {
    let n = samples.len();
    if n == 0 || k == 0 {
        return (Vec::new(), Vec::new());
    }
    let k = k.min(n);

    // ---- k-means++ 初始化 ----
    let train: Vec<usize> = if sample_cap < n {
        (0..sample_cap).map(|i| i * n / sample_cap).collect()
    } else {
        (0..n).collect()
    };
    let mut rng = Lcg::new(KMEANS_SEED);
    let mut centroids: Vec<[u8; 3]> = Vec::with_capacity(k);
    centroids.push(samples[train[rng.below(train.len() as u64) as usize]]);

    // 到已选质心的最小距离平方（惰性更新）
    let mut closest = vec![u32::MAX; train.len()];
    while centroids.len() < k {
        update_closest(samples, &train, &centroids, &mut closest);
        let total: u64 = closest.iter().map(|&d| d as u64).sum();
        let picked = if total == 0 {
            // 所有训练样本都已被质心覆盖：退化为均匀取样，保证 k 个质心
            train[centroids.len() % train.len()]
        } else {
            let mut r = rng.below(total);
            let mut idx = train.len() - 1;
            for (i, &d) in closest.iter().enumerate() {
                if r < d as u64 {
                    idx = i;
                    break;
                }
                r -= d as u64;
            }
            train[idx]
        };
        centroids.push(samples[picked]);
    }

    // ---- Lloyd 迭代（在训练子集上更新质心，全量归类） ----
    let mut assign = vec![0usize; n];
    for _ in 0..iters.max(1) {
        let mut changed = false;
        for (i, s) in samples.iter().enumerate() {
            let best = nearest_centroid(*s, &centroids);
            if assign[i] != best {
                assign[i] = best;
                changed = true;
            }
        }
        if !changed && centroids.len() == k {
            break;
        }
        update_centroids(samples, &train, &assign, k, &mut centroids);
    }
    (centroids, assign)
}

fn nearest_centroid(s: [u8; 3], centroids: &[[u8; 3]]) -> usize {
    let mut best = 0usize;
    let mut best_d = u32::MAX;
    for (c, cent) in centroids.iter().enumerate() {
        let d = rgb_dist_sq(s, *cent);
        if d < best_d {
            best_d = d;
            best = c;
        }
    }
    best
}

fn update_closest(
    samples: &[[u8; 3]],
    train: &[usize],
    centroids: &[[u8; 3]],
    closest: &mut [u32],
) {
    for (slot, &i) in train.iter().enumerate() {
        closest[slot] = rgb_dist_sq(samples[i], centroids[centroids.len() - 1]).min(closest[slot]);
    }
}

fn update_centroids(
    samples: &[[u8; 3]],
    train: &[usize],
    assign: &[usize],
    k: usize,
    centroids: &mut [[u8; 3]],
) {
    let mut sums = vec![[0u64; 3]; k];
    let mut counts = vec![0u64; k];
    for &i in train {
        let a = assign[i].min(k - 1);
        let s = samples[i];
        sums[a][0] += s[0] as u64;
        sums[a][1] += s[1] as u64;
        sums[a][2] += s[2] as u64;
        counts[a] += 1;
    }
    for c in 0..k {
        // 空簇（counts == 0）保持原质心不动：比随机重置更确定，避免跨平台结果漂移
        if let (Some(r), Some(g), Some(b)) = (
            sums[c][0].checked_div(counts[c]),
            sums[c][1].checked_div(counts[c]),
            sums[c][2].checked_div(counts[c]),
        ) {
            centroids[c] = [r as u8, g as u8, b as u8];
        }
    }
}

// ------------------------------------------------------------
// 量化入口
// ------------------------------------------------------------

pub struct QuantizeResult {
    /// 去重后的 DMC 调色板
    pub palette: Vec<DmcColor>,
    /// 每格调色板索引；usize::MAX = 空格
    pub assignments: Vec<usize>,
    /// 聚类阶段样本数（0 表示空图）
    pub sample_count: usize,
}

/// 量化入口（默认采样上限，DMC 色卡）。
pub fn quantize(
    grid: &[Option<[u8; 3]>],
    grid_w: u32,
    grid_h: u32,
    max_colors: usize,
    dither: bool,
) -> CoreResult<QuantizeResult> {
    quantize_with_options(
        grid,
        grid_w,
        grid_h,
        max_colors,
        dither,
        CLUSTER_SAMPLE_CAP,
        Card::Dmc,
    )
}

/// 量化入口（可指定聚类采样上限与目标色卡；传 usize::MAX 表示全量参与）。
pub fn quantize_with_options(
    grid: &[Option<[u8; 3]>],
    grid_w: u32,
    grid_h: u32,
    max_colors: usize,
    dither: bool,
    sample_cap: usize,
    card: Card,
) -> CoreResult<QuantizeResult> {
    if grid_w == 0 || grid_h == 0 {
        return Err(CoreError::InvalidArg("empty grid".into()));
    }
    let expected = (grid_w * grid_h) as usize;
    if grid.len() != expected {
        return Err(CoreError::InvalidArg(format!(
            "grid buffer size mismatch: {} != {}",
            grid.len(),
            expected
        )));
    }

    let samples: Vec<[u8; 3]> = grid.iter().flatten().copied().collect();
    if samples.is_empty() {
        return Ok(QuantizeResult {
            palette: Vec::new(),
            assignments: vec![usize::MAX; expected],
            sample_count: 0,
        });
    }

    // 去重样本数决定实际颜色上限
    let mut distinct = samples.clone();
    distinct.sort_unstable();
    distinct.dedup();
    let k = max_colors.clamp(1, 64).min(distinct.len());

    let (centroids, centroid_assign) = kmeans_sampled(&samples, k, 12, sample_cap);

    // 质心 → 目标色卡匹配并去重
    let mut palette: Vec<DmcColor> = Vec::new();
    let mut remap = vec![usize::MAX; centroids.len()];
    for (i, cent) in centroids.iter().enumerate() {
        let matched = nearest_card(*cent, card);
        if let Some(pos) = palette.iter().position(|p| p.code == matched.code) {
            remap[i] = pos;
        } else {
            remap[i] = palette.len();
            palette.push(matched);
        }
    }

    let assignments = if dither {
        floyd_steinberg(grid, grid_w, grid_h, &palette)
    } else {
        let mut out = vec![usize::MAX; expected];
        let mut sample_i = 0usize;
        for (cell, slot) in grid.iter().zip(out.iter_mut()) {
            if cell.is_some() {
                *slot = remap[centroid_assign[sample_i]];
                sample_i += 1;
            }
        }
        out
    };

    Ok(QuantizeResult {
        palette,
        assignments,
        sample_count: samples.len(),
    })
}

/// Floyd–Steinberg 误差扩散（针对已匹配的 DMC 调色板）。
fn floyd_steinberg(
    grid: &[Option<[u8; 3]>],
    grid_w: u32,
    grid_h: u32,
    palette: &[DmcColor],
) -> Vec<usize> {
    let w = grid_w as usize;
    let h = grid_h as usize;
    let mut buf: Vec<[f64; 3]> = grid
        .iter()
        .map(|c| {
            c.map(|v| [v[0] as f64, v[1] as f64, v[2] as f64])
                .unwrap_or([0.0; 3])
        })
        .collect();
    let mut out = vec![usize::MAX; w * h];

    let add_err = |buf: &mut Vec<[f64; 3]>, x: i64, y: i64, err: [f64; 3], factor: f64| {
        if x < 0 || y < 0 || x >= w as i64 || y >= h as i64 {
            return;
        }
        let i = (y as usize) * w + (x as usize);
        if grid[i].is_none() {
            return;
        }
        buf[i][0] += err[0] * factor;
        buf[i][1] += err[1] * factor;
        buf[i][2] += err[2] * factor;
    };

    for y in 0..h {
        for x in 0..w {
            let i = y * w + x;
            if grid[i].is_none() || palette.is_empty() {
                continue;
            }
            let px = buf[i];
            // 最近调色板色（RGB 距离）
            let mut best = 0usize;
            let mut best_d = f64::MAX;
            for (p, color) in palette.iter().enumerate() {
                let dr = px[0] - color.rgb[0] as f64;
                let dg = px[1] - color.rgb[1] as f64;
                let db = px[2] - color.rgb[2] as f64;
                let d = dr * dr + dg * dg + db * db;
                if d < best_d {
                    best_d = d;
                    best = p;
                }
            }
            out[i] = best;
            let c = palette[best].rgb;
            let err = [
                px[0] - c[0] as f64,
                px[1] - c[1] as f64,
                px[2] - c[2] as f64,
            ];
            add_err(&mut buf, x as i64 + 1, y as i64, err, 7.0 / 16.0);
            add_err(&mut buf, x as i64 - 1, y as i64 + 1, err, 3.0 / 16.0);
            add_err(&mut buf, x as i64, y as i64 + 1, err, 5.0 / 16.0);
            add_err(&mut buf, x as i64 + 1, y as i64 + 1, err, 1.0 / 16.0);
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn kmeans_separates_two_clusters() {
        let mut samples = Vec::new();
        for _ in 0..50 {
            samples.push([200, 10, 10]);
        }
        for _ in 0..50 {
            samples.push([10, 10, 200]);
        }
        let (centroids, assign) = kmeans(&samples, 2, 20);
        assert_eq!(centroids.len(), 2);
        assert!(assign[..50].iter().all(|&a| a == assign[0]));
        assert!(assign[50..].iter().all(|&a| a == assign[50]));
        assert_ne!(assign[0], assign[50]);
    }

    #[test]
    fn kmeans_is_deterministic() {
        let samples: Vec<[u8; 3]> = (0..300)
            .map(|i| [(i % 17) as u8 * 15, (i % 7) as u8 * 30, (i % 5) as u8 * 50])
            .collect();
        let a = kmeans(&samples, 8, 12);
        let b = kmeans(&samples, 8, 12);
        assert_eq!(a.0, b.0);
        assert_eq!(a.1, b.1);
    }

    /// 聚类标签是任意的，比较「划分」而非标签：按首次出现顺序重编号后比较。
    fn canonical(assign: &[usize]) -> Vec<usize> {
        let mut map: Vec<usize> = Vec::new();
        let mut out = Vec::with_capacity(assign.len());
        for &a in assign {
            let id = match map.iter().position(|&x| x == a) {
                Some(i) => i,
                None => {
                    map.push(a);
                    map.len() - 1
                }
            };
            out.push(id);
        }
        out
    }

    #[test]
    fn kmeans_sampled_matches_full_on_simple_data() {
        // 两个远离的簇：采样与否应得到相同的划分（标签顺序可能不同）
        let mut samples = Vec::new();
        for _ in 0..500 {
            samples.push([240, 20, 20]);
        }
        for _ in 0..500 {
            samples.push([20, 20, 240]);
        }
        let full = kmeans(&samples, 2, 12);
        let sampled = kmeans_sampled(&samples, 2, 12, 64);
        assert_eq!(canonical(&full.1), canonical(&sampled.1));
    }

    #[test]
    fn kmeans_respects_requested_cluster_count() {
        let samples: Vec<[u8; 3]> = (0..64)
            .map(|i| [(i * 4) as u8, (255 - i * 4) as u8, 128])
            .collect();
        let (centroids, assign) = kmeans(&samples, 5, 8);
        assert_eq!(centroids.len(), 5);
        assert_eq!(assign.len(), samples.len());
        assert!(assign.iter().all(|&a| a < 5));
    }

    #[test]
    fn nearest_card_matches_exact_entry() {
        // DMC 321 Red = [199, 15, 46]
        let c = nearest_card([199, 15, 46], Card::Dmc);
        assert_eq!(c.code, "321");
        // Artkal S13 Black = [0, 0, 0]
        let p = nearest_card([0, 0, 0], Card::Perler);
        assert_eq!(p.code, "S13");
    }

    #[test]
    fn perler_card_is_separate_from_dmc() {
        // 拼豆色卡与 DMC 是两套独立体系：黑在 DMC 为 310，拼豆为 S13
        assert_eq!(nearest_card([0, 0, 0], Card::Dmc).code, "310");
        assert_eq!(nearest_card([0, 0, 0], Card::Perler).code, "S13");
        for c in PERLER_COLORS.iter() {
            assert!(
                c.code.starts_with('S'),
                "拼豆色号应以 S 开头，实际 {}",
                c.code
            );
        }
    }

    #[test]
    fn nearest_card_pruned_equals_bruteforce() {
        // 窗口剪枝必须与全表扫描结果一致（两套色卡各测）
        let probes = [
            [0u8, 0, 0],
            [255, 255, 255],
            [128, 64, 32],
            [10, 200, 180],
            [90, 90, 90],
            [255, 128, 0],
        ];
        for p in probes {
            for (card, table) in [(Card::Dmc, DMC_COLORS), (Card::Perler, PERLER_COLORS)] {
                let pruned = nearest_card(p, card);
                let lab = rgb_to_lab(p);
                let brute = table
                    .iter()
                    .map(|c| (lab_dist_sq(lab, rgb_to_lab(c.rgb)), *c))
                    .fold(
                        (f64::MAX, table[0]),
                        |acc, x| if x.0 < acc.0 { x } else { acc },
                    )
                    .1;
                assert_eq!(pruned.code, brute.code, "mismatch for {p:?} on {card:?}");
            }
        }
    }

    #[test]
    fn nearest_in_palette_picks_closest() {
        let palette = vec![
            DmcColor {
                code: "310",
                name: "Black",
                rgb: [0, 0, 0],
            },
            DmcColor {
                code: "B5200",
                name: "Snow White",
                rgb: [255, 255, 255],
            },
        ];
        assert_eq!(nearest_in_palette([10, 10, 10], &palette), Some(0));
        assert_eq!(nearest_in_palette([250, 250, 250], &palette), Some(1));
        assert_eq!(nearest_in_palette([10, 10, 10], &[]), None);
    }

    #[test]
    fn quantize_dedupes_palette() {
        let grid: Vec<Option<[u8; 3]>> =
            (0..64).map(|i| Some([(i % 2) as u8 * 250, 0, 0])).collect();
        let r = quantize(&grid, 8, 8, 16, false).unwrap();
        assert!(r.palette.len() <= 2);
        assert!(r.assignments.iter().all(|&a| a != usize::MAX));
    }

    #[test]
    fn quantize_empty_image() {
        let grid = vec![None; 16];
        let r = quantize(&grid, 4, 4, 16, false).unwrap();
        assert!(r.palette.is_empty());
        assert!(r.assignments.iter().all(|&a| a == usize::MAX));
    }

    #[test]
    fn dither_keeps_every_cell_assigned() {
        let grid: Vec<Option<[u8; 3]>> = (0..256)
            .map(|i| Some([(i % 16) as u8 * 16, 80, 160 - (i / 16) as u8 * 10]))
            .collect();
        let r = quantize(&grid, 16, 16, 8, true).unwrap();
        assert!(r.assignments.iter().all(|&a| a != usize::MAX));
    }
}
