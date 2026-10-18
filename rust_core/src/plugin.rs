//! 模块插件框架（开发大纲第 11 章）。
//!
//! 目标：新增手作品类 = 实现 trait + 声明 feature，主框架与内核零改动。

/// 模块元信息（FFI 暴露给 Qt 渲染模块选择器）。
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ModuleMeta {
    pub id: &'static str,
    /// i18n key，由 Qt 翻译（Rust 不持有本地化字符串）
    pub name_key: &'static str,
    pub version: &'static str,
}

/// 模块插件 trait。所有实现必须 Send + Sync（FFI 跨线程安全）。
pub trait ModulePlugin: Send + Sync {
    fn meta(&self) -> ModuleMeta;
    /// 参数化模板 ID 列表（对应 resources/pattern_templates 下的 json）
    fn templates(&self) -> Vec<&'static str>;
    /// 依赖的素材库类型（UI 素材 Dock 自动切换依据）
    fn material_kinds(&self) -> Vec<&'static str>;
}

/// 十字绣模块（Phase 1 首个落地模块）。
pub struct CrossStitchModule;

impl ModulePlugin for CrossStitchModule {
    fn meta(&self) -> ModuleMeta {
        ModuleMeta {
            id: "cross_stitch",
            name_key: "module.cross_stitch",
            version: "0.1.0",
        }
    }
    fn templates(&self) -> Vec<&'static str> {
        vec!["blank"]
    }
    fn material_kinds(&self) -> Vec<&'static str> {
        vec!["floss", "bead"]
    }
}

/// 自由纸样模块（Phase 0 几何 Demo 承载，Phase 4 扩展参数化服装）。
pub struct FreePatternModule;

impl ModulePlugin for FreePatternModule {
    fn meta(&self) -> ModuleMeta {
        ModuleMeta {
            id: "free_pattern",
            name_key: "module.free_pattern",
            version: "0.1.0",
        }
    }
    fn templates(&self) -> Vec<&'static str> {
        vec!["seam_rect"]
    }
    fn material_kinds(&self) -> Vec<&'static str> {
        vec!["fabric"]
    }
}

/// 内置模块表。Cargo feature 可裁剪（如 `--no-default-features --features cross_stitch`）。
pub fn builtin_modules() -> Vec<Box<dyn ModulePlugin>> {
    vec![Box::new(CrossStitchModule), Box::new(FreePatternModule)]
}

/// 模块元信息列表（FFI 用）。
pub fn module_metas() -> Vec<ModuleMeta> {
    builtin_modules().iter().map(|m| m.meta()).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn module_ids_unique() {
        let metas = module_metas();
        assert_eq!(metas.len(), 2);
        let ids: Vec<&str> = metas.iter().map(|m| m.id).collect();
        assert!(ids.contains(&"cross_stitch"));
        assert!(ids.contains(&"free_pattern"));
        assert_ne!(metas[0].id, metas[1].id);
    }

    #[test]
    fn module_meta_has_i18n_key() {
        for m in module_metas() {
            assert!(
                m.name_key.starts_with("module."),
                "name_key must be i18n key"
            );
        }
    }
}
