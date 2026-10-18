//! PDF 导出：十字绣图纸（网格 + 符号 + 材料清单页）。
//!
//! 本地化边界（第 7 章）：title / 文案由 UI 层翻译后传入，Rust 只排版。
//! Phase 1 使用内置 Helvetica（WinAnsi，ASCII 符号安全）；中文标注待 TTF 嵌入（Phase 3）。

use crate::common::error::{CoreError, CoreResult};
use crate::common::ffi_types::COLOR_NONE;
use crate::cross_stitch::CrossStitchChart;
use printpdf::path::{PaintMode, WindingOrder};
use printpdf::{
    BuiltinFont, Color, Line, Mm, PdfDocument, PdfDocumentReference, PdfLayerIndex, PdfPageIndex,
    Point, Polygon, Rgb,
};
use std::io::BufWriter;
use std::path::Path;

const PAGE_W: f64 = 210.0;
const PAGE_H: f64 = 297.0;
const MARGIN: f64 = 15.0;

/// f64 毫米 → printpdf::Mm（内部 f32）。
fn mm(v: f64) -> Mm {
    Mm(v as f32)
}

fn rgb(r: f64, g: f64, b: f64) -> Rgb {
    Rgb::new(r as f32, g as f32, b as f32, None)
}

/// 导出十字绣图纸 PDF（A4，第 1 页图纸 + 第 2 页材料清单）。
pub fn export_chart_pdf(chart: &CrossStitchChart, path: &Path, title: &str) -> CoreResult<()> {
    if chart.width == 0 || chart.height == 0 {
        return Err(CoreError::InvalidArg("empty chart".into()));
    }
    let (doc, page1, layer1) = PdfDocument::new(
        if title.is_empty() {
            "BeanDraw Chart"
        } else {
            title
        },
        mm(PAGE_W),
        mm(PAGE_H),
        "chart",
    );
    let font = doc.add_builtin_font(BuiltinFont::Helvetica)?;
    let font_bold = doc.add_builtin_font(BuiltinFont::HelveticaBold)?;

    draw_chart_page(&doc, page1, layer1, chart, title, &font, &font_bold);

    let (page2, layer2) = doc.add_page(mm(PAGE_W), mm(PAGE_H), "chart_page_2");
    draw_legend_page(&doc, page2, layer2, chart, &font, &font_bold);

    let file = std::fs::File::create(path)?;
    let mut writer = BufWriter::new(file);
    doc.save(&mut writer)?;
    Ok(())
}

fn draw_chart_page(
    doc: &PdfDocumentReference,
    page: PdfPageIndex,
    layer_idx: PdfLayerIndex,
    chart: &CrossStitchChart,
    title: &str,
    font: &printpdf::IndirectFontRef,
    font_bold: &printpdf::IndirectFontRef,
) {
    let layer = doc.get_page(page).get_layer(layer_idx);

    if !title.is_empty() {
        layer.use_text(
            title,
            14.0,
            mm(MARGIN),
            mm(PAGE_H - MARGIN - 6.0),
            font_bold,
        );
    }

    // 网格区（A4 可用区，扣除标题）
    let avail_w = PAGE_W - 2.0 * MARGIN;
    let avail_h = PAGE_H - 2.0 * MARGIN - 18.0;
    let cell = (avail_w / chart.width as f64)
        .min(avail_h / chart.height as f64)
        .clamp(0.4, 6.0);
    let grid_w = cell * chart.width as f64;
    let grid_h = cell * chart.height as f64;
    let origin_x = MARGIN + (avail_w - grid_w) / 2.0;
    let origin_y = MARGIN + 8.0; // PDF y 向上，自底部布局

    // 色块（PDF 坐标 y 向上，网格行序自上而下 → 翻转）
    for c in &chart.cells {
        if c.color_index == COLOR_NONE {
            continue;
        }
        let Some(color) = chart.palette.get(c.color_index as usize) else {
            continue;
        };
        let cx = origin_x + c.x as f64 * cell;
        let cy = origin_y + (chart.height - 1 - c.y) as f64 * cell;

        layer.set_fill_color(Color::Rgb(rgb(
            color.rgb[0] as f64 / 255.0,
            color.rgb[1] as f64 / 255.0,
            color.rgb[2] as f64 / 255.0,
        )));
        add_filled_rect(&layer, cx, cy, cell, cell);

        // 符号（按亮度选黑 / 白字）
        let luminance =
            0.299 * color.rgb[0] as f64 + 0.587 * color.rgb[1] as f64 + 0.114 * color.rgb[2] as f64;
        layer.set_fill_color(Color::Rgb(if luminance < 128.0 {
            rgb(1.0, 1.0, 1.0)
        } else {
            rgb(0.0, 0.0, 0.0)
        }));
        let symbol = chart
            .symbols
            .get(c.symbol_index as usize)
            .copied()
            .unwrap_or('?');
        layer.use_text(
            symbol.to_string(),
            (cell * 0.62 * 72.0 / 25.4) as f32, // mm → pt
            mm(cx + cell * 0.28),
            mm(cy + cell * 0.26),
            font,
        );
    }

    // 网格线
    layer.set_outline_color(Color::Rgb(rgb(0.55, 0.55, 0.55)));
    layer.set_outline_thickness(0.15);
    for gx in 0..=chart.width {
        let x = origin_x + gx as f64 * cell;
        add_line(&layer, x, origin_y, x, origin_y + grid_h);
    }
    for gy in 0..=chart.height {
        let y = origin_y + gy as f64 * cell;
        add_line(&layer, origin_x, y, origin_x + grid_w, y);
    }
    // 每 10 格粗线
    layer.set_outline_color(Color::Rgb(rgb(0.0, 0.0, 0.0)));
    layer.set_outline_thickness(0.5);
    for gx in (0..=chart.width).step_by(10) {
        let x = origin_x + gx as f64 * cell;
        add_line(&layer, x, origin_y, x, origin_y + grid_h);
    }
    for gy in (0..=chart.height).step_by(10) {
        let y = origin_y + gy as f64 * cell;
        add_line(&layer, origin_x, y, origin_x + grid_w, y);
    }
}

