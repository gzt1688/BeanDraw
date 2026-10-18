//! PDF 字体管理（开发大纲 export/font.rs）。
//!
//! 本地化边界（第 7 章）：Rust 只负责排版；文案由 UI 层已翻译后传入。
//! Phase 1 使用 PDF 内置字体（WinAnsi，ASCII 符号安全）；
//! TTF 嵌入（中文图纸标注、Unicode 刺绣符号）在 Phase 3 实现。

use crate::common::error::{CoreError, CoreResult};
use printpdf::BuiltinFont;
use std::path::Path;

pub fn builtin(font: BuiltinFont) -> BuiltinFont {
    font
}

/// 可嵌入 TTF（预留接口，Phase 3 实现字体子集化）。
pub fn embed_ttf(_path: &Path) -> CoreResult<()> {
    Err(CoreError::Pdf(
        "TTF embedding is scheduled for Phase 3; builtin fonts used meanwhile".into(),
    ))
}
