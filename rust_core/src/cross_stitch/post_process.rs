//! 图纸后处理（开发大纲 Phase 1 十字绣编辑能力）：
//! 去杂点、稀有色合并、区域填充、矩形填充、全局换色。
//!
//! 所有函数直接就地修改 `cells`（行主序，长度 = width × height），
//! 返回被修改的格子数，便于 UI 侧提示与命令栈标签。

use super::DmcColor;
use crate::common::ffi_types::{cleanup, CmGridCell, COLOR_NONE};

/// 4 邻域偏移（上、下、左、右）。
const NEIGHBORS: [(i32, i32); 4] = [(0, -1), (0, 1), (-1, 0), (1, 0)];

#[inline]
fn idx(x: u32, y: u32, w: u32) -> usize {
    (y * w + x) as usize
}

#[inline]
fn in_bounds(x: i32, y: i32, w: u32, h: u32) -> bool {
    x >= 0 && y >= 0 && (x as u32) < w && (y as u32) < h
}

/// 取邻域中出现最多的颜色（忽略空格）；全空返回 None。
/// 平局时取 Lab 距离意义上更"居中"的颜色代价过高，这里按索引升序取先出现者，保证确定性。
fn dominant_neighbor(cells: &[CmGridCell], w: u32, h: u32, x: u32, y: u32) -> Option<u32> {
    let mut counts: Vec<(u32, u32)> = Vec::new(); // (color, count)
    for (dx, dy) in NEIGHBORS {
        let nx = x as i32 + dx;
        let ny = y as i32 + dy;
        if !in_bounds(nx, ny, w, h) {
            continue;
        }
        let c = cells[idx(nx as u32, ny as u32, w)].color_index;
        if c == COLOR_NONE {
            continue;
        }
        if let Some(slot) = counts.iter_mut().find(|(col, _)| *col == c) {
            slot.1 += 1;
        } else {
            counts.push((c, 1));
        }
    }
    counts
        .into_iter()
        .max_by_key(|(col, n)| (*n, u32::MAX - *col))
        .map(|(col, _)| col)
}

