//! 素材资源库（开发大纲 material_lib）。

pub mod db;
pub mod model;

pub use db::MaterialDb;
pub use model::{Material, MaterialKind};
