//! 十字绣导出：材料清单 CSV（Excel 兼容）+ PNG 预览图。

use super::CrossStitchChart;
use crate::common::error::{CoreError, CoreResult};
use crate::common::ffi_types::COLOR_NONE;
use crate::export::csv::open_material_csv;
use image::{Rgba, RgbaImage};
use std::io::Write;
use std::path::Path;

/// 材料清单 CSV：Symbol, DMC Code, Name, Stitches, Beads, Length(cm)。
/// 带 UTF-8 BOM，Excel 双击打开不乱码（开发大纲第 16 章）。
pub fn export_csv(chart: &CrossStitchChart, path: &Path) -> CoreResult<()> {
    let mut w = open_material_csv(path)?;
    writeln!(w, "Symbol,DMC Code,Name,Stitches,Beads,Length(cm)")?;
    for (i, e) in chart.legend().iter().enumerate() {
        let color = chart
            .palette
            .get(i)
            .ok_or_else(|| CoreError::Internal("legend/palette index mismatch".into()))?;
        let symbol = chart.symbols.get(i).copied().unwrap_or('?');
        writeln!(
            w,
            "{},{},{},{},{},{:.2}",
            symbol,
            color.code,
            color.name,
            e.stitch_count,
            e.bead_count,
            e.length_cm_x100 as f64 / 100.0
        )?;
    }
    let total = chart.total_stitches();
    writeln!(w, "TOTAL,,,{total},,")?;
    w.flush()?;
    Ok(())
}

/// PNG 预览：按 cell_px 像素/格 渲染网格与符号色块（返回 PNG 字节）。
pub fn render_png(chart: &CrossStitchChart, cell_px: u32) -> CoreResult<Vec<u8>> {
    if cell_px == 0 || cell_px > 64 {
        return Err(CoreError::InvalidArg(format!(
            "cell_px {cell_px} out of range 1..=64"
        )));
    }
    let w = chart.width * cell_px;
    let h = chart.height * cell_px;
    let mut img = RgbaImage::new(w, h);

    // 面料底色
    for px in img.pixels_mut() {
        *px = Rgba([232, 222, 198, 255]);
    }

    // 色块
    for cell in &chart.cells {
        if cell.color_index == COLOR_NONE {
            continue;
        }
        let Some(color) = chart.palette.get(cell.color_index as usize) else {
            continue;
        };
        let x0 = cell.x * cell_px;
        let y0 = cell.y * cell_px;
        for dy in 0..cell_px {
            for dx in 0..cell_px {
                img.put_pixel(
                    x0 + dx,
                    y0 + dy,
                    Rgba([color.rgb[0], color.rgb[1], color.rgb[2], 255]),
                );
            }
        }
    }

    // 网格线（每格细线，每 10 格粗线）
    let line = |img: &mut RgbaImage, dark: bool| {
        let c = if dark {
            Rgba([90, 90, 90, 255])
        } else {
            Rgba([170, 170, 170, 255])
        };
        for gx in 0..=chart.width {
            let x = gx * cell_px;
            if x >= w {
                continue;
            }
            for y in 0..h {
                img.put_pixel(x, y, c);
            }
        }
        for gy in 0..=chart.height {
            let y = gy * cell_px;
            if y >= h {
                continue;
            }
            for x in 0..w {
                img.put_pixel(x, y, c);
            }
        }
    };
    line(&mut img, false);
    // 每 10 格加粗（画 2px）
    for gx in (0..=chart.width).step_by(10) {
        let x = (gx * cell_px).min(w.saturating_sub(1));
        for y in 0..h {
            img.put_pixel(x, y, Rgba([60, 60, 60, 255]));
        }
    }
    for gy in (0..=chart.height).step_by(10) {
        let y = (gy * cell_px).min(h.saturating_sub(1));
        for x in 0..w {
            img.put_pixel(x, y, Rgba([60, 60, 60, 255]));
        }
    }

    let mut buf = Vec::new();
    img.write_to(&mut std::io::Cursor::new(&mut buf), image::ImageFormat::Png)?;
    Ok(buf)
}

/// PNG 预览：写入文件。
pub fn export_png(chart: &CrossStitchChart, path: &Path, cell_px: u32) -> CoreResult<()> {
    let bytes = render_png(chart, cell_px)?;
    std::fs::write(path, bytes)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::ffi_types::{CmCrossStitchParams, CmGridCell};

    fn sample_chart() -> CrossStitchChart {
        let mut chart = CrossStitchChart::empty(4, 4, CmCrossStitchParams::default());
        chart.palette = vec![
            crate::cross_stitch::DmcColor {
                code: "321",
                name: "Red",
                rgb: [199, 15, 46],
            },
            crate::cross_stitch::DmcColor {
                code: "336",
                name: "Navy Blue",
                rgb: [37, 59, 113],
            },
        ];
        chart.symbols = vec!['A', 'B'];
        chart.cells = (0..16)
            .map(|i| CmGridCell {
                x: i % 4,
                y: i / 4,
                color_index: if i % 2 == 0 { 0 } else { 1 },
                symbol_index: if i % 2 == 0 { 0 } else { 1 },
            })
            .collect();
        chart
    }

    #[test]
    fn csv_export_writes_bom_and_rows() {
        let dir = std::env::temp_dir();
        let path = dir.join("craftmaster_test_material.csv");
        let chart = sample_chart();
        export_csv(&chart, &path).unwrap();
        let bytes = std::fs::read(&path).unwrap();
        assert!(bytes.starts_with(&[0xEF, 0xBB, 0xBF]));
        let text = String::from_utf8(bytes[3..].to_vec()).unwrap();
        assert!(text.contains("Symbol,DMC Code,Name"));
        assert!(text.contains("321"));
        assert!(text.contains("TOTAL,,,16"));
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn png_export_creates_image() {
        let dir = std::env::temp_dir();
        let path = dir.join("craftmaster_test_preview.png");
        let chart = sample_chart();
        export_png(&chart, &path, 8).unwrap();
        let img = image::open(&path).unwrap();
        assert_eq!(img.width(), 32);
        assert_eq!(img.height(), 32);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn png_rejects_bad_cell_px() {
        let chart = sample_chart();
        let dir = std::env::temp_dir();
        assert!(export_png(&chart, &dir.join("x.png"), 0).is_err());
    }
}
