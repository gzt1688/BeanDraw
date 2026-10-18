//! DXF 导出：内置最小 R12 writer（ASCII LINE 实体 + LAYER 表）。
//!
//! 选型说明：未采用 dxf-rs（成熟度风险，开发大纲第 10 章风险 12）。
//! DXF LINE 实体格式简单、激光切割 / 印花设备普遍支持，自研可控。
//! 切割线 / 折痕线 / 标注 / 斩孔分图层导出（第 16 章）。

use crate::common::error::{CoreError, CoreResult};
use crate::common::ffi_types::CmPoint;
use std::path::Path;

/// 导出多个图层（layer_name, points）。闭合多边形自动首尾相连。
pub fn export_paths_dxf(layers: &[(String, Vec<CmPoint>)], path: &Path) -> CoreResult<()> {
    let mut s = String::with_capacity(1024);

    // HEADER
    s.push_str("0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n0\nENDSEC\n");

    // TABLES / LAYER
    s.push_str("0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n");
    s.push_str(&layers.len().to_string());
    s.push('\n');
    for (i, (name, _)) in layers.iter().enumerate() {
        s.push_str("0\nLAYER\n2\n");
        s.push_str(&sanitize_layer(name));
        s.push_str("\n70\n0\n62\n");
        s.push_str(&(7 + i).to_string());
        s.push_str("\n6\nCONTINUOUS\n");
    }
    s.push_str("0\nENDTAB\n0\nENDSEC\n");

    // ENTITIES
    s.push_str("0\nSECTION\n2\nENTITIES\n");
    for (name, pts) in layers {
        if pts.len() < 2 {
            continue;
        }
        let layer = sanitize_layer(name);
        let segments = pts.len(); // 闭合
        for i in 0..segments {
            let a = pts[i];
            let b = pts[(i + 1) % pts.len()];
            s.push_str("0\nLINE\n8\n");
            s.push_str(&layer);
            s.push_str("\n10\n");
            s.push_str(&format_coord(a.x));
            s.push_str("\n20\n");
            s.push_str(&format_coord(a.y));
            s.push_str("\n30\n0.0\n11\n");
            s.push_str(&format_coord(b.x));
            s.push_str("\n21\n");
            s.push_str(&format_coord(b.y));
            s.push_str("\n31\n0.0\n");
        }
    }
    s.push_str("0\nENDSEC\n0\nEOF\n");

    std::fs::write(path, s)?;
    Ok(())
}

/// DXF 层名安全化（去空格与特殊字符）。
fn sanitize_layer(name: &str) -> String {
    let cleaned: String = name
        .chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() || c == '_' || c == '-' {
                c
            } else {
                '_'
            }
        })
        .collect();
    if cleaned.is_empty() {
        "LAYER0".to_string()
    } else {
        cleaned
    }
}

fn format_coord(v: f64) -> String {
    if !v.is_finite() {
        return "0.0".to_string();
    }
    format!("{v:.4}")
}

/// 裁片路径 → 标准双层导出（OUTLINE / SEAM）。
pub fn export_piece_paths(outline: &[CmPoint], seam: &[CmPoint], path: &Path) -> CoreResult<()> {
    if outline.len() < 3 {
        return Err(CoreError::GeometryInvalid(
            "outline needs at least 3 points".into(),
        ));
    }
    let mut layers: Vec<(String, Vec<CmPoint>)> = vec![("OUTLINE".to_string(), outline.to_vec())];
    if seam.len() >= 3 {
        layers.push(("SEAM".to_string(), seam.to_vec()));
    }
    export_paths_dxf(&layers, path)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dxf_contains_layers_and_lines() {
        let dir = std::env::temp_dir();
        let path = dir.join("craftmaster_test.dxf");
        let outline = vec![
            CmPoint { x: 0.0, y: 0.0 },
            CmPoint { x: 100.0, y: 0.0 },
            CmPoint { x: 100.0, y: 50.0 },
        ];
        let seam = vec![
            CmPoint { x: -5.0, y: -5.0 },
            CmPoint { x: 105.0, y: -5.0 },
            CmPoint { x: 105.0, y: 55.0 },
        ];
        export_piece_paths(&outline, &seam, &path).unwrap();
        let text = std::fs::read_to_string(&path).unwrap();
        assert!(text.contains("$ACADVER"));
        assert!(text.contains("LAYER\n2\nOUTLINE"));
        assert!(text.contains("LAYER\n2\nSEAM"));
        assert!(text.contains("0\nLINE\n8\nOUTLINE"));
        assert!(text.trim_end().ends_with("EOF"));
        // 3 点闭合 = 3 条线 × 2 层
        assert_eq!(text.matches("0\nLINE").count(), 6);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn dxf_rejects_degenerate_outline() {
        let dir = std::env::temp_dir();
        let path = dir.join("craftmaster_test_bad.dxf");
        assert!(export_piece_paths(&[], &[], &path).is_err());
    }

    #[test]
    fn layer_name_sanitized() {
        assert_eq!(sanitize_layer("my layer 1"), "my_layer_1");
        assert_eq!(sanitize_layer(""), "LAYER0");
    }
}
