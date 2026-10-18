//! 全局错误类型与错误码号段（开发大纲第 12 章）。
//!
//! 号段：0–999 公共；1000+ 十字绣；2000+ 纸样；3000+ 素材；4000+ 导出；5000+ 工程。
//! Rust 只返回数字错误码，文案由 Qt 按 i18n 翻译。

use std::fmt;

pub mod codes {
    // 公共 0–999
    pub const OK: i32 = 0;
    pub const INVALID_ARG: i32 = 1;
    pub const NULL_PTR: i32 = 2;
    pub const UTF8: i32 = 3;
    pub const INTERNAL: i32 = 4;
    // 十字绣 1000–1999
    pub const CROSS_STITCH_BASE: i32 = 1000;
    pub const IMAGE_DECODE: i32 = 1001;
    pub const IMAGE_TOO_LARGE: i32 = 1002;
    pub const QUANTIZE_FAILED: i32 = 1003;
    // 纸样 2000–2999
    pub const PATTERN_BASE: i32 = 2000;
    pub const GEOMETRY_INVALID: i32 = 2001;
    // 素材 3000–3999
    pub const MATERIAL_BASE: i32 = 3000;
    pub const DB_ERROR: i32 = 3001;
    // 导出 4000–4999
    pub const EXPORT_BASE: i32 = 4000;
    pub const PDF_FAILED: i32 = 4001;
    // 工程 5000–5999
    pub const PROJECT_BASE: i32 = 5000;
    pub const PROJECT_CORRUPT: i32 = 5001;
    pub const PROJECT_VERSION: i32 = 5002;
    pub const PROJECT_UNSUPPORTED_MODULE: i32 = 5003;
}

#[derive(Debug)]
pub enum CoreError {
    InvalidArg(String),
    NullPointer,
    Utf8(String),
    Internal(String),
    ImageDecode(String),
    ImageTooLarge(String),
    QuantizeFailed(String),
    GeometryInvalid(String),
    Db(String),
    Pdf(String),
    ProjectCorrupt(String),
    ProjectVersion(String),
    ProjectUnsupportedModule(String),
    Io(std::io::Error),
    Json(serde_json::Error),
    Bincode(bincode::Error),
    Zip(zip::result::ZipError),
    Sql(rusqlite::Error),
}

impl CoreError {
    pub fn code(&self) -> i32 {
        match self {
            Self::InvalidArg(_) => codes::INVALID_ARG,
            Self::NullPointer => codes::NULL_PTR,
            Self::Utf8(_) => codes::UTF8,
            Self::Internal(_) => codes::INTERNAL,
            Self::ImageDecode(_) => codes::IMAGE_DECODE,
            Self::ImageTooLarge(_) => codes::IMAGE_TOO_LARGE,
            Self::QuantizeFailed(_) => codes::QUANTIZE_FAILED,
            Self::GeometryInvalid(_) => codes::GEOMETRY_INVALID,
            Self::Db(_) | Self::Sql(_) => codes::DB_ERROR,
            Self::Pdf(_) => codes::PDF_FAILED,
            Self::ProjectCorrupt(_) => codes::PROJECT_CORRUPT,
            Self::ProjectVersion(_) => codes::PROJECT_VERSION,
            Self::ProjectUnsupportedModule(_) => codes::PROJECT_UNSUPPORTED_MODULE,
            Self::Io(_) | Self::Json(_) | Self::Bincode(_) | Self::Zip(_) => codes::INTERNAL,
        }
        .max(0)
    }
}

// 工程类错误专用构造（供 export::project 使用）
impl CoreError {
    pub fn project_corrupt(msg: impl Into<String>) -> Self {
        Self::ProjectCorrupt(msg.into())
    }
    pub fn project_version(msg: impl Into<String>) -> Self {
        Self::ProjectVersion(msg.into())
    }
    pub fn project_unsupported_module(msg: impl Into<String>) -> Self {
        Self::ProjectUnsupportedModule(msg.into())
    }
}

impl fmt::Display for CoreError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // 仅用于 tracing 日志诊断，不进入 UI（UI 文案由错误码翻译）
        match self {
            Self::InvalidArg(m) => write!(f, "invalid argument: {m}"),
            Self::NullPointer => write!(f, "null pointer"),
            Self::Utf8(m) => write!(f, "utf8 error: {m}"),
            Self::Internal(m) => write!(f, "internal error: {m}"),
            Self::ImageDecode(m) => write!(f, "image decode failed: {m}"),
            Self::ImageTooLarge(m) => write!(f, "image too large: {m}"),
            Self::QuantizeFailed(m) => write!(f, "quantize failed: {m}"),
            Self::GeometryInvalid(m) => write!(f, "geometry invalid: {m}"),
            Self::Db(m) => write!(f, "database error: {m}"),
            Self::Pdf(m) => write!(f, "pdf export failed: {m}"),
            Self::ProjectCorrupt(m) => write!(f, "project corrupt: {m}"),
            Self::ProjectVersion(m) => write!(f, "project version: {m}"),
            Self::ProjectUnsupportedModule(m) => write!(f, "unsupported module: {m}"),
            Self::Io(e) => write!(f, "io error: {e}"),
            Self::Json(e) => write!(f, "json error: {e}"),
            Self::Bincode(e) => write!(f, "bincode error: {e}"),
            Self::Zip(e) => write!(f, "zip error: {e}"),
            Self::Sql(e) => write!(f, "sqlite error: {e}"),
        }
    }
}

impl std::error::Error for CoreError {}

impl From<std::io::Error> for CoreError {
    fn from(e: std::io::Error) -> Self {
        Self::Io(e)
    }
}
impl From<serde_json::Error> for CoreError {
    fn from(e: serde_json::Error) -> Self {
        Self::Json(e)
    }
}
impl From<bincode::Error> for CoreError {
    fn from(e: bincode::Error) -> Self {
        Self::Bincode(e)
    }
}
impl From<zip::result::ZipError> for CoreError {
    fn from(e: zip::result::ZipError) -> Self {
        Self::Zip(e)
    }
}
impl From<rusqlite::Error> for CoreError {
    fn from(e: rusqlite::Error) -> Self {
        Self::Sql(e)
    }
}
impl From<printpdf::Error> for CoreError {
    fn from(e: printpdf::Error) -> Self {
        Self::Pdf(e.to_string())
    }
}
impl From<image::ImageError> for CoreError {
    fn from(e: image::ImageError) -> Self {
        Self::ImageDecode(e.to_string())
    }
}

pub type CoreResult<T> = Result<T, CoreError>;