/// 去杂点：把孤立 / 极小的同色块并入邻域主色。
///
/// - `cleanup::LIGHT`：仅处理四邻域内无同色的孤立单针
/// - `cleanup::STRONG`：额外处理面积 < 3 格的碎片（连通域分析）
///
/// 返回被修改的格子数。
pub fn remove_isolated(cells: &mut [CmGridCell], w: u32, h: u32, strength: u32) -> u32 {
    if strength == cleanup::OFF || w == 0 || h == 0 {
        return 0;
    }
    if cells.len() != (w * h) as usize {
        return 0;
    }

    let mut changed = 0u32;

    // 第一遍：孤立单针（四邻域无同色）
    let mut targets: Vec<(u32, u32)> = Vec::new();
    for y in 0..h {
        for x in 0..w {
            let i = idx(x, y, w);
            let c = cells[i].color_index;
            if c == COLOR_NONE {
                continue;
            }
            let same = NEIGHBORS.iter().any(|(dx, dy)| {
                let nx = x as i32 + dx;
                let ny = y as i32 + dy;
                in_bounds(nx, ny, w, h) && cells[idx(nx as u32, ny as u32, w)].color_index == c
            });
            if !same {
                targets.push((x, y));
            }
        }
    }
    for (x, y) in targets {
        // 逐个处理：前一步的修改会影响后一步的邻域判断，这是期望的收敛行为
        let c = cells[idx(x, y, w)].color_index;
        if c == COLOR_NONE {
            continue;
        }
        let still_isolated = NEIGHBORS.iter().all(|(dx, dy)| {
            let nx = x as i32 + dx;
            let ny = y as i32 + dy;
            !in_bounds(nx, ny, w, h) || cells[idx(nx as u32, ny as u32, w)].color_index != c
        });
        if !still_isolated {
            continue;
        }
        if let Some(target) = dominant_neighbor(cells, w, h, x, y) {
            let i = idx(x, y, w);
            cells[i].color_index = target;
            cells[i].symbol_index = target;
            changed += 1;
        }
    }

    if strength < cleanup::STRONG {
        return changed;
    }

    // 第二遍：面积 < 3 格的同色连通域
    let mut visited = vec![false; cells.len()];
    let mut stack: Vec<(u32, u32)> = Vec::new();
    for y in 0..h {
        for x in 0..w {
            let start = idx(x, y, w);
            if visited[start] || cells[start].color_index == COLOR_NONE {
                continue;
            }
            let color = cells[start].color_index;
            stack.clear();
            stack.push((x, y));
            visited[start] = true;
            let mut component: Vec<(u32, u32)> = Vec::new();
            while let Some((cx, cy)) = stack.pop() {
                component.push((cx, cy));
                for (dx, dy) in NEIGHBORS {
                    let nx = cx as i32 + dx;
                    let ny = cy as i32 + dy;
                    if !in_bounds(nx, ny, w, h) {
                        continue;
                    }
                    let ni = idx(nx as u32, ny as u32, w);
                    if visited[ni] || cells[ni].color_index != color {
                        continue;
                    }
                    visited[ni] = true;
                    stack.push((nx as u32, ny as u32));
                }
            }
            if component.len() >= 3 {
                continue;
            }
            // 碎片：整体并入出现次数最多的外部邻接色
            let mut counts: Vec<(u32, u32)> = Vec::new();
            for (cx, cy) in &component {
                for (dx, dy) in NEIGHBORS {
                    let nx = *cx as i32 + dx;
                    let ny = *cy as i32 + dy;
                    if !in_bounds(nx, ny, w, h) {
                        continue;
                    }
                    let c = cells[idx(nx as u32, ny as u32, w)].color_index;
                    if c == COLOR_NONE || c == color {
                        continue;
                    }
                    if let Some(slot) = counts.iter_mut().find(|(col, _)| *col == c) {
                        slot.1 += 1;
                    } else {
                        counts.push((c, 1));
                    }
                }
            }
            let Some((target, _)) = counts
                .into_iter()
                .max_by_key(|(col, n)| (*n, u32::MAX - *col))
            else {
                continue;
            };
            for (cx, cy) in component {
                let i = idx(cx, cy, w);
                cells[i].color_index = target;
                cells[i].symbol_index = target;
                changed += 1;
            }
        }
    }
    changed
}

