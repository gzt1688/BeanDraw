//! BeanDraw Rust Core FFI 入口（开发大纲第 12 章）。
//!
//! 约定：
//! - 函数命名 `cm_<模块>_<动作>`；资源为不透明句柄，仅 Rust 释放
//! - 批量数据以「数组指针 + 长度」传输；字符串以 `*const c_char` 暴露
//! - 错误经可选 `err: *mut i32` 输出数字错误码（文案由 Qt 翻译）
//! - 所有导出函数 panic 安全（catch_unwind，绝不到 C++ 栈展开）
//!
//! 内部模块开放为 pub：便于 Rust 侧集成测试与未来插件复用；
//! FFI 边界按设计解引用裸指针（调用契约由 C 头文件锁定）。
#![allow(clippy::not_unsafe_ptr_arg_deref)]

pub mod command;
pub mod common;
pub mod cross_stitch;
pub mod export;
pub mod material_lib;
pub mod pattern;
pub mod plugin;

use std::ffi::{c_char, c_void, CStr, CString};
use std::os::raw::c_int;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::Path;
use std::slice;
use std::sync::OnceLock;

use command::History;
use common::error::{codes, CoreError, CoreResult};
use common::ffi_types::*;
use common::progress::Progress;
use cross_stitch::CrossStitchChart;
use material_lib::{MaterialDb, MaterialKind};
use pattern::PiecePaths;

// ============================================================
// 内部工具
// ============================================================

/// panic 安全执行 + 错误码输出。
fn guard<T>(err: *mut c_int, f: impl FnOnce() -> CoreResult<T>) -> Option<T> {
    match catch_unwind(AssertUnwindSafe(f)) {
        Ok(Ok(v)) => {
            if !err.is_null() {
                unsafe { *err = codes::OK };
            }
            Some(v)
        }
        Ok(Err(e)) => {
            if !err.is_null() {
                unsafe { *err = e.code() };
            }
            None
        }
        Err(_) => {
            if !err.is_null() {
                unsafe { *err = codes::INTERNAL };
            }
            None
        }
    }
}

/// 安全 C 字符串切片（空指针 → 空串）。
unsafe fn cstr_to_str<'a>(ptr: *const c_char) -> &'a str {
    if ptr.is_null() {
        return "";
    }
    CStr::from_ptr(ptr).to_str().unwrap_or("")
}

/// Path 辅助：C 字符串路径 → &Path。
unsafe fn cstr_to_path<'a>(ptr: *const c_char) -> CoreResult<&'a Path> {
    if ptr.is_null() {
        return Err(CoreError::NullPointer);
    }
    let s = CStr::from_ptr(ptr)
        .to_str()
        .map_err(|_| CoreError::Utf8("path is not valid UTF-8".into()))?;
    Ok(Path::new(s))
}

// ============================================================
// 版本 / 模块注册表
// ============================================================

struct ModuleInfoStorage {
    _cstrings: Vec<CString>,
    infos: Vec<CmModuleInfo>,
}

static MODULE_INFO: OnceLock<ModuleInfoStorage> = OnceLock::new();

fn module_info_storage() -> &'static ModuleInfoStorage {
    MODULE_INFO.get_or_init(|| {
        let metas = plugin::module_metas();
        let mut cstrings: Vec<CString> = Vec::with_capacity(metas.len() * 3);
        let mut infos = Vec::with_capacity(metas.len());
        for m in &metas {
            let id = CString::new(m.id).expect("module id ascii");
            let name_key = CString::new(m.name_key).expect("name key ascii");
            let version = CString::new(m.version).expect("version ascii");
            infos.push(CmModuleInfo {
                id: id.as_ptr(),
                name_key: name_key.as_ptr(),
                version: version.as_ptr(),
            });
            cstrings.push(id);
            cstrings.push(name_key);
            cstrings.push(version);
        }
        ModuleInfoStorage {
            _cstrings: cstrings,
            infos,
        }
    })
}

/// 内核版本号（语义化版本）。
static VERSION_CSTRING: OnceLock<CString> = OnceLock::new();

#[no_mangle]
pub extern "C" fn cm_version() -> *const c_char {
    VERSION_CSTRING
        .get_or_init(|| CString::new(env!("CARGO_PKG_VERSION")).expect("version ascii"))
        .as_ptr()
}

/// 模块列表（静态数据，无需释放）。返回数组指针，数量经 out_len 输出。
#[no_mangle]
pub extern "C" fn cm_modules_list(out_len: *mut usize) -> *const CmModuleInfo {
    let storage = module_info_storage();
    if !out_len.is_null() {
        unsafe { *out_len = storage.infos.len() };
    }
    storage.infos.as_ptr()
}

// ============================================================
// 十字绣：生成 / 访问 / 编辑
// ============================================================

/// 生成十字绣图纸。rgba 为 RGBA8 源图（width × height × 4 字节）。
/// `options` 可为空（取默认高级选项）。
#[no_mangle]
pub extern "C" fn cm_cross_stitch_generate(
    rgba: *const u8,
    rgba_len: usize,
    width: u32,
    height: u32,
    params: *const CmCrossStitchParams,
    options: *const CmStitchOptions,
    progress_cb: CmProgressFn,
    user_data: *mut c_void,
    err: *mut c_int,
) -> *mut CrossStitchChart {
    guard(err, || {
        if params.is_null() {
            return Err(CoreError::NullPointer);
        }
        let params = unsafe { *params };
        let options = if options.is_null() {
            CmStitchOptions::default()
        } else {
            unsafe { *options }
        };
        let pixels = unsafe { slice::from_raw_parts(rgba, rgba_len) };
        let mut progress = Progress::new(progress_cb, user_data);
        let chart =
            cross_stitch::generate_chart(pixels, width, height, params, options, &mut progress)?;
        Ok(Box::into_raw(Box::new(chart)))
    })
    .unwrap_or(std::ptr::null_mut())
}

