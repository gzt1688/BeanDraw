//! 网格生成：符号分配、图例与用量统计、成品尺寸换算。
//!
//! 符号分配策略（参考 Cstitch `symbolChooser` 与 KXStitch `Scheme`）：
//! **按用量降序分配最清晰的符号**——用得最多的颜色拿到符号集里最好认的字符，
//! 这样绣制时最常查的符号最容易辨认，同时保证一色一符号、全图一致。

use super::CrossStitchChart;
use crate::common::ffi_types::{CmFabricSize, CmGridCell, CmLegendEntry, COLOR_NONE};
use crate::common::util;

/// 为调色板分配符号（按调色板顺序，用于无用量信息时的兜底）。
pub fn assign_symbols(palette_len: usize, symbol_set: u32) -> Vec<char> {
    (0..palette_len)
        .map(|i| util::symbol_in_set(symbol_set, i))
        .collect()
}

/// 按用量降序分配符号：针数最多的颜色获得符号集中最靠前（最易辨认）的字符。
///
/// `counts` 长度应与调色板一致；长度不符时退化为按调色板顺序分配。
/// 相同针数按调色板索引升序，保证结果确定。
pub fn assign_symbols_by_usage(counts: &[u32], symbol_set: u32) -> Vec<char> {
    let n = counts.len();
    let mut order: Vec<usize> = (0..n).collect();
    order.sort_by(|&a, &b| counts[b].cmp(&counts[a]).then_with(|| a.cmp(&b)));

    let mut symbols = vec!['?'; n];
    for (rank, &color_index) in order.iter().enumerate() {
        symbols[color_index] = util::symbol_in_set(symbol_set, rank);
    }
    symbols
}

/// 换符号集：保持「用量 → 符号清晰度」的映射关系不变。
pub fn reassign_symbol_set(chart: &mut CrossStitchChart, symbol_set: u32) -> bool {
    if chart.options.symbol_set == symbol_set {
        return false;
    }
    chart.options.symbol_set = symbol_set;
    let counts =
        crate::cross_stitch::post_process::color_histogram(&chart.cells, chart.palette.len());
    chart.symbols = assign_symbols_by_usage(&counts, symbol_set);
    true
}

/// 图例与用量统计。
pub fn legend(chart: &CrossStitchChart) -> Vec<CmLegendEntry> {
    let mut entries: Vec<CmLegendEntry> = chart
        .palette
        .iter()
        .enumerate()
        .map(|(i, _)| CmLegendEntry {
            color_index: i as u32,
            symbol_index: i as u32,
            stitch_count: 0,
            bead_count: 0,
            length_cm_x100: 0,
        })
        .collect();

    let bead_mode = chart.params.bead_mode != 0;
    for cell in &chart.cells {
        if cell.color_index == COLOR_NONE {
            continue;
        }
        if let Some(e) = entries.get_mut(cell.color_index as usize) {
            e.stitch_count += 1;
            if bead_mode {
                e.bead_count += 1;
            }
        }
    }
    for e in &mut entries {
        e.length_cm_x100 = length_cm_x100(e.stitch_count, chart.params.fabric_count);
    }
    entries
}

/// 总针数。
pub fn total_stitches(chart: &CrossStitchChart) -> u32 {
    chart
        .cells
        .iter()
        .filter(|c| c.color_index != COLOR_NONE)
        .count() as u32
}

/// 用线长度估算（厘米 × 100）。
///
/// 经验公式：14ct Aida 每针约 1.2cm（含两根对角线段、背面走线与损耗），
/// 按面料计数线性缩放——计数越大单格越小，用线越省（开发估算值，非工艺标准）。
pub fn length_cm_x100(stitches: u32, fabric_count: u32) -> u32 {
    let count = fabric_count.clamp(6, 32) as f64;
    let cm = stitches as f64 * 1.2 * (14.0 / count);
    (cm * 100.0).round() as u32
}

/// 成品尺寸换算：网格 + 面料计数 → 毫米。
///
/// Aida `ct` 表示每英寸的格数，故单格边长 = 25.4mm / ct。
pub fn fabric_size(width: u32, height: u32, fabric_count: u32) -> CmFabricSize {
    let count = fabric_count.clamp(6, 32) as f64;
    let cell_mm = 25.4 / count;
    CmFabricSize {
        width_mm: (width as f64 * cell_mm).round() as u32,
        height_mm: (height as f64 * cell_mm).round() as u32,
        cell_mm_x100: (cell_mm * 100.0).round() as u32,
    }
}

/// 由目标成品尺寸反推网格数（属性面板「按尺寸生成」用）。
pub fn grid_for_size(width_mm: u32, height_mm: u32, fabric_count: u32) -> (u32, u32) {
    let count = fabric_count.clamp(6, 32) as f64;
    let cell_mm = 25.4 / count;
    let w = (width_mm as f64 / cell_mm).round().max(1.0) as u32;
    let h = (height_mm as f64 / cell_mm).round().max(1.0) as u32;
    (w, h)
}

