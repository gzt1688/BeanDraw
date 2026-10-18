//! 素材库数据模型（开发大纲 material_lib/model.rs）。

/// 素材大类。
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MaterialKind {
    Fabric,
    Yarn,
    Leather,
    Button,
    Zipper,
    Bead,
    /// Artkal 拼豆（5mm 软胶豆），色卡来源 perler_data。
    Perler,
}

impl MaterialKind {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Fabric => "fabric",
            Self::Yarn => "yarn",
            Self::Leather => "leather",
            Self::Button => "button",
            Self::Zipper => "zipper",
            Self::Bead => "bead",
            Self::Perler => "perler",
        }
    }

    /// 字符串 → 枚举（FFI 入参解析）。
    pub fn from_str_opt(s: &str) -> Option<Self> {
        match s {
            "fabric" => Some(Self::Fabric),
            "yarn" => Some(Self::Yarn),
            "leather" => Some(Self::Leather),
            "button" => Some(Self::Button),
            "zipper" => Some(Self::Zipper),
            "bead" => Some(Self::Bead),
            "perler" => Some(Self::Perler),
            _ => None,
        }
    }
}

/// 素材条目。
#[derive(Clone, Debug)]
pub struct Material {
    pub kind: MaterialKind,
    /// 编号（如 DMC 色号、皮料型号）
    pub code: String,
    /// 名称
    pub name: String,
    /// 颜色（如有）
    pub rgb: Option<[u8; 3]>,
    /// 单价（分，避免浮点误差）
    pub price_cents: Option<u32>,
    /// 备注
    pub note: String,
}
