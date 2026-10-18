//! Rust 侧集成冒烟测试：验证公开 API（供未来插件 / 工具复用）。

use craftmaster_core::command::History;
use craftmaster_core::common::ffi_types::{CmCrossStitchParams, CmStitchOptions, COLOR_NONE};
use craftmaster_core::common::progress::Progress;
use craftmaster_core::cross_stitch::{generate_chart, CrossStitchChart};
use craftmaster_core::export::{dxf, pdf, project};
use craftmaster_core::material_lib::MaterialDb;
use craftmaster_core::pattern::geometry;
use craftmaster_core::plugin;

fn test_image(w: u32, h: u32, f: impl Fn(u32, u32) -> [u8; 4]) -> Vec<u8> {
    let mut v = Vec::new();
    for y in 0..h {
        for x in 0..w {
            v.extend_from_slice(&f(x, y));
        }
    }
    v
}

#[test]
fn end_to_end_cross_stitch_pipeline() {
    let rgba = test_image(32, 32, |x, y| {
        if (x / 8 + y / 8) % 2 == 0 {
            [200, 30, 30, 255]
        } else {
            [30, 30, 200, 255]
        }
    });
    let params = CmCrossStitchParams {
        grid_width: 16,
        grid_height: 16,
        max_colors: 8,
        ..Default::default()
    };
    let mut progress = Progress::noop();
    let chart: CrossStitchChart = generate_chart(
        &rgba,
        32,
        32,
        params,
        CmStitchOptions::default(),
        &mut progress,
    )
    .expect("generate");

    assert_eq!(chart.cells.len(), 256);
    assert!(chart.palette.len() >= 2);
    assert_eq!(chart.total_stitches(), 256);
    assert!(chart.cells.iter().all(|c| c.color_index != COLOR_NONE));

    // 导出矩阵
    let dir = std::env::temp_dir();
    let csv = dir.join("cm_smoke.csv");
    let png = dir.join("cm_smoke.png");
    let pdf_path = dir.join("cm_smoke.pdf");
    let bean = dir.join("cm_smoke.bean");

    craftmaster_core::cross_stitch::export::export_csv(&chart, &csv).unwrap();
    craftmaster_core::cross_stitch::export::export_png(&chart, &png, 4).unwrap();
    pdf::export_chart_pdf(&chart, &pdf_path, "Smoke").unwrap();
    project::save_project(&chart, &bean).unwrap();

    assert!(std::fs::metadata(&csv).unwrap().len() > 20);
    assert!(std::fs::metadata(&png).unwrap().len() > 100);
    assert!(std::fs::read(&pdf_path).unwrap().starts_with(b"%PDF"));

    let reopened = project::open_project(&bean).unwrap();
    assert_eq!(reopened.total_stitches(), chart.total_stitches());

    for p in [&csv, &png, &pdf_path, &bean] {
        std::fs::remove_file(p).ok();
    }
}

#[test]
fn pattern_and_history_public_api() {
    let (outline, seam) = geometry::seam_rect(120.0, 60.0, 8.0).unwrap();
    assert_eq!(outline.len(), 4);
    assert_eq!(seam.len(), 4);

    let dir = std::env::temp_dir();
    let dxf_path = dir.join("cm_smoke.dxf");
    dxf::export_piece_paths(&outline, &seam, &dxf_path).unwrap();
    assert!(std::fs::read_to_string(&dxf_path)
        .unwrap()
        .contains("OUTLINE"));
    std::fs::remove_file(&dxf_path).ok();

    let mut history = History::new(10);
    history.init(vec![1]);
    history.push("edit", vec![2]);
    assert_eq!(history.undo(), Some(&[1][..]));
    assert_eq!(history.redo(), Some(&[2][..]));
}

#[test]
fn plugin_registry_and_material_db() {
    let metas = plugin::module_metas();
    assert_eq!(metas.len(), 2);
    assert!(metas.iter().any(|m| m.id == "cross_stitch"));

    let db = MaterialDb::open_in_memory().unwrap();
    let dmc = db.find_by_code("DMC-NOPE").unwrap();
    assert!(dmc.is_none());
    let white = db.find_by_code("B5200").unwrap().expect("B5200 seeded");
    assert_eq!(white.rgb, Some([255, 255, 255]));
}