/// 工程的生成参数与高级选项（UI 侧镜像保存，避免重复解析）。
#[no_mangle]
pub extern "C" fn cm_chart_params(
    chart: *const CrossStitchChart,
    out_params: *mut CmCrossStitchParams,
    out_options: *mut CmStitchOptions,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &*chart };
    unsafe {
        if !out_params.is_null() {
            *out_params = chart.params;
        }
        if !out_options.is_null() {
            *out_options = chart.options;
        }
    }
    codes::OK
}

/// 成品尺寸（网格 × 面料计数 → 毫米）。
#[no_mangle]
pub extern "C" fn cm_chart_fabric_size(
    chart: *const CrossStitchChart,
    out: *mut CmFabricSize,
) -> c_int {
    if chart.is_null() || out.is_null() {
        return codes::NULL_PTR;
    }
    unsafe {
        *out = (&*chart).fabric_size();
    }
    codes::OK
}

/// 由目标成品尺寸反推网格数（属性面板「按尺寸生成」）。
#[no_mangle]
pub extern "C" fn cm_grid_for_size(
    width_mm: u32,
    height_mm: u32,
    fabric_count: u32,
    out_w: *mut u32,
    out_h: *mut u32,
) -> c_int {
    if out_w.is_null() || out_h.is_null() {
        return codes::NULL_PTR;
    }
    let (w, h) = cross_stitch::grid_gen::grid_for_size(width_mm, height_mm, fabric_count);
    unsafe {
        *out_w = w;
        *out_h = h;
    }
    codes::OK
}

/// 符号集内的第 index 个符号（UTF-32 码点；越界回绕）。
#[no_mangle]
pub extern "C" fn cm_symbol_in_set(symbol_set: u32, index: u32) -> u32 {
    common::util::symbol_in_set(symbol_set, index as usize) as u32
}

// ------------------------------------------------------------
// 图纸编辑（画布工具 → 命令栈 → 内核）
// ------------------------------------------------------------

