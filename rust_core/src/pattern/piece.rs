//! 裁片基础数据模型（开发大纲 pattern/piece.rs）。

use crate::common::ffi_types::CmPoint;

/// 裁片：名称 + 轮廓 + 缝份路径 + 刀口标记位置。
#[derive(Clone, Debug, Default)]
pub struct Piece {
    pub name: String,
    pub outline: Vec<CmPoint>,
    pub seam: Vec<CmPoint>,
    /// 刀口标记（轮廓上的位置）
    pub notches: Vec<CmPoint>,
    /// 对位标记（裁剪对位点）
    pub registration_marks: Vec<CmPoint>,
    /// 数量（裁剪份数）
    pub quantity: u32,
}

impl Piece {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            quantity: 1,
            ..Default::default()
        }
    }

    /// 周长（mm）。
    pub fn perimeter_mm(&self) -> f64 {
        perimeter(&self.outline)
    }
}

fn perimeter(points: &[CmPoint]) -> f64 {
    if points.len() < 2 {
        return 0.0;
    }
    let mut total = 0.0;
    for i in 0..points.len() {
        let a = points[i];
        let b = points[(i + 1) % points.len()];
        total += ((b.x - a.x).powi(2) + (b.y - a.y).powi(2)).sqrt();
    }
    total
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn piece_perimeter() {
        let mut p = Piece::new("front");
        p.outline = vec![
            CmPoint { x: 0.0, y: 0.0 },
            CmPoint { x: 100.0, y: 0.0 },
            CmPoint { x: 100.0, y: 50.0 },
            CmPoint { x: 0.0, y: 50.0 },
        ];
        assert!((p.perimeter_mm() - 300.0).abs() < 1e-9);
        assert_eq!(p.quantity, 1);
    }
}