/// 从量化结果生成网格单元（符号索引与调色板索引一一对应，便于图例同步）。
pub fn build_cells(grid_w: u32, assignments: &[usize], palette_len: usize) -> Vec<CmGridCell> {
    let mut cells = Vec::with_capacity(assignments.len());
    for (i, &a) in assignments.iter().enumerate() {
        let (x, y) = ((i as u32) % grid_w, (i as u32) / grid_w);
        if a == usize::MAX || a >= palette_len {
            cells.push(CmGridCell {
                x,
                y,
                color_index: COLOR_NONE,
                symbol_index: 0,
            });
        } else {
            cells.push(CmGridCell {
                x,
                y,
                color_index: a as u32,
                symbol_index: a as u32,
            });
        }
    }
    cells
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::ffi_types::CmCrossStitchParams;

    fn make_chart(cells: Vec<CmGridCell>, palette_len: usize) -> CrossStitchChart {
        let mut chart = CrossStitchChart::empty(4, 4, CmCrossStitchParams::default());
        chart.cells = cells;
        // 用假调色板填充
        while chart.palette.len() < palette_len {
            chart.palette.push(super::super::DmcColor {
                code: "310",
                name: "Black",
                rgb: [0, 0, 0],
            });
        }
        chart
    }

    #[test]
    fn legend_counts_stitches() {
        let cells = vec![
            CmGridCell {
                x: 0,
                y: 0,
                color_index: 0,
                symbol_index: 0,
            },
            CmGridCell {
                x: 1,
                y: 0,
                color_index: 0,
                symbol_index: 0,
            },
            CmGridCell {
                x: 2,
                y: 0,
                color_index: 1,
                symbol_index: 1,
            },
            CmGridCell {
                x: 3,
                y: 0,
                color_index: COLOR_NONE,
                symbol_index: 0,
            },
        ];
        let chart = make_chart(cells, 2);
        let legend = legend(&chart);
        assert_eq!(legend.len(), 2);
        assert_eq!(legend[0].stitch_count, 2);
        assert_eq!(legend[1].stitch_count, 1);
        assert_eq!(legend[0].bead_count, 0); // 默认非珠绣
        assert_eq!(total_stitches(&chart), 3);
        assert!(legend[0].length_cm_x100 > 0);
    }

    #[test]
    fn bead_mode_counts_beads() {
        let mut cells = vec![
            CmGridCell {
                x: 0,
                y: 0,
                color_index: 0,
                symbol_index: 0
            };
            4
        ];
        cells[3].color_index = COLOR_NONE;
        let mut chart = make_chart(cells, 1);
        chart.params.bead_mode = 1;
        let legend = legend(&chart);
        assert_eq!(legend[0].stitch_count, 3);
        assert_eq!(legend[0].bead_count, 3);
    }

    #[test]
    fn build_cells_marks_empty() {
        let cells = build_cells(2, &[0, usize::MAX, 1, 0], 2);
        assert_eq!(cells.len(), 4);
        assert_eq!(cells[1].color_index, COLOR_NONE);
        assert_eq!(cells[0].symbol_index, 0);
        assert_eq!(cells[2].symbol_index, 1);
    }

    #[test]
    fn symbol_sequence_covers_palette() {
        let symbols = assign_symbols(10, crate::common::ffi_types::symbol::LETTERS);
        assert_eq!(symbols.len(), 10);
        assert_eq!(symbols[0], 'A');
        assert_eq!(symbols[9], 'J');
    }

    #[test]
    fn symbols_follow_usage_order() {
        // 色 1 用量最大 → 应拿到符号集第一个字符
        let counts = [5u32, 100, 1];
        let sym = assign_symbols_by_usage(&counts, crate::common::ffi_types::symbol::LETTERS);
        assert_eq!(sym[1], 'A');
        assert_eq!(sym[0], 'B');
        assert_eq!(sym[2], 'C');
    }

    #[test]
    fn symbol_set_switch_keeps_usage_order() {
        // 数字集与字母集切换后，用量最大的色仍应拿到各自集合的首字符
        let counts = [1u32, 9];
        let letters = assign_symbols_by_usage(&counts, crate::common::ffi_types::symbol::LETTERS);
        let digits = assign_symbols_by_usage(&counts, crate::common::ffi_types::symbol::DIGITS);
        assert_eq!(letters[1], 'A');
        assert_eq!(digits[1], '1');
    }

    #[test]
    fn symbols_wrap_around_on_overflow() {
        // 几何集只有十几个字符，分配 20 个色应回绕且不 panic
        let set = crate::common::ffi_types::symbol::GEOMETRIC;
        let len = util::symbol_set_len(set);
        assert!(len < 20);
        let sym = assign_symbols(20, set);
        assert_eq!(sym.len(), 20);
        assert_eq!(sym[0], sym[len], "超出符号集容量应回绕");
    }

    #[test]
    fn fabric_size_matches_aida_spacing() {
        // 14ct：单格 = 25.4/14 ≈ 1.814mm → 100 格 ≈ 181mm
        let s = fabric_size(100, 50, 14);
        assert_eq!(s.cell_mm_x100, 181);
        assert_eq!(s.width_mm, 181);
        assert_eq!(s.height_mm, 91);
        // 18ct 布更密 → 同格数成品更小
        assert!(fabric_size(100, 100, 18).width_mm < s.width_mm);
    }

    #[test]
    fn grid_for_size_roundtrips() {
        let (w, h) = grid_for_size(181, 91, 14);
        assert_eq!(w, 100);
        assert_eq!(h, 50);
        // 极小尺寸也至少 1 格
        assert_eq!(grid_for_size(0, 0, 14), (1, 1));
    }
}
