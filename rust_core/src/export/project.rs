//! `.bean` 工程文件：ZIP 容器 + manifest.json + 数据分块 + 预览图（开发大纲第 14 章）。
//!
//! 结构：
//! ```text
//! manifest.json            # 元数据（magic / format_version / module_id / schema_version）
//! data/cross_stitch.bin    # bincode 数据块（仅承诺同 schema 版本内兼容）
//! preview.png              # 工程缩略图
//! ```
//! 大资源（贴图）按 manifest.assets 以相对路径外置引用，不进工程文件。

use crate::common::error::{CoreError, CoreResult};
use crate::cross_stitch::{ChartData, CrossStitchChart};
use serde::{Deserialize, Serialize};
use std::fs::File;
use std::io::{Read, Write};
use std::path::Path;
use zip::{CompressionMethod, ZipArchive, ZipWriter};

pub const MAGIC: &str = "BEAN1";
/// 旧版 CraftMaster 时代的 magic，打开时一并接受（向后兼容旧工程文件）。
pub const LEGACY_MAGIC: &str = "CRAFT1";
pub const FORMAT_VERSION: u32 = 1;

#[derive(Serialize, Deserialize, Debug)]
pub struct Manifest {
    pub magic: String,
    pub format_version: u32,
    pub schema_version: u32,
    /// 所属手作模块（打开时自动切换工作模式）
    pub module_id: String,
    pub app_version: String,
    /// 外置资源清单（资源 ID → 相对路径）
    pub assets: Vec<String>,
}

fn zip_options() -> zip::write::FileOptions<'static, ()> {
    zip::write::FileOptions::default()
        .compression_method(CompressionMethod::Deflated)
        .large_file(false)
}

/// 保存工程（含 4px/格 预览图）。
pub fn save_project(chart: &CrossStitchChart, path: &Path) -> CoreResult<()> {
    let file = File::create(path)?;
    let mut zip = ZipWriter::new(file);

    let manifest = Manifest {
        magic: MAGIC.to_string(),
        format_version: FORMAT_VERSION,
        schema_version: ChartData::SCHEMA_VERSION,
        module_id: "cross_stitch".to_string(),
        app_version: env!("CARGO_PKG_VERSION").to_string(),
        assets: Vec::new(),
    };
    let json = serde_json::to_vec_pretty(&manifest)?;
    zip.start_file("manifest.json", zip_options())?;
    zip.write_all(&json)?;

    zip.start_file("data/cross_stitch.bin", zip_options())?;
    zip.write_all(&chart.to_bytes()?)?;

    let preview = crate::cross_stitch::export::render_png(chart, 4)?;
    zip.start_file("preview.png", zip_options())?;
    zip.write_all(&preview)?;

    zip.finish()?;
    Ok(())
}

/// 打开工程。校验 magic（兼容旧版 `CRAFT1`）/ format_version / 模块支持。
pub fn open_project(path: &Path) -> CoreResult<CrossStitchChart> {
    let file = File::open(path)?;
    let mut archive = ZipArchive::new(file)?;

    let mut manifest_json = String::new();
    archive
        .by_name("manifest.json")?
        .read_to_string(&mut manifest_json)?;
    let manifest: Manifest = serde_json::from_str(&manifest_json)
        .map_err(|e| CoreError::project_corrupt(format!("bad manifest: {e}")))?;

    if manifest.magic != MAGIC && manifest.magic != LEGACY_MAGIC {
        return Err(CoreError::project_corrupt(format!(
            "bad magic {:?}, expected {MAGIC:?} or {LEGACY_MAGIC:?}",
            manifest.magic
        )));
    }
    if manifest.format_version > FORMAT_VERSION {
        return Err(CoreError::project_version(format!(
            "format_version {} newer than supported {}",
            manifest.format_version, FORMAT_VERSION
        )));
    }
    if manifest.module_id != "cross_stitch" {
        return Err(CoreError::project_unsupported_module(format!(
            "module {:?} not supported by this build",
            manifest.module_id
        )));
    }

    let mut bin = Vec::new();
    archive
        .by_name("data/cross_stitch.bin")?
        .read_to_end(&mut bin)?;
    CrossStitchChart::from_bytes(&bin)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::ffi_types::{CmCrossStitchParams, CmGridCell};
    use crate::cross_stitch::DmcColor;

    fn sample_chart() -> CrossStitchChart {
        let mut chart = CrossStitchChart::empty(6, 6, CmCrossStitchParams::default());
        chart.palette = vec![DmcColor {
            code: "321",
            name: "Red",
            rgb: [199, 15, 46],
        }];
        chart.symbols = vec!['A'];
        chart.cells = (0..36)
            .map(|i| CmGridCell {
                x: i % 6,
                y: i / 6,
                color_index: if i % 3 == 0 {
                    0
                } else {
                    COLOR_NONE_PLACEHOLDER
                },
                symbol_index: 0,
            })
            .collect();
        chart
    }

    const COLOR_NONE_PLACEHOLDER: u32 = crate::common::ffi_types::COLOR_NONE;

    #[test]
    fn project_save_open_roundtrip() {
        let dir = std::env::temp_dir();
        let path = dir.join("beandraw_test_project.bean");
        let chart = sample_chart();
        save_project(&chart, &path).unwrap();
        let restored = open_project(&path).unwrap();
        assert_eq!(restored.cells.len(), chart.cells.len());
        assert_eq!(restored.total_stitches(), chart.total_stitches());
        assert_eq!(restored.palette[0].code, "321");
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn project_rejects_corrupt_file() {
        let dir = std::env::temp_dir();
        let path = dir.join("beandraw_test_corrupt.bean");
        std::fs::write(&path, b"not a zip").unwrap();
        let err = open_project(&path).unwrap_err();
        assert!(matches!(err, CoreError::Zip(_) | CoreError::Io(_)));
        std::fs::remove_file(&path).ok();
    }
}
