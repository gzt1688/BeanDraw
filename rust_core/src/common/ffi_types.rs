//! FFI C 兼容类型定义。
//!
//! 对应 Qt 侧 `qt_ui/src/ffi_bind/ffi_types.h`，修改必须双边同步（开发大纲第 12 章）。
//! 约定：批量数据以「连续数组指针 + 长度 + 释放函数」传输；句柄仅 Rust 侧释放。

use std::os::raw::{c_char, c_void};

/// 进度回调：progress ∈ [0,1]，stage 为阶段码（见 `progress::stage`）。
/// 约定 progress < 0 表示失败终止。
pub type CmProgressFn = Option<extern "C" fn(user_data: *mut c_void, progress: f32, stage: i32)>;

/// 2D 点（内部单位：毫米）。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct CmPoint {
    pub x: f64,
    pub y: f64,
}

/// 十字绣生成参数。
#[repr(C)]
#[derive(Clone, Copy, Debug, serde::Serialize, serde::Deserialize)]
pub struct CmCrossStitchParams {
    /// 网格宽（格）
    pub grid_width: u32,
    /// 网格高（格）
    pub grid_height: u32,
    /// 最大颜色数（1..=64）
    pub max_colors: u32,
    /// 珠绣模式：0 关 / 1 开
    pub bead_mode: u32,
    /// 面料计数（ct，如 14 表示 14ct Aida），用于用量估算
    pub fabric_count: u32,
    /// 抖动（Floyd–Steinberg）：0 关 / 1 开
    pub dither: u32,
}

impl Default for CmCrossStitchParams {
    fn default() -> Self {
        Self {
            grid_width: 100,
            grid_height: 100,
            max_colors: 16,
            bead_mode: 0,
            fabric_count: 14,
            dither: 0,
        }
    }
}

/// 十字绣高级生成选项。
///
/// 独立于 `CmCrossStitchParams`：工程文件 schema 演进时该结构可整体缺省，
/// 旧版本 `.bean` 仍能反序列化（开发大纲第 14 章向前兼容）。
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CmStitchOptions {
    /// 符号集（见 `symbol::set` 常量）：0 字母 / 1 数字 / 2 几何 / 3 混合
    pub symbol_set: u32,
    /// 下采样归并策略（见 `sample::mode` 常量）：0 盒式平均 / 1 众数 / 2 中心色(medoid)
    pub sample_mode: u32,
    /// 去杂点强度：0 关 / 1 弱（孤立单针）/ 2 强（孤立 + 小碎片）
    pub cleanup: u32,
    /// 稀有色合并阈值（千分数，0 = 不合并）：占比低于此值的色并入 Lab 最近主色
    pub rare_color_ppt: u32,
    /// 手作品类模式（见 `craft::mode` 常量）：0 十字绣 / 1 珠绣 / 2 拼豆。
    /// 原为对齐保留位；旧工程缺省该字段（serde default）→ 回落 0（十字绣）。
    #[serde(default)]
    pub craft_mode: u32,
}

impl Default for CmStitchOptions {
    fn default() -> Self {
        Self {
            symbol_set: 0,
            sample_mode: 1,
            cleanup: 1,
            rare_color_ppt: 3,
            craft_mode: 0,
        }
    }
}

/// 符号集常量（与 Qt 侧 `SymbolSet` 枚举一一对应）。
pub mod symbol {
    pub const LETTERS: u32 = 0;
    pub const DIGITS: u32 = 1;
    pub const GEOMETRIC: u32 = 2;
    pub const MIXED: u32 = 3;
}

/// 下采样归并策略常量（与 Qt 侧 `SampleMode` 枚举一一对应）。
pub mod sample {
    /// 盒式平均：色彩过渡平滑，但会产生原图/色卡中不存在的中间色。
    pub const MEAN: u32 = 0;
    /// 众数：取格内出现最多的颜色，保留实色、轮廓锐利。
    pub const MODE: u32 = 1;
    /// 中心色(medoid)：取与格内所有像素总距离最小的候选色，兼顾平滑与实色。
    pub const MEDOID: u32 = 2;
}

/// 去杂点强度常量。
pub mod cleanup {
    pub const OFF: u32 = 0;
    pub const LIGHT: u32 = 1;
    pub const STRONG: u32 = 2;
}

/// 手作品类模式常量（`CmStitchOptions::craft_mode`）。
pub mod craft {
    /// 十字绣（默认）：绣线 + DMC 色卡
    pub const CROSS_STITCH: u32 = 0;
    /// 珠绣：每格一粒珠，仍按 DMC 珠色卡
    pub const BEAD: u32 = 1;
    /// 拼豆：每格一粒 5mm 豆，按 Artkal S 系列拼豆色卡
    pub const PERLER: u32 = 2;
}

/// 成品尺寸（由网格与面料计数换算，供 UI 与图纸标注使用）。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CmFabricSize {
    /// 成品宽（毫米）
    pub width_mm: u32,
    /// 成品高（毫米）
    pub height_mm: u32,
    /// 单格边长（毫米 × 100）
    pub cell_mm_x100: u32,
}

/// 网格单元。color_index = COLOR_NONE 表示空（透明底）。
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct CmGridCell {
    pub x: u32,
    pub y: u32,
    pub color_index: u32,
    pub symbol_index: u32,
}

/// 图例 / 用量统计条目。字符串通过 `cm_chart_color_code/name` 获取。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct CmLegendEntry {
    pub color_index: u32,
    pub symbol_index: u32,
    /// 绣针数（珠绣模式下为珠子数）
    pub stitch_count: u32,
    /// 珠子数（非珠绣模式为 0）
    pub bead_count: u32,
    /// 估算用线长度（厘米 × 100）
    pub length_cm_x100: u32,
}

/// 模块元信息。字符串为静态生命周期，由注册表持有，无需释放。
/// 裸指针仅指向进程内静态数据，因此手工实现 Send/Sync（FFI 静态表需要）。
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CmModuleInfo {
    pub id: *const c_char,
    /// i18n key，由 Qt 翻译（Rust 不持有本地化字符串）
    pub name_key: *const c_char,
    pub version: *const c_char,
}

unsafe impl Send for CmModuleInfo {}
unsafe impl Sync for CmModuleInfo {}

/// 几何路径集合（裁片轮廓 + 缝份路径）。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CmPaths {
    pub outline: *const CmPoint,
    pub outline_len: usize,
    pub seam: *const CmPoint,
    pub seam_len: usize,
}

/// 素材查询结果（定长字符数组，避免跨语言字符串生命周期问题）。
/// code ≤ 15 字符，name ≤ 47 字符。
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CmDmcColor {
    pub rgb: [u8; 3],
    pub code: [std::os::raw::c_char; 16],
    pub name: [std::os::raw::c_char; 48],
}

/// 空颜色哨兵值。
pub const COLOR_NONE: u32 = u32::MAX;

/// 网格参数上限，防止异常输入导致巨量分配。
pub const MAX_GRID_SIDE: u32 = 2000;
pub const MAX_COLORS: u32 = 64;