/// 稀有色合并：占比低于 `ppt`（千分数）的颜色并入调色板内 Lab 最近的主色。
///
/// 返回重映射表 `remap`（原调色板索引 → 新调色板索引，长度 = palette.len()）。
/// 调色板本身不在此压缩（由调用方依据 remap 重建），保证图例与 cells 同步更新。
pub fn merge_rare_colors(cells: &mut [CmGridCell], palette: &[DmcColor], ppt: u32) -> Vec<usize> {
    let n = palette.len();
    let mut remap: Vec<usize> = (0..n).collect();
    if ppt == 0 || n == 0 || cells.is_empty() {
        return remap;
    }

    let mut counts = vec![0u32; n];
    let mut total = 0u32;
    for c in cells.iter() {
        if c.color_index == COLOR_NONE {
            continue;
        }
        if let Some(slot) = counts.get_mut(c.color_index as usize) {
            *slot += 1;
            total += 1;
        }
    }
    if total == 0 {
        return remap;
    }

    // 阈值：count * 1000 < total * ppt
    let mut order: Vec<usize> = (0..n).collect();
    order.sort_by_key(|&i| (counts[i], i));

    for i in order {
        if counts[i] == 0 {
            continue; // 调色板里已经被完全淘汰的色，稍后由压缩逻辑处理
        }
        if (counts[i] as u64) * 1000 >= (total as u64) * (ppt as u64) {
            break; // 已按计数升序，后面的都不再是稀有色
        }
        // 找最近的主色（计数达标且不是自己）
        let lab = crate::common::util::rgb_to_lab(palette[i].rgb);
        let mut best: Option<(usize, f64)> = None;
        for (j, cand) in palette.iter().enumerate() {
            if j == i || counts[j] == 0 {
                continue;
            }
            if (counts[j] as u64) * 1000 < (total as u64) * (ppt as u64) {
                continue; // 不合并到另一个稀有色
            }
            let d =
                crate::common::util::lab_dist_sq(lab, crate::common::util::rgb_to_lab(cand.rgb));
            if best.is_none_or(|(_, bd)| d < bd) {
                best = Some((j, d));
            }
        }
        if let Some((j, _)) = best {
            remap[i] = j;
            counts[j] += counts[i];
            counts[i] = 0;
        }
    }

    // 迭代闭包：a→b→c 需要展开为 a→c
    for i in 0..n {
        let mut cur = i;
        for _ in 0..n {
            let next = remap[cur];
            if next == cur {
                break;
            }
            cur = next;
        }
        remap[i] = cur;
    }

    if remap.iter().enumerate().any(|(i, &r)| i != r) {
        for c in cells.iter_mut() {
            if c.color_index == COLOR_NONE {
                continue;
            }
            if let Some(&r) = remap.get(c.color_index as usize) {
                let r = r as u32;
                c.color_index = r;
                c.symbol_index = r;
            }
        }
    }
    remap
}

/// 区域填充（油漆桶）：从 (x,y) 出发的 4-连通同色区域替换为 `color`。
/// `color` 为 COLOR_NONE 时表示擦除。返回被修改的格子数。
pub fn flood_fill(cells: &mut [CmGridCell], w: u32, h: u32, x: u32, y: u32, color: u32) -> u32 {
    if w == 0 || h == 0 || x >= w || y >= h || cells.len() != (w * h) as usize {
        return 0;
    }
    let target = cells[idx(x, y, w)].color_index;
    if target == color {
        return 0;
    }

    let mut stack = vec![(x, y)];
    let mut changed = 0u32;
    while let Some((cx, cy)) = stack.pop() {
        let i = idx(cx, cy, w);
        if cells[i].color_index != target {
            continue;
        }
        cells[i].color_index = color;
        cells[i].symbol_index = if color == COLOR_NONE { 0 } else { color };
        changed += 1;
        for (dx, dy) in NEIGHBORS {
            let nx = cx as i32 + dx;
            let ny = cy as i32 + dy;
            if in_bounds(nx, ny, w, h) {
                let ni = idx(nx as u32, ny as u32, w);
                if cells[ni].color_index == target {
                    stack.push((nx as u32, ny as u32));
                }
            }
        }
    }
    changed
}

/// 矩形填充：把闭区间矩形整体设为 `color`。返回被修改的格子数。
///
/// 参数多于 clippy 默认的 7 个上限，但这里是一组语义完整的矩形坐标，
/// 拆成结构体反而增加调用方负担，故保留扁平签名。
#[allow(clippy::too_many_arguments)]
pub fn rect_fill(
    cells: &mut [CmGridCell],
    w: u32,
    h: u32,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    color: u32,
) -> u32 {
    if w == 0 || h == 0 || cells.len() != (w * h) as usize {
        return 0;
    }
    let (xa, xb) = if x0 <= x1 { (x0, x1) } else { (x1, x0) };
    let (ya, yb) = if y0 <= y1 { (y0, y1) } else { (y1, y0) };
    let xb = xb.min(w - 1);
    let yb = yb.min(h - 1);

    let mut changed = 0u32;
    for y in ya..=yb {
        for x in xa..=xb {
            let i = idx(x, y, w);
            if cells[i].color_index == color {
                continue;
            }
            cells[i].color_index = color;
            cells[i].symbol_index = if color == COLOR_NONE { 0 } else { color };
            changed += 1;
        }
    }
    changed
}

