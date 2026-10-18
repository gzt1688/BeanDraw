//! 纸样几何引擎（kurbo 封装，Phase 0 Demo：带缝份矩形裁片）。

use crate::common::error::{CoreError, CoreResult};
use crate::common::ffi_types::CmPoint;
use kurbo::Rect;

/// 矩形裁片：轮廓 + 缝份外扩路径。
/// 内部单位：毫米。
pub fn seam_rect(
    width_mm: f64,
    height_mm: f64,
    seam_mm: f64,
) -> CoreResult<(Vec<CmPoint>, Vec<CmPoint>)> {
    if !width_mm.is_finite() || !height_mm.is_finite() || !seam_mm.is_finite() {
        return Err(CoreError::GeometryInvalid("non-finite parameter".into()));
    }
    if width_mm <= 0.0 || height_mm <= 0.0 {
        return Err(CoreError::GeometryInvalid(format!(
            "size {width_mm}x{height_mm} must be positive"
        )));
    }
    if seam_mm < 0.0 || seam_mm > width_mm.min(height_mm) / 2.0 {
        return Err(CoreError::GeometryInvalid(format!(
            "seam {seam_mm} out of range 0..={}",
            width_mm.min(height_mm) / 2.0
        )));
    }

    let outline = rect_points(Rect::new(0.0, 0.0, width_mm, height_mm));
    let seam = rect_points(Rect::new(
        -seam_mm,
        -seam_mm,
        width_mm + seam_mm,
        height_mm + seam_mm,
    ));
    Ok((outline, seam))
}

/// 矩形 → 4 顶点（CCW）。
fn rect_points(r: Rect) -> Vec<CmPoint> {
    vec![
        CmPoint { x: r.x0, y: r.y0 },
        CmPoint { x: r.x1, y: r.y0 },
        CmPoint { x: r.x1, y: r.y1 },
        CmPoint { x: r.x0, y: r.y1 },
    ]
}

/// 多边形面积（鞋带公式，mm²）。
pub fn polygon_area_mm2(points: &[CmPoint]) -> f64 {
    if points.len() < 3 {
        return 0.0;
    }
    let mut sum = 0.0;
    for i in 0..points.len() {
        let a = points[i];
        let b = points[(i + 1) % points.len()];
        sum += a.x * b.y - b.x * a.y;
    }
    (sum / 2.0).abs()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn seam_rect_basic() {
        let (outline, seam) = seam_rect(100.0, 50.0, 10.0).unwrap();
        assert_eq!(outline.len(), 4);
        assert_eq!(seam.len(), 4);
        assert_eq!(seam[0], CmPoint { x: -10.0, y: -10.0 });
        assert_eq!(seam[2], CmPoint { x: 110.0, y: 60.0 });
        assert!((polygon_area_mm2(&outline) - 5000.0).abs() < 1e-9);
        assert!((polygon_area_mm2(&seam) - 120.0 * 70.0).abs() < 1e-9);
    }

    #[test]
    fn seam_rect_validation() {
        assert!(seam_rect(0.0, 10.0, 1.0).is_err());
        assert!(seam_rect(-5.0, 10.0, 1.0).is_err());
        assert!(seam_rect(10.0, 10.0, -1.0).is_err());
        assert!(seam_rect(10.0, 10.0, 6.0).is_err()); // 超过半宽
        assert!(seam_rect(f64::NAN, 10.0, 1.0).is_err());
    }
}