/// 单格涂色 / 擦除（color_index = COLOR_NONE 表示擦除）。输出改动格数。
#[no_mangle]
pub extern "C" fn cm_chart_paint_cell(
    chart: *mut CrossStitchChart,
    x: u32,
    y: u32,
    color_index: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.paint_cell(x, y, color_index)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 区域填充（油漆桶）。输出改动格数。
#[no_mangle]
pub extern "C" fn cm_chart_flood_fill(
    chart: *mut CrossStitchChart,
    x: u32,
    y: u32,
    color_index: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.flood_fill(x, y, color_index)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 矩形填充。输出改动格数。
#[no_mangle]
pub extern "C" fn cm_chart_rect_fill(
    chart: *mut CrossStitchChart,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    color_index: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.rect_fill(x0, y0, x1, y1, color_index)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 批量填色：对坐标列表逐格设置颜色（圆形 / 椭圆 / 多边形填充共用）。
/// 输出实际改变的格数。
#[no_mangle]
pub extern "C" fn cm_chart_fill_cells(
    chart: *mut CrossStitchChart,
    xs: *const u32,
    ys: *const u32,
    count: usize,
    color_index: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    if xs.is_null() || ys.is_null() || count == 0 {
        return codes::INVALID_ARG;
    }
    let n = guard(err, || {
        let chart = unsafe { &mut *chart };
        let xs = unsafe { std::slice::from_raw_parts(xs, count) };
        let ys = unsafe { std::slice::from_raw_parts(ys, count) };
        let points: Vec<(u32, u32)> = xs.iter().zip(ys.iter()).map(|(&x, &y)| (x, y)).collect();
        chart.fill_cells(&points, color_index)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 区域镜像（闭区间）：horizontal=1 左右镜像，0 上下镜像。
/// 输出发生变化（与源不同）的格数。
#[no_mangle]
pub extern "C" fn cm_chart_mirror_region(
    chart: *mut CrossStitchChart,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    horizontal: c_int,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let n = guard(err, || {
        let chart = unsafe { &mut *chart };
        Ok::<u32, CoreError>(chart.mirror_region(x0, y0, x1, y1, horizontal != 0))
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 任意直线轴镜像（闭区间）：轴由 (ax0,ay0)-(ax1,ay1) 两点定义（格子单位，
/// 可传半格坐标，如竖格线 x=k 用 (k,0)-(k,1)）。输出实际写入的格数。
#[no_mangle]
pub extern "C" fn cm_chart_mirror_region_axis(
    chart: *mut CrossStitchChart,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    ax0: f64,
    ay0: f64,
    ax1: f64,
    ay1: f64,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let n = guard(err, || {
        let chart = unsafe { &mut *chart };
        chart.mirror_region_axis(x0, y0, x1, y1, ax0, ay0, ax1, ay1)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 4-连通同色区域识别：返回与 (x,y) 同色的相连区域格子索引列表
/// （索引 = y*width + x）。out_indices 由调用方按 width*height 预分配，
/// out_count 写入实际区域格数（点击空格子时为 0）。
#[no_mangle]
pub extern "C" fn cm_chart_flood_region(
    chart: *const CrossStitchChart,
    x: u32,
    y: u32,
    out_indices: *mut u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let region = guard(err, || {
        let chart = unsafe { &*chart };
        chart
            .flood_region(x, y)
            .ok_or_else(|| CoreError::InvalidArg("flood region out of bounds".to_string()))
    });
    match region {
        Some(cells) => {
            let n = cells.len();
            if n > 0 && !out_indices.is_null() {
                let dst = unsafe { std::slice::from_raw_parts_mut(out_indices, n) };
                for (d, &i) in dst.iter_mut().zip(cells.iter()) {
                    *d = i as u32;
                }
            }
            if !out_count.is_null() {
                unsafe { *out_count = n as u32 };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 区域缩放（最近邻重采样）：把 [x0,y0,x1,y1] 内容按 new_w×new_h 重采样，
/// 写回以 (dst_x0, dst_y0) 为左上角的新区域。输出实际改变的格数。
#[no_mangle]
pub extern "C" fn cm_chart_scale_region(
    chart: *mut CrossStitchChart,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    dst_x0: u32,
    dst_y0: u32,
    new_w: u32,
    new_h: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.scale_region(x0, y0, x1, y1, dst_x0, dst_y0, new_w, new_h)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 移动选区内容（剪切平移，不缩放）。dx/dy 为格子偏移，可正可负。
#[no_mangle]
pub extern "C" fn cm_chart_move_region(
    chart: *mut CrossStitchChart,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    dx: i32,
    dy: i32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.move_region(x0, y0, x1, y1, dx, dy)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 全局换色。输出改动格数。
#[no_mangle]
pub extern "C" fn cm_chart_replace_color(
    chart: *mut CrossStitchChart,
    from: u32,
    to: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.replace_color(from, to)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 合并颜色：from 的针全部改用 to，并把 from 从调色板移除。输出改动格数。
#[no_mangle]
pub extern "C" fn cm_chart_merge_color(
    chart: *mut CrossStitchChart,
    from: u32,
    to: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.merge_color_into(from, to)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 去杂点（strength: 0 关 / 1 弱 / 2 强）。输出改动格数。
#[no_mangle]
pub extern "C" fn cm_chart_cleanup(
    chart: *mut CrossStitchChart,
    strength: u32,
    out_count: *mut u32,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    let n = chart.cleanup(strength);
    if !out_count.is_null() {
        unsafe { *out_count = n };
    }
    codes::OK
}

/// 清理未使用颜色（删除用量为 0 的调色板项）。输出被删除的颜色种数。
#[no_mangle]
pub extern "C" fn cm_chart_prune_unused(
    chart: *mut CrossStitchChart,
    out_count: *mut u32,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    let n = chart.prune_unused_colors();
    if !out_count.is_null() {
        unsafe { *out_count = n };
    }
    codes::OK
}

/// 稀有色合并（ppt = 千分比阈值）。输出被合并的颜色种数。
#[no_mangle]
pub extern "C" fn cm_chart_merge_rare(
    chart: *mut CrossStitchChart,
    ppt: u32,
    out_count: *mut u32,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    let n = chart.merge_rare_colors(ppt);
    if !out_count.is_null() {
        unsafe { *out_count = n };
    }
    codes::OK
}

/// 删除颜色：该色所有针清空并从调色板移除。输出被清空的针数。
#[no_mangle]
pub extern "C" fn cm_chart_delete_color(
    chart: *mut CrossStitchChart,
    color_index: u32,
    out_count: *mut u32,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        chart.delete_color(color_index)
    });
    match n {
        Some(v) => {
            if !out_count.is_null() {
                unsafe { *out_count = v };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 更改某色符号（new_symbol 为 Unicode 码点）。
/// 目标符号被占用时两色交换。输出被交换的另一色索引（-1 = 无冲突）。
#[no_mangle]
pub extern "C" fn cm_chart_set_color_symbol(
    chart: *mut CrossStitchChart,
    color_index: u32,
    new_symbol: u32,
    out_swapped: *mut c_int,
    err: *mut c_int,
) -> c_int {
    let n = guard(err, || {
        null_check(chart)?;
        let chart = unsafe { &mut *chart };
        let symbol = char::from_u32(new_symbol).ok_or(CoreError::InvalidArg(
            "new_symbol is not a valid unicode scalar".into(),
        ))?;
        chart.set_color_symbol(color_index, symbol)
    });
    match n {
        Some(v) => {
            if !out_swapped.is_null() {
                unsafe { *out_swapped = v.map(|i| i as c_int).unwrap_or(-1) };
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 切换符号集（0 字母 / 1 数字 / 2 几何 / 3 混合）。
#[no_mangle]
pub extern "C" fn cm_chart_set_symbol_set(chart: *mut CrossStitchChart, symbol_set: u32) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    chart.set_symbol_set(symbol_set);
    codes::OK
}

/// 按当前用量重新分配符号。
#[no_mangle]
pub extern "C" fn cm_chart_redistribute_symbols(chart: *mut CrossStitchChart) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    chart.redistribute_symbols();
    codes::OK
}

/// 把指定 DMC 色号加入图纸调色板（已存在则返回已有索引）。返回索引，失败返回负数。
#[no_mangle]
pub extern "C" fn cm_chart_add_color(chart: *mut CrossStitchChart, code: *const c_char) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    let code = unsafe { cstr_to_str(code) };
    chart.add_palette_color(code)
}

/// 句柄空指针检查（编辑类 FFI 统一前置）。
fn null_check<T>(p: *const T) -> CoreResult<()> {
    if p.is_null() {
        Err(CoreError::NullPointer)
    } else {
        Ok(())
    }
}

/// 释放图纸句柄。
#[no_mangle]
pub extern "C" fn cm_chart_free(chart: *mut CrossStitchChart) {
    if chart.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(chart));
    }
}

/// 网格数组（借用指针，生命周期同 chart）。
#[no_mangle]
pub extern "C" fn cm_chart_cells(
    chart: *const CrossStitchChart,
    out_ptr: *mut *const CmGridCell,
    out_len: *mut usize,
) -> c_int {
    if chart.is_null() || out_ptr.is_null() || out_len.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &*chart };
    let (ptr, len) = chart.cells_slice();
    unsafe {
        *out_ptr = ptr;
        *out_len = len;
    }
    codes::OK
}

/// 图例 / 统计数组（重建缓存，指针在下次调用前稳定）。
#[no_mangle]
pub extern "C" fn cm_chart_legend(
    chart: *mut CrossStitchChart,
    out_ptr: *mut *const CmLegendEntry,
    out_len: *mut usize,
) -> c_int {
    if chart.is_null() || out_ptr.is_null() || out_len.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    let legend = chart.legend_slice();
    unsafe {
        *out_ptr = legend.as_ptr();
        *out_len = legend.len();
    }
    codes::OK
}

/// 符号数组（UTF-32 码点，重建缓存后返回稳定指针）。
/// 与 `cm_chart_legend` 一一对应，用于画布与图例显示实际分配到的符号。
#[no_mangle]
pub extern "C" fn cm_chart_symbols(
    chart: *mut CrossStitchChart,
    out_ptr: *mut *const u32,
    out_len: *mut usize,
) -> c_int {
    if chart.is_null() || out_ptr.is_null() || out_len.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &mut *chart };
    let symbols = chart.symbol_slice();
    unsafe {
        *out_ptr = symbols.as_ptr();
        *out_len = symbols.len();
    }
    codes::OK
}

/// 色号字符串（借用指针）。
#[no_mangle]
pub extern "C" fn cm_chart_color_code(
    chart: *const CrossStitchChart,
    index: usize,
) -> *const c_char {
    if chart.is_null() {
        return std::ptr::null();
    }
    unsafe { (&*chart).color_code(index) }
}

/// 色名字符串（借用指针）。
#[no_mangle]
pub extern "C" fn cm_chart_color_name(
    chart: *const CrossStitchChart,
    index: usize,
) -> *const c_char {
    if chart.is_null() {
        return std::ptr::null();
    }
    unsafe { (&*chart).color_name(index) }
}

/// 调色板颜色 RGB（写入 out_rgb[0..3]，返回 0 = 成功）。
#[no_mangle]
pub extern "C" fn cm_chart_color_rgb(
    chart: *const CrossStitchChart,
    index: usize,
    out_rgb: *mut u8,
) -> c_int {
    if chart.is_null() || out_rgb.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &*chart };
    match chart.palette.get(index) {
        Some(color) => {
            unsafe {
                *out_rgb.add(0) = color.rgb[0];
                *out_rgb.add(1) = color.rgb[1];
                *out_rgb.add(2) = color.rgb[2];
            }
            codes::OK
        }
        None => codes::INVALID_ARG,
    }
}

/// 符号字符（返回 Unicode 码位；与 util::SYMBOL_SEQUENCE 一致）。
#[no_mangle]
pub extern "C" fn cm_symbol_at(index: u32) -> u32 {
    common::util::symbol_at(index as usize) as u32
}

/// 总针数。
#[no_mangle]
pub extern "C" fn cm_chart_stitch_total(chart: *const CrossStitchChart) -> u32 {
    if chart.is_null() {
        return 0;
    }
    unsafe { (&*chart).total_stitches() }
}

/// 网格尺寸。
#[no_mangle]
pub extern "C" fn cm_chart_dimensions(
    chart: *const CrossStitchChart,
    out_w: *mut u32,
    out_h: *mut u32,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    let chart = unsafe { &*chart };
    if !out_w.is_null() {
        unsafe { *out_w = chart.width };
    }
    if !out_h.is_null() {
        unsafe { *out_h = chart.height };
    }
    codes::OK
}

/// 创建空图（指定网格尺寸，调色板为空；供空盘自由添加颜色后直接编辑）。
/// 返回空指针表示非法尺寸。
#[no_mangle]
pub extern "C" fn cm_cross_stitch_empty(
    width: u32,
    height: u32,
    params: *const CmCrossStitchParams,
    options: *const CmStitchOptions,
) -> *mut CrossStitchChart {
    if width == 0 || height == 0 || (width as usize).saturating_mul(height as usize) > 400_000_000 {
        return std::ptr::null_mut();
    }
    let params = if params.is_null() {
        CmCrossStitchParams::default()
    } else {
        unsafe { *params }
    };
    let options = if options.is_null() {
        CmStitchOptions::default()
    } else {
        unsafe { *options }
    };
    Box::into_raw(Box::new(CrossStitchChart::empty_with_options(
        width, height, params, options,
    )))
}

/// 调整图案网格尺寸（画布裁剪/扩展，保留图案内容，不缩放）。
/// anchor_left=1 左边固定（右边增减），=0 右边固定（左边增减）；
/// anchor_top 同理控制上/下边。
#[no_mangle]
pub extern "C" fn cm_chart_resize(
    chart: *mut CrossStitchChart,
    new_w: u32,
    new_h: u32,
    anchor_left: u32,
    anchor_top: u32,
) -> c_int {
    if chart.is_null() {
        return codes::NULL_PTR;
    }
    if new_w == 0 || new_h == 0 || (new_w as usize).saturating_mul(new_h as usize) > 400_000_000 {
        return codes::INVALID_ARG;
    }
    let chart = unsafe { &mut *chart };
    chart.resize(new_w, new_h, anchor_left != 0, anchor_top != 0);
    codes::OK
}

/// 设置单元格符号（画布编辑，需配合命令栈使用）。
#[no_mangle]
pub extern "C" fn cm_chart_set_symbol(
    chart: *mut CrossStitchChart,
    x: u32,
    y: u32,
    symbol_index: u32,
    err: *mut c_int,
) -> c_int {
    guard(err, || {
        if chart.is_null() {
            return Err(CoreError::NullPointer);
        }
        let chart = unsafe { &mut *chart };
        chart.set_symbol(x, y, symbol_index)
    })
    .map(|_| codes::OK)
    .unwrap_or(codes::INTERNAL)
}

/// 序列化图纸（cm_bytes_free 释放）。
#[no_mangle]
pub extern "C" fn cm_chart_to_bytes(
    chart: *const CrossStitchChart,
    out_ptr: *mut *mut u8,
    out_len: *mut usize,
    err: *mut c_int,
) -> c_int {
    guard(err, || {
        if chart.is_null() {
            return Err(CoreError::NullPointer);
        }
        let chart = unsafe { &*chart };
        let bytes = chart.to_bytes()?;
        let len = bytes.len();
        let ptr = Box::into_raw(bytes.into_boxed_slice()) as *mut u8;
        unsafe {
            *out_ptr = ptr;
            *out_len = len;
        }
        Ok(())
    })
    .map(|_| codes::OK)
    .unwrap_or(codes::INTERNAL)
}

/// 反序列化图纸。
#[no_mangle]
pub extern "C" fn cm_chart_from_bytes(
    ptr: *const u8,
    len: usize,
    err: *mut c_int,
) -> *mut CrossStitchChart {
    guard(err, || {
        let bytes = unsafe { slice::from_raw_parts(ptr, len) };
        let chart = CrossStitchChart::from_bytes(bytes)?;
        Ok(Box::into_raw(Box::new(chart)))
    })
    .unwrap_or(std::ptr::null_mut())
}

/// 释放 cm_chart_to_bytes 分配的内存。
#[no_mangle]
pub extern "C" fn cm_bytes_free(ptr: *mut u8, len: usize) {
    if ptr.is_null() || len == 0 {
        return;
    }
    unsafe {
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(ptr, len)));
    }
}

// ============================================================
// 纸样几何（Phase 0 Demo）
// ============================================================

/// 带缝份矩形裁片。
#[no_mangle]
pub extern "C" fn cm_pattern_seam_rect(
    width_mm: f64,
    height_mm: f64,
    seam_mm: f64,
    err: *mut c_int,
) -> *mut PiecePaths {
    guard(err, || {
        let (outline, seam) = pattern::geometry::seam_rect(width_mm, height_mm, seam_mm)?;
        Ok(Box::into_raw(Box::new(PiecePaths { outline, seam })))
    })
    .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn cm_paths_outline(
    paths: *const PiecePaths,
    out_ptr: *mut *const CmPoint,
    out_len: *mut usize,
) -> c_int {
    if paths.is_null() || out_ptr.is_null() || out_len.is_null() {
        return codes::NULL_PTR;
    }
    let paths = unsafe { &*paths };
    unsafe {
        *out_ptr = paths.outline.as_ptr();
        *out_len = paths.outline.len();
    }
    codes::OK
}

#[no_mangle]
pub extern "C" fn cm_paths_seam(
    paths: *const PiecePaths,
    out_ptr: *mut *const CmPoint,
    out_len: *mut usize,
) -> c_int {
    if paths.is_null() || out_ptr.is_null() || out_len.is_null() {
        return codes::NULL_PTR;
    }
    let paths = unsafe { &*paths };
    unsafe {
        *out_ptr = paths.seam.as_ptr();
        *out_len = paths.seam.len();
    }
    codes::OK
}

#[no_mangle]
pub extern "C" fn cm_paths_free(paths: *mut PiecePaths) {
    if !paths.is_null() {
        unsafe {
            drop(Box::from_raw(paths));
        }
    }
}

// ============================================================
// 历史栈（撤销 / 重做）
// ============================================================

#[no_mangle]
pub extern "C" fn cm_history_new(max_depth: usize) -> *mut History {
    Box::into_raw(Box::new(History::new(max_depth)))
}

#[no_mangle]
pub extern "C" fn cm_history_free(history: *mut History) {
    if !history.is_null() {
        unsafe {
            drop(Box::from_raw(history));
        }
    }
}

/// 记录初始状态。
#[no_mangle]
pub extern "C" fn cm_history_init(history: *mut History, state: *const u8, len: usize) -> c_int {
    if history.is_null() {
        return codes::NULL_PTR;
    }
    let history = unsafe { &mut *history };
    let state = unsafe { slice::from_raw_parts(state, len) };
    history.init(state.to_vec());
    codes::OK
}

/// 压入操作后的状态。label 为 i18n key 或调试名。
#[no_mangle]
pub extern "C" fn cm_history_push(
    history: *mut History,
    label: *const c_char,
    state: *const u8,
    len: usize,
) -> c_int {
    if history.is_null() {
        return codes::NULL_PTR;
    }
    let history = unsafe { &mut *history };
    let label = unsafe { cstr_to_str(label) };
    let state = unsafe { slice::from_raw_parts(state, len) };
    history.push(label, state.to_vec());
    codes::OK
}

/// 撤销：返回 1 并输出应恢复的状态（借用指针，直至下次变更）。
#[no_mangle]
pub extern "C" fn cm_history_undo(
    history: *mut History,
    out_ptr: *mut *const u8,
    out_len: *mut usize,
) -> c_int {
    if history.is_null() {
        return codes::NULL_PTR;
    }
    let history = unsafe { &mut *history };
    match history.undo() {
        Some(state) => {
            unsafe {
                *out_ptr = state.as_ptr();
                *out_len = state.len();
            }
            1
        }
        None => 0,
    }
}

/// 重做：返回 1 并输出应恢复的状态。
#[no_mangle]
pub extern "C" fn cm_history_redo(
    history: *mut History,
    out_ptr: *mut *const u8,
    out_len: *mut usize,
) -> c_int {
    if history.is_null() {
        return codes::NULL_PTR;
    }
    let history = unsafe { &mut *history };
    match history.redo() {
        Some(state) => {
            unsafe {
                *out_ptr = state.as_ptr();
                *out_len = state.len();
            }
            1
        }
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn cm_history_can_undo(history: *mut History) -> c_int {
    if history.is_null() {
        return -1;
    }
    unsafe { (&*history).can_undo() as c_int }
}

#[no_mangle]
pub extern "C" fn cm_history_can_redo(history: *mut History) -> c_int {
    if history.is_null() {
        return -1;
    }
    unsafe { (&*history).can_redo() as c_int }
}

/// 当前状态（借用指针）。
#[no_mangle]
pub extern "C" fn cm_history_current(
    history: *mut History,
    out_ptr: *mut *const u8,
    out_len: *mut usize,
) -> c_int {
    if history.is_null() {
        return codes::NULL_PTR;
    }
    let history = unsafe { &*history };
    match history.current() {
        Some(state) => {
            unsafe {
                *out_ptr = state.as_ptr();
                *out_len = state.len();
            }
            1
        }
        None => 0,
    }
}

#[no_mangle]
pub extern "C" fn cm_history_clear(history: *mut History) -> c_int {
    if history.is_null() {
        return codes::NULL_PTR;
    }
    unsafe { (&mut *history).clear() };
    codes::OK
}

// ============================================================
// 导出
// ============================================================

#[no_mangle]
pub extern "C" fn cm_export_csv(
    chart: *const CrossStitchChart,
    path: *const c_char,
    err: *mut c_int,
) -> c_int {
    guard(err, || {
        if chart.is_null() {
            return Err(CoreError::NullPointer);
        }
        let chart = unsafe { &*chart };
        let path = unsafe { cstr_to_path(path)? };
        cross_stitch::export::export_csv(chart, path)
    })
    .map(|_| codes::OK)
    .unwrap_or(codes::INTERNAL)
}

#[no_mangle]
pub extern "C" fn cm_export_png(
    chart: *const CrossStitchChart,
    path: *const c_char,
    cell_px: u32,
    err: *mut c_int,
) -> c_int {
    guard(err, || {
        if chart.is_null() {
            return Err(CoreError::NullPointer);
        }
        let chart = unsafe { &*chart };
        let path = unsafe { cstr_to_path(path)? };
        cross_stitch::export::export_png(chart, path, cell_px)
    })
    .map(|_| codes::OK)
    .unwrap_or(codes::INTERNAL)
}

#[no_mangle]
pub extern "C" fn cm_export_pdf(
    chart: *const CrossStitchChart,
    path: *const c_char,
    title: *const c_char,
    err: *mut c_int,
) -> c_int {
    guard(err, || {
        if chart.is_null() {
            return Err(CoreError::NullPointer);
        }
        let chart = unsafe { &*chart };
        let path = unsafe { cstr_to_path(path)? };
        let title = unsafe { cstr_to_str(title) };
        export::pdf::export_chart_pdf(chart, path, title)
    })
    .map(|_| codes::OK)
    .unwrap_or(codes::INTERNAL)
}

#[no_mangle]
pub extern "C" fn cm_export_dxf_piece(
    paths: *const PiecePaths,
    path: *const c_char,
    err: *mut c_int,
) -> c_int {
    guard(err, || {
        if paths.is_null() {
            return Err(CoreError::NullPointer);
        }
        let paths = unsafe { &*paths };
        let path = unsafe { cstr_to_path(path)? };
        export::dxf::export_piece_paths(&paths.outline, &paths.seam, path)
    })
    .map(|_| codes::OK)
    .unwrap_or(codes::INTERNAL)
}

// ============================================================
// 工程文件（.bean）
// ============================================================

#[no_mangle]
pub extern "C" fn cm_project_save(
    chart: *const CrossStitchChart,
    path: *const c_char,
    err: *mut c_int,
) -> c_int {
    guard(err, || {
        if chart.is_null() {
            return Err(CoreError::NullPointer);
        }
        let chart = unsafe { &*chart };
        let path = unsafe { cstr_to_path(path)? };
        export::project::save_project(chart, path)
    })
    .map(|_| codes::OK)
    .unwrap_or(codes::INTERNAL)
}

#[no_mangle]
pub extern "C" fn cm_project_open(path: *const c_char, err: *mut c_int) -> *mut CrossStitchChart {
    guard(err, || {
        let path = unsafe { cstr_to_path(path)? };
        let chart = export::project::open_project(path)?;
        Ok(Box::into_raw(Box::new(chart)))
    })
    .unwrap_or(std::ptr::null_mut())
}

// ============================================================
// 素材库
// ============================================================

fn fill_dmc_ffi(out: &mut CmDmcColor, rgb: [u8; 3], code: &str, name: &str) {
    out.rgb = rgb;
    for slot in out.code.iter_mut() {
        *slot = 0;
    }
    for slot in out.name.iter_mut() {
        *slot = 0;
    }
    for (i, ch) in code.bytes().take(out.code.len() - 1).enumerate() {
        out.code[i] = ch as c_char;
    }
    for (i, ch) in name.bytes().take(out.name.len() - 1).enumerate() {
        out.name[i] = ch as c_char;
    }
}

#[no_mangle]
pub extern "C" fn cm_material_db_new() -> *mut MaterialDb {
    match MaterialDb::open_in_memory() {
        Ok(db) => Box::into_raw(Box::new(db)),
        Err(e) => {
            tracing::error!("material db init failed: {e}");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn cm_material_db_free(db: *mut MaterialDb) {
    if !db.is_null() {
        unsafe {
            drop(Box::from_raw(db));
        }
    }
}

/// 按色号查询。返回 1=找到，0=未找到，负数=错误。
#[no_mangle]
pub extern "C" fn cm_material_find_dmc(
    db: *mut MaterialDb,
    code: *const c_char,
    out: *mut CmDmcColor,
) -> c_int {
    if db.is_null() || out.is_null() {
        return codes::NULL_PTR;
    }
    let db = unsafe { &*db };
    let code = unsafe { cstr_to_str(code) };
    match db.find_by_code(code) {
        Ok(Some(m)) => {
            let rgb = m.rgb.unwrap_or([0, 0, 0]);
            unsafe { fill_dmc_ffi(&mut *out, rgb, &m.code, &m.name) };
            1
        }
        Ok(None) => 0,
        Err(e) => e.code(),
    }
}

/// 某大类素材数量（kind: fabric/yarn/leather/button/zipper/bead）。负数=错误。
#[no_mangle]
pub extern "C" fn cm_material_count(db: *mut MaterialDb, kind: *const c_char) -> c_int {
    if db.is_null() {
        return codes::NULL_PTR;
    }
    let db = unsafe { &*db };
    let kind = unsafe { cstr_to_str(kind) };
    let kind = match MaterialKind::from_str_opt(kind) {
        Some(k) => k,
        None => return codes::INVALID_ARG,
    };
    match db.count(kind) {
        Ok(n) => n as c_int,
        Err(e) => e.code(),
    }
}

/// 枚举某大类全部素材到调用方缓冲区。返回总数；out 为 null 时仅返回总数（用于探测容量）。
#[no_mangle]
pub extern "C" fn cm_material_all(
    db: *mut MaterialDb,
    kind: *const c_char,
    out: *mut CmDmcColor,
    out_cap: usize,
) -> c_int {
    if db.is_null() {
        return codes::NULL_PTR;
    }
    let db = unsafe { &*db };
    let kind = unsafe { cstr_to_str(kind) };
    let kind = match MaterialKind::from_str_opt(kind) {
        Some(k) => k,
        None => return codes::INVALID_ARG,
    };
    let materials = match db.list(kind) {
        Ok(m) => m,
        Err(_) => return codes::INTERNAL,
    };
    let total = materials.len() as c_int;
    if out.is_null() {
        return total;
    }
    let to_copy = std::cmp::min(materials.len(), out_cap);
    let slice = unsafe { slice::from_raw_parts_mut(out, to_copy) };
    for (i, m) in materials.iter().take(to_copy).enumerate() {
        let rgb = m.rgb.unwrap_or([0, 0, 0]);
        fill_dmc_ffi(&mut slice[i], rgb, &m.code, &m.name);
    }
    total
}

// ============================================================
// Rust 侧单元测试（FFI 冒烟）
// ============================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn test_rgba(w: u32, h: u32, f: impl Fn(u32, u32) -> [u8; 4]) -> Vec<u8> {
        let mut v = Vec::new();
        for y in 0..h {
            for x in 0..w {
                v.extend_from_slice(&f(x, y));
            }
        }
        v
    }

    #[test]
    fn ffi_generate_and_access() {
        let rgba = test_rgba(16, 16, |x, _| {
            if x < 8 {
                [255, 0, 0, 255]
            } else {
                [0, 0, 255, 255]
            }
        });
        let params = CmCrossStitchParams {
            grid_width: 8,
            grid_height: 8,
            max_colors: 8,
            ..Default::default()
        };
        let mut err: c_int = -1;
        let chart = cm_cross_stitch_generate(
            rgba.as_ptr(),
            rgba.len(),
            16,
            16,
            &params,
            std::ptr::null(),
            None,
            std::ptr::null_mut(),
            &mut err,
        );
        assert!(!chart.is_null());
        assert_eq!(err, codes::OK);

        let mut ptr: *const CmGridCell = std::ptr::null();
        let mut len: usize = 0;
        assert_eq!(cm_chart_cells(chart, &mut ptr, &mut len), codes::OK);
        assert_eq!(len, 64);
        assert!(!ptr.is_null());
        assert_eq!(cm_chart_stitch_total(chart), 64);

        let mut w = 0u32;
        let mut h = 0u32;
        cm_chart_dimensions(chart, &mut w, &mut h);
        assert_eq!((w, h), (8, 8));

        // 序列化 roundtrip
        let mut bptr: *mut u8 = std::ptr::null_mut();
        let mut blen = 0usize;
        assert_eq!(
            cm_chart_to_bytes(chart, &mut bptr, &mut blen, &mut err),
            codes::OK
        );
        let restored = cm_chart_from_bytes(bptr, blen, &mut err);
        assert!(!restored.is_null());
        cm_bytes_free(bptr, blen);
        cm_chart_free(restored);
        cm_chart_free(chart);
    }

    #[test]
    fn ffi_progress_callback_fires() {
        use std::sync::atomic::{AtomicU32, Ordering};
        static STAGES: AtomicU32 = AtomicU32::new(0);

        extern "C" fn cb(_ud: *mut c_void, _p: f32, stage: i32) {
            if stage >= 0 {
                STAGES.fetch_add(1, Ordering::SeqCst);
            }
        }

        let rgba = test_rgba(8, 8, |_, _| [10, 200, 10, 255]);
        let params = CmCrossStitchParams {
            grid_width: 4,
            grid_height: 4,
            max_colors: 4,
            ..Default::default()
        };
        let mut err: c_int = -1;
        let chart = cm_cross_stitch_generate(
            rgba.as_ptr(),
            rgba.len(),
            8,
            8,
            &params,
            std::ptr::null(),
            Some(cb),
            std::ptr::null_mut(),
            &mut err,
        );
        assert!(!chart.is_null());
        assert!(
            STAGES.load(Ordering::SeqCst) >= 4,
            "progress stages reported"
        );
        cm_chart_free(chart);
    }

    #[test]
    fn ffi_seam_rect_and_dxf() {
        let mut err: c_int = -1;
        let paths = cm_pattern_seam_rect(100.0, 50.0, 10.0, &mut err);
        assert!(!paths.is_null());
        let mut optr: *const CmPoint = std::ptr::null();
        let mut olen = 0usize;
        assert_eq!(cm_paths_outline(paths, &mut optr, &mut olen), codes::OK);
        assert_eq!(olen, 4);

        let dir = std::env::temp_dir();
        let path = dir.join("craftmaster_ffi_test.dxf");
        let cpath = CString::new(path.to_str().unwrap()).unwrap();
        assert_eq!(
            cm_export_dxf_piece(paths, cpath.as_ptr(), &mut err),
            codes::OK
        );
        assert!(path.exists());
        std::fs::remove_file(&path).ok();
        cm_paths_free(paths);
    }

    #[test]
    fn ffi_history_flow() {
        let h = cm_history_new(10);
        assert!(!h.is_null());
        let s0: &[u8] = &[0];
        let s1: &[u8] = &[1];
        cm_history_init(h, s0.as_ptr(), s0.len());
        assert_eq!(cm_history_can_undo(h), 0);
        let label = CString::new("op1").unwrap();
        cm_history_push(h, label.as_ptr(), s1.as_ptr(), s1.len());
        assert_eq!(cm_history_can_undo(h), 1);

        let mut ptr: *const u8 = std::ptr::null();
        let mut len = 0usize;
        assert_eq!(cm_history_undo(h, &mut ptr, &mut len), 1);
        let restored = unsafe { slice::from_raw_parts(ptr, len) };
        assert_eq!(restored, &[0]);

        assert_eq!(cm_history_redo(h, &mut ptr, &mut len), 1);
        let restored = unsafe { slice::from_raw_parts(ptr, len) };
        assert_eq!(restored, &[1]);
        cm_history_free(h);
    }

    #[test]
    fn ffi_project_save_open() {
        let rgba = test_rgba(8, 8, |x, _| {
            if x < 4 {
                [255, 0, 0, 255]
            } else {
                [0, 0, 255, 255]
            }
        });
        let params = CmCrossStitchParams {
            grid_width: 4,
            grid_height: 4,
            max_colors: 8,
            ..Default::default()
        };
        let mut err: c_int = -1;
        let chart = cm_cross_stitch_generate(
            rgba.as_ptr(),
            rgba.len(),
            8,
            8,
            &params,
            std::ptr::null(),
            None,
            std::ptr::null_mut(),
            &mut err,
        );
        assert!(!chart.is_null());

        let dir = std::env::temp_dir();
        let path = dir.join("beandraw_ffi_test.bean");
        let cpath = CString::new(path.to_str().unwrap()).unwrap();
        assert_eq!(cm_project_save(chart, cpath.as_ptr(), &mut err), codes::OK);

        let opened = cm_project_open(cpath.as_ptr(), &mut err);
        assert!(!opened.is_null());
        assert_eq!(cm_chart_stitch_total(opened), 16);
        cm_chart_free(opened);
        cm_chart_free(chart);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn ffi_modules_list() {
        let mut len = 0usize;
        let infos = cm_modules_list(&mut len);
        assert!(!infos.is_null());
        assert_eq!(len, 2);
        let infos = unsafe { slice::from_raw_parts(infos, len) };
        assert_eq!(unsafe { cstr_to_str(infos[0].id) }, "cross_stitch");
        assert_eq!(unsafe { cstr_to_str(infos[1].id) }, "free_pattern");
        // version 非空
        assert!(!unsafe { cstr_to_str(infos[0].version) }.is_empty());
    }

    #[test]
    fn ffi_material_db() {
        let db = cm_material_db_new();
        assert!(!db.is_null());
        let code = CString::new("321").unwrap();
        let mut out: CmDmcColor = unsafe { std::mem::zeroed() };
        assert_eq!(cm_material_find_dmc(db, code.as_ptr(), &mut out), 1);
        assert_eq!(out.rgb, [199, 15, 46]);
        let code_str =
            unsafe { std::slice::from_raw_parts(out.code.as_ptr() as *const u8, out.code.len()) };
        assert!(code_str.starts_with(b"321"));

        let yarn = CString::new("yarn").unwrap();
        assert!(cm_material_count(db, yarn.as_ptr()) >= 90);
        let bad = CString::new("nope").unwrap();
        assert_eq!(cm_material_count(db, bad.as_ptr()), codes::INVALID_ARG);
        cm_material_db_free(db);
    }

    #[test]
    fn ffi_null_safety() {
        // 全空指针不应崩溃
        assert_eq!(
            cm_chart_cells(
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut()
            ),
            codes::NULL_PTR
        );
        cm_chart_free(std::ptr::null_mut());
        cm_paths_free(std::ptr::null_mut());
        cm_history_free(std::ptr::null_mut());
        cm_material_db_free(std::ptr::null_mut());
        cm_bytes_free(std::ptr::null_mut(), 0);
        assert_eq!(cm_history_can_undo(std::ptr::null_mut()), -1);
    }
}
