//! 矢量纸样引擎（kurbo）。Phase 0 落地几何 Demo，后续扩展贝塞尔 / 布尔运算。

pub mod geometry;
pub mod piece;

pub use piece::Piece;

use crate::common::ffi_types::CmPoint;

/// 裁片路径对（FFI 句柄持有）。
#[derive(Clone, Debug, Default)]
pub struct PiecePaths {
    pub outline: Vec<CmPoint>,
    pub seam: Vec<CmPoint>,
}