/// 全局换色：把所有 `from` 色替换为 `to`。返回被修改的格子数。
pub fn replace_color(cells: &mut [CmGridCell], from: u32, to: u32) -> u32 {
    let mut changed = 0u32;
    for c in cells.iter_mut() {
        if c.color_index != from {
            continue;
        }
        c.color_index = to;
        c.symbol_index = if to == COLOR_NONE { 0 } else { to };
        changed += 1;
    }
    changed
}

/// 单格涂色（绣笔）。`color` 为 COLOR_NONE 时擦除。返回 1 表示有改动。
pub fn paint_cell(cells: &mut [CmGridCell], w: u32, h: u32, x: u32, y: u32, color: u32) -> u32 {
    if w == 0 || h == 0 || x >= w || y >= h || cells.len() != (w * h) as usize {
        return 0;
    }
    let i = idx(x, y, w);
    if cells[i].color_index == color {
        return 0;
    }
    cells[i].color_index = color;
    cells[i].symbol_index = if color == COLOR_NONE { 0 } else { color };
    1
}

/// 统计每种颜色的格数（调色板索引 → 计数）。
pub fn color_histogram(cells: &[CmGridCell], palette_len: usize) -> Vec<u32> {
    let mut counts = vec![0u32; palette_len];
    for c in cells {
        if c.color_index == COLOR_NONE {
            continue;
        }
        if let Some(slot) = counts.get_mut(c.color_index as usize) {
            *slot += 1;
        }
    }
    counts
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::ffi_types::CmGridCell;

    fn grid(w: u32, h: u32, colors: &[u32]) -> Vec<CmGridCell> {
        assert_eq!(colors.len(), (w * h) as usize);
        colors
            .iter()
            .enumerate()
            .map(|(i, &c)| CmGridCell {
                x: (i as u32) % w,
                y: (i as u32) / w,
                color_index: c,
                symbol_index: if c == COLOR_NONE { 0 } else { c },
            })
            .collect()
    }

    /// 4x4 网格：整片 0 色，正中 (1,1) 一根孤立 1 色针
    fn isolated_cells() -> Vec<CmGridCell> {
        let mut v = vec![0u32; 16];
        v[5] = 1; // (1,1)
        grid(4, 4, &v)
    }

    #[test]
    fn cleanup_light_removes_single_stitch() {
        let mut cells = isolated_cells();
        let changed = remove_isolated(&mut cells, 4, 4, cleanup::LIGHT);
        assert_eq!(changed, 1);
        assert_eq!(cells[5].color_index, 0, "孤立针应并入邻域主色");
    }

    #[test]
    fn cleanup_off_does_nothing() {
        let mut cells = isolated_cells();
        assert_eq!(remove_isolated(&mut cells, 4, 4, cleanup::OFF), 0);
        assert_eq!(cells[5].color_index, 1);
    }

    #[test]
    fn cleanup_strong_merges_two_cell_fragment() {
        // 4x4：底色 0，(1,1)(2,1) 为 2 格碎片色 1 → STRONG 应并入 0
        let mut v = vec![0u32; 16];
        v[5] = 1;
        v[6] = 1;
        let mut cells = grid(4, 4, &v);
        assert_eq!(remove_isolated(&mut cells, 4, 4, cleanup::LIGHT), 0);
        let changed = remove_isolated(&mut cells, 4, 4, cleanup::STRONG);
        assert_eq!(changed, 2);
        assert_eq!(cells[5].color_index, 0);
        assert_eq!(cells[6].color_index, 0);
    }

    #[test]
    fn cleanup_preserves_large_regions() {
        // 8x8：左半 0 右半 1，大块不应被清除
        let mut v = vec![0u32; 64];
        for (i, c) in v.iter_mut().enumerate() {
            if i % 8 >= 4 {
                *c = 1;
            }
        }
        let mut cells = grid(8, 8, &v);
        assert_eq!(remove_isolated(&mut cells, 8, 8, cleanup::STRONG), 0);
    }

    #[test]
    fn flood_fill_replaces_connected_region() {
        // 4x4：左上 2x2 为 1，其余为 0 → 从 (0,0) 填 2 影响 4 格
        let mut v = vec![0u32; 16];
        v[0] = 1;
        v[1] = 1;
        v[4] = 1;
        v[5] = 1;
        let mut cells = grid(4, 4, &v);
        assert_eq!(flood_fill(&mut cells, 4, 4, 0, 0, 2), 4);
        assert_eq!(cells[0].color_index, 2);
        assert_eq!(cells[5].color_index, 2);
        assert_eq!(cells[2].color_index, 0, "区域外不受影响");
    }

    #[test]
    fn flood_fill_same_color_is_noop() {
        let mut cells = grid(2, 2, &[1, 1, 1, 1]);
        assert_eq!(flood_fill(&mut cells, 2, 2, 0, 0, 1), 0);
    }

    #[test]
    fn rect_fill_clamps_and_counts() {
        let mut cells = grid(4, 4, &[0u32; 16]);
        assert_eq!(rect_fill(&mut cells, 4, 4, 1, 1, 2, 2, 3), 4);
        assert_eq!(cells[idx(1, 1, 4)].color_index, 3);
        assert_eq!(cells[idx(0, 0, 4)].color_index, 0);
    }

    #[test]
    fn replace_color_global() {
        let mut cells = grid(3, 1, &[1, 2, 1]);
        assert_eq!(replace_color(&mut cells, 1, 2), 2);
        assert!(cells.iter().all(|c| c.color_index == 2));
    }

    #[test]
    fn paint_cell_and_erase() {
        let mut cells = grid(2, 2, &[0, 0, 0, 0]);
        assert_eq!(paint_cell(&mut cells, 2, 2, 1, 1, 5), 1);
        assert_eq!(
            paint_cell(&mut cells, 2, 2, 1, 1, 5),
            0,
            "重复涂同色不算改动"
        );
        assert_eq!(paint_cell(&mut cells, 2, 2, 1, 1, COLOR_NONE), 1);
        assert_eq!(cells[3].color_index, COLOR_NONE);
        assert_eq!(paint_cell(&mut cells, 2, 2, 9, 9, 1), 0, "越界安全");
    }

    #[test]
    fn merge_rare_colors_merges_below_threshold() {
        // 100 格：色 0 占 97 格，色 1 占 3 格 → 阈值 50‰ 时 1 应并入 0
        let mut v = vec![0u32; 100];
        for c in v.iter_mut().take(3) {
            *c = 1;
        }
        let mut cells = grid(10, 10, &v);
        let palette = vec![
            DmcColor {
                code: "A",
                name: "a",
                rgb: [10, 10, 10],
            },
            DmcColor {
                code: "B",
                name: "b",
                rgb: [240, 240, 240],
            },
        ];
        let remap = merge_rare_colors(&mut cells, &palette, 50);
        assert_eq!(remap[1], 0);
        assert!(cells.iter().all(|c| c.color_index == 0));
    }

    #[test]
    fn merge_rare_colors_disabled_at_zero() {
        let mut v = vec![0u32; 100];
        v[0] = 1;
        let mut cells = grid(10, 10, &v);
        let palette = vec![
            DmcColor {
                code: "A",
                name: "a",
                rgb: [10, 10, 10],
            },
            DmcColor {
                code: "B",
                name: "b",
                rgb: [240, 240, 240],
            },
        ];
        let remap = merge_rare_colors(&mut cells, &palette, 0);
        assert_eq!(remap, vec![0, 1]);
    }

    #[test]
    fn histogram_counts_colors() {
        let cells = grid(4, 1, &[0, 1, 0, COLOR_NONE]);
        assert_eq!(color_histogram(&cells, 2), vec![2, 1]);
    }
}