fn draw_legend_page(
    doc: &PdfDocumentReference,
    page: PdfPageIndex,
    layer_idx: PdfLayerIndex,
    chart: &CrossStitchChart,
    font: &printpdf::IndirectFontRef,
    font_bold: &printpdf::IndirectFontRef,
) {
    let layer = doc.get_page(page).get_layer(layer_idx);
    layer.use_text(
        "Material List",
        14.0,
        mm(MARGIN),
        mm(PAGE_H - MARGIN - 6.0),
        font_bold,
    );

    let cols = [
        ("Symbol", MARGIN),
        ("DMC", MARGIN + 22.0),
        ("Name", MARGIN + 48.0),
        ("Stitches", MARGIN + 108.0),
        ("Beads", MARGIN + 138.0),
        ("Length(cm)", MARGIN + 158.0),
    ];
    let mut y = PAGE_H - MARGIN - 20.0;
    layer.set_fill_color(Color::Rgb(rgb(0.0, 0.0, 0.0)));
    for (text, x) in cols {
        layer.use_text(text, 9.0, mm(x), mm(y), font_bold);
    }
    y -= 6.0;
    add_line(&layer, MARGIN, y + 4.0, PAGE_W - MARGIN, y + 4.0);

    for (i, e) in chart.legend().iter().enumerate() {
        if y < MARGIN + 10.0 {
            break; // 单页放不下的极端情况：Phase 3 加分页
        }
        let color = &chart.palette[i];
        let symbol = chart.symbols.get(i).copied().unwrap_or('?');
        let row = [
            symbol.to_string(),
            color.code.to_string(),
            color.name.to_string(),
            e.stitch_count.to_string(),
            e.bead_count.to_string(),
            format!("{:.2}", e.length_cm_x100 as f64 / 100.0),
        ];
        layer.set_fill_color(Color::Rgb(rgb(0.0, 0.0, 0.0)));
        for (text, (_, x)) in row.iter().zip(cols.iter()) {
            layer.use_text(text, 8.5, mm(*x), mm(y), font);
        }
        y -= 5.5;
    }
}

fn add_line(layer: &printpdf::PdfLayerReference, x1: f64, y1: f64, x2: f64, y2: f64) {
    layer.add_line(Line {
        points: vec![
            (Point::new(mm(x1), mm(y1)), false),
            (Point::new(mm(x2), mm(y2)), false),
        ],
        is_closed: false,
    });
}

fn add_filled_rect(layer: &printpdf::PdfLayerReference, x: f64, y: f64, w: f64, h: f64) {
    layer.add_polygon(Polygon {
        rings: vec![vec![
            (Point::new(mm(x), mm(y)), false),
            (Point::new(mm(x + w), mm(y)), false),
            (Point::new(mm(x + w), mm(y + h)), false),
            (Point::new(mm(x), mm(y + h)), false),
        ]],
        mode: PaintMode::Fill,
        winding_order: WindingOrder::EvenOdd,
    });
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::ffi_types::{CmCrossStitchParams, CmGridCell};
    use crate::cross_stitch::DmcColor;

    fn sample_chart() -> CrossStitchChart {
        let mut chart = CrossStitchChart::empty(8, 8, CmCrossStitchParams::default());
        chart.palette = vec![
            DmcColor {
                code: "321",
                name: "Red",
                rgb: [199, 15, 46],
            },
            DmcColor {
                code: "310",
                name: "Black",
                rgb: [0, 0, 0],
            },
        ];
        chart.symbols = vec!['A', 'B'];
        chart.cells = (0..64)
            .map(|i| CmGridCell {
                x: i % 8,
                y: i / 8,
                color_index: if i % 2 == 0 { 0 } else { 1 },
                symbol_index: if i % 2 == 0 { 0 } else { 1 },
            })
            .collect();
        chart
    }

    #[test]
    fn pdf_export_creates_file() {
        let dir = std::env::temp_dir();
        let path = dir.join("craftmaster_test_chart.pdf");
        let chart = sample_chart();
        export_chart_pdf(&chart, &path, "Test Chart").unwrap();
        let bytes = std::fs::read(&path).unwrap();
        assert!(bytes.starts_with(b"%PDF"));
        assert!(bytes.len() > 1000);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn pdf_rejects_empty_chart() {
        let chart = CrossStitchChart::empty(0, 0, CmCrossStitchParams::default());
        let dir = std::env::temp_dir();
        assert!(export_chart_pdf(&chart, &dir.join("empty.pdf"), "").is_err());
    }
}
