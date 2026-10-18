//! 十字绣模块（含珠绣子模式）。
//!
//! 流水线（开发大纲 Phase 1 + 参考 Cstitch / KXStitch 的算法取舍）：
//!
//! ```text
//! 源图 → 下采样(盒式/众数/中心色) → KMeans++ 量化 → DMC Lab 最近匹配
//!      → 去杂点 → 稀有色合并 → 按用量分配符号 → 网格 + 图例统计
//! ```
//!
//! 算法选型说明：
//! - 下采样不用纯盒式平均（会造出原图与色卡中不存在的中间色），默认走众数归并
//! - 符号「按用量降序分配」：绣制时查得最多的符号最好认（Cstitch symbolChooser 思路）

pub mod color_quant;
pub mod dmc_data;
pub mod export;
pub mod grid_gen;
pub mod image_pixel;
pub mod perler_data;
pub mod post_process;

/// DMC 色卡条目（色号 / 名称 / RGB）。
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DmcColor {
    pub code: &'static str,
    pub name: &'static str,
    pub rgb: [u8; 3],
}

use crate::common::error::{CoreError, CoreResult};
use crate::common::ffi_types::*;
use crate::common::progress::{stage, Progress};
use grid_gen::{build_cells, legend, total_stitches};
use post_process::{
    color_histogram, flood_fill, merge_rare_colors, paint_cell, rect_fill, remove_isolated,
    replace_color,
};
use std::ffi::CString;

/// 十字绣工程数据（Rust 侧持有，通过 FFI 句柄暴露）。
#[derive(Debug)]
pub struct CrossStitchChart {
    pub params: CmCrossStitchParams,
    /// 高级生成选项（符号集 / 归并策略 / 后处理强度）
    pub options: CmStitchOptions,
    pub width: u32,
    pub height: u32,
    pub cells: Vec<CmGridCell>,
    pub palette: Vec<DmcColor>,
    /// 与 palette 一一对应的符号
    pub symbols: Vec<char>,
    /// 源图名称（用于工程记录）
    pub source_name: String,
    /// FFI 字符串缓存（codes/names，生命周期跟随 chart）
    cstrings: Vec<CString>,
    /// 图例缓存（保证 FFI 返回指针在下次调用前稳定）
    legend_cache: Vec<CmLegendEntry>,
    /// 符号缓存（UTF-32 码点，与 symbols 同步）
    symbol_cache: Vec<u32>,
}

impl CrossStitchChart {
    /// 空工程（新建 / 测试用）。
    pub fn empty(width: u32, height: u32, params: CmCrossStitchParams) -> Self {
        Self::empty_with_options(width, height, params, CmStitchOptions::default())
    }

    /// 空工程（指定高级选项）。
    pub fn empty_with_options(
        width: u32,
        height: u32,
        params: CmCrossStitchParams,
        options: CmStitchOptions,
    ) -> Self {
        let cells = vec![
            CmGridCell {
                x: 0,
                y: 0,
                color_index: COLOR_NONE,
                symbol_index: 0,
            };
            (width * height) as usize
        ];
        let mut chart = Self {
            params,
            options,
            width,
            height,
            cells,
            palette: Vec::new(),
            symbols: Vec::new(),
            source_name: String::new(),
            cstrings: Vec::new(),
            legend_cache: Vec::new(),
            symbol_cache: Vec::new(),
        };
        chart.sync_cstrings();
        chart
    }

    fn sync_cstrings(&mut self) {
        self.cstrings.clear();
        for c in &self.palette {
            self.cstrings.push(CString::new(c.code).unwrap_or_default());
        }
        for c in &self.palette {
            self.cstrings.push(CString::new(c.name).unwrap_or_default());
        }
    }

    /// 调整网格尺寸（画布裁剪 / 扩展，不缩放图案）：
    /// - 图案生成后固定，内容原样保留；
    /// - 变小的边裁掉对应格，变大的边补空白格（color_index = COLOR_NONE）；
    /// - `anchor_left` = 左边固定（右增减），`anchor_top` = 上边固定（下增减）。
    ///
    /// 方向取反则镜像：右边固定（左增减）、下边固定（上增减）。
    pub fn resize(&mut self, new_w: u32, new_h: u32, anchor_left: bool, anchor_top: bool) {
        let (ow, oh) = (self.width, self.height);
        let ox: i64 = if anchor_left {
            0
        } else {
            ow as i64 - new_w as i64
        };
        let oy: i64 = if anchor_top {
            0
        } else {
            oh as i64 - new_h as i64
        };
        let mut new_cells = Vec::with_capacity((new_w as usize).saturating_mul(new_h as usize));
        for y in 0..new_h {
            for x in 0..new_w {
                let sx = x as i64 - ox;
                let sy = y as i64 - oy;
                let cell = if sx >= 0 && sy >= 0 && sx < ow as i64 && sy < oh as i64 {
                    let old = self.cells[sy as usize * ow as usize + sx as usize];
                    CmGridCell {
                        x,
                        y,
                        color_index: old.color_index,
                        symbol_index: old.symbol_index,
                    }
                } else {
                    CmGridCell {
                        x,
                        y,
                        color_index: COLOR_NONE,
                        symbol_index: 0,
                    }
                };
                new_cells.push(cell);
            }
        }
        self.width = new_w;
        self.height = new_h;
        self.cells = new_cells;
    }

    /// 图例 / 统计条目。
    pub fn legend(&self) -> Vec<CmLegendEntry> {
        legend(self)
    }

    /// 图例切片（FFI 用，重建缓存后返回稳定指针）。
    pub fn legend_slice(&mut self) -> &[CmLegendEntry] {
        self.legend_cache = legend(self);
        &self.legend_cache
    }

    /// 符号切片（UTF-32 码点；FFI 用，重建缓存后返回稳定指针）。
    pub fn symbol_slice(&mut self) -> &[u32] {
        self.symbol_cache = self.symbols.iter().map(|c| *c as u32).collect();
        &self.symbol_cache
    }

    /// 总针数。
    pub fn total_stitches(&self) -> u32 {
        total_stitches(self)
    }

    /// 成品尺寸（由网格与面料计数换算）。
    pub fn fabric_size(&self) -> CmFabricSize {
        grid_gen::fabric_size(self.width, self.height, self.params.fabric_count)
    }

    /// 网格数组指针与长度（FFI 数组传输）。
    pub fn cells_slice(&self) -> (*const CmGridCell, usize) {
        (self.cells.as_ptr(), self.cells.len())
    }

    /// 色号字符串（index 越界返回空串指针，非空保证）。
    pub fn color_code(&self, index: usize) -> *const std::os::raw::c_char {
        self.cstrings
            .get(index)
            .map(|s| s.as_ptr())
            .unwrap_or_else(|| EMPTY_STR.as_ptr() as *const std::os::raw::c_char)
    }

    /// 色名文本（i18n key 由 Qt 侧决定是否翻译，此处为原始英文名）。
    pub fn color_name(&self, index: usize) -> *const std::os::raw::c_char {
        self.cstrings
            .get(self.palette.len() + index)
            .map(|s| s.as_ptr())
            .unwrap_or_else(|| EMPTY_STR.as_ptr() as *const std::os::raw::c_char)
    }

    // ------------------------------------------------------------
    // 编辑接口（画布工具 → 命令栈 → 内核）
    // ------------------------------------------------------------

    fn check_cell(&self, x: u32, y: u32) -> CoreResult<usize> {
        if x >= self.width || y >= self.height {
            return Err(CoreError::InvalidArg(format!(
                "cell ({x},{y}) out of grid {}x{}",
                self.width, self.height
            )));
        }
        Ok((y * self.width + x) as usize)
    }

    fn check_color(&self, color_index: u32) -> CoreResult<()> {
        if color_index != COLOR_NONE && color_index as usize >= self.palette.len() {
            return Err(CoreError::InvalidArg(format!(
                "color index {color_index} out of palette ({} colors)",
                self.palette.len()
            )));
        }
        Ok(())
    }

    /// 单格涂色（绣笔）。`color_index` 为 COLOR_NONE 表示擦除。
    pub fn paint_cell(&mut self, x: u32, y: u32, color_index: u32) -> CoreResult<u32> {
        self.check_cell(x, y)?;
        self.check_color(color_index)?;
        Ok(paint_cell(
            &mut self.cells,
            self.width,
            self.height,
            x,
            y,
            color_index,
        ))
    }

    /// 区域填充（油漆桶）：4-连通同色区域替换。
    pub fn flood_fill(&mut self, x: u32, y: u32, color_index: u32) -> CoreResult<u32> {
        self.check_cell(x, y)?;
        self.check_color(color_index)?;
        Ok(flood_fill(
            &mut self.cells,
            self.width,
            self.height,
            x,
            y,
            color_index,
        ))
    }

    /// 矩形填充。
    pub fn rect_fill(
        &mut self,
        x0: u32,
        y0: u32,
        x1: u32,
        y1: u32,
        color_index: u32,
    ) -> CoreResult<u32> {
        self.check_color(color_index)?;
        Ok(rect_fill(
            &mut self.cells,
            self.width,
            self.height,
            x0,
            y0,
            x1,
            y1,
            color_index,
        ))
    }

    /// 批量填色：按坐标列表逐格设置颜色（圆形 / 椭圆 / 多边形填充共用）。
    ///
    /// 坐标越界跳过，返回实际改变的格数（与已有值相同则不计）。
    pub fn fill_cells(&mut self, points: &[(u32, u32)], color_index: u32) -> CoreResult<u32> {
        self.check_color(color_index)?;
        let mut changed = 0u32;
        for &(x, y) in points {
            if x >= self.width || y >= self.height {
                continue;
            }
            let idx = (y * self.width + x) as usize;
            if self.cells[idx].color_index != color_index {
                self.cells[idx].color_index = color_index;
                changed += 1;
            }
        }
        Ok(changed)
    }

    /// 区域镜像（闭区间）：horizontal=true 左右镜像，false 上下镜像。
    ///
    /// 区域坐标越界返回 0（无改动）；返回镜像后发生变化（与源不同）的格数。
    pub fn mirror_region(&mut self, x0: u32, y0: u32, x1: u32, y1: u32, horizontal: bool) -> u32 {
        let w = self.width as usize;
        if x1 >= self.width || y1 >= self.height || x1 < x0 || y1 < y0 {
            return 0;
        }
        let rw = (x1 - x0 + 1) as usize;
        let rh = (y1 - y0 + 1) as usize;
        // 先拷贝区域快照，再按镜像坐标回写（避免同区读写互相污染）
        let src: Vec<CmGridCell> = self
            .cells
            .chunks(w)
            .skip(y0 as usize)
            .take(rh)
            .flat_map(|row| row.iter().skip(x0 as usize).take(rw).cloned())
            .collect();
        let mut changed = 0u32;
        for dy in 0..rh {
            for dx in 0..rw {
                let sx = if horizontal { rw - 1 - dx } else { dx };
                let sy = if !horizontal { rh - 1 - dy } else { dy };
                let cell = &src[sy * rw + sx];
                let dst = &mut self.cells[(y0 as usize + dy) * w + (x0 as usize + dx)];
                if dst.color_index != cell.color_index {
                    changed += 1;
                }
                *dst = *cell;
            }
        }
        changed
    }

    /// 4-连通同色区域识别：返回与 (x,y) 颜色相同的相连区域格子索引列表
    /// （索引 = y*width + x，顺序不定）。空格子（未着色）返回空列表；坐标越界返回 None。
    pub fn flood_region(&self, x: u32, y: u32) -> Option<Vec<usize>> {
        if x >= self.width || y >= self.height {
            return None;
        }
        let w = self.width as usize;
        let h = self.height as usize;
        let idx = (y as usize) * w + (x as usize);
        let target = self.cells[idx].color_index;
        if target == u32::MAX {
            return Some(Vec::new());
        }
        let mut visited = vec![false; w * h];
        let mut out = Vec::new();
        let mut stack = vec![idx];
        visited[idx] = true;
        while let Some(i) = stack.pop() {
            out.push(i);
            let cx = i % w;
            let cy = i / w;
            if cx > 0 {
                let ni = i - 1;
                if !visited[ni] && self.cells[ni].color_index == target {
                    visited[ni] = true;
                    stack.push(ni);
                }
            }
            if cx + 1 < w {
                let ni = i + 1;
                if !visited[ni] && self.cells[ni].color_index == target {
                    visited[ni] = true;
                    stack.push(ni);
                }
            }
            if cy > 0 {
                let ni = i - w;
                if !visited[ni] && self.cells[ni].color_index == target {
                    visited[ni] = true;
                    stack.push(ni);
                }
            }
            if cy + 1 < h {
                let ni = i + w;
                if !visited[ni] && self.cells[ni].color_index == target {
                    visited[ni] = true;
                    stack.push(ni);
                }
            }
        }
        Some(out)
    }

    /// 任意直线轴镜像（闭区间）：轴由两点 (ax0,ay0)-(ax1,ay1) 定义（格子单位，
    /// 可传半格坐标，如竖格线 x=k 用 (k,0)-(k,1)，横格线 y=k 用 (0,k)-(1,k)）。
    ///
    /// 对选区内每个格子中心求关于直线的镜像点，最近邻映射回图纸；
    /// 目标格越界跳过。返回实际写入（与源不同）的格数。
    #[allow(clippy::too_many_arguments)]
    pub fn mirror_region_axis(
        &mut self,
        x0: u32,
        y0: u32,
        x1: u32,
        y1: u32,
        ax0: f64,
        ay0: f64,
        ax1: f64,
        ay1: f64,
    ) -> CoreResult<u32> {
        if x1 >= self.width || y1 >= self.height || x1 < x0 || y1 < y0 {
            return Err(CoreError::InvalidArg(
                "mirror region out of bounds".to_string(),
            ));
        }
        let (dx, dy) = (ax1 - ax0, ay1 - ay0);
        let len2 = dx * dx + dy * dy;
        if len2 < f64::EPSILON {
            return Err(CoreError::InvalidArg("mirror axis degenerate".to_string()));
        }
        let w = self.width as usize;
        let rw = (x1 - x0 + 1) as usize;
        let rh = (y1 - y0 + 1) as usize;
        // 先拷贝区域快照，再按镜像坐标回写（避免同区读写互相污染）
        let src: Vec<CmGridCell> = self
            .cells
            .chunks(w)
            .skip(y0 as usize)
            .take(rh)
            .flat_map(|row| row.iter().skip(x0 as usize).take(rw).cloned())
            .collect();
        let mut changed = 0u32;
        for dyi in 0..rh {
            for dxi in 0..rw {
                // 源格中心（格 (x,y) 覆盖 [x,x+1)，中心 = (x+0.5, y+0.5)）
                let cx = x0 as f64 + dxi as f64 + 0.5;
                let cy = y0 as f64 + dyi as f64 + 0.5;
                // 关于直线 (P0, d) 的镜像：P' = 2*proj - P
                let t = ((cx - ax0) * dx + (cy - ay0) * dy) / len2;
                let mx = 2.0 * (ax0 + t * dx) - cx;
                let my = 2.0 * (ay0 + t * dy) - cy;
                // 目标格坐标：镜像点坐标直接 floor（格 [g,g+1) 中心 g+0.5）
                let gx = mx.floor() as i64;
                let gy = my.floor() as i64;
                if gx < 0 || gy < 0 || gx >= self.width as i64 || gy >= self.height as i64 {
                    continue;
                }
                let cell = &src[dyi * rw + dxi];
                let dst = &mut self.cells[(gy as usize) * w + (gx as usize)];
                if dst.color_index != cell.color_index {
                    changed += 1;
                }
                *dst = *cell;
            }
        }
        Ok(changed)
    }

    /// 区域缩放（最近邻重采样）：把闭区间 [x0,y0,x1,y1] 的内容按
    /// `new_w × new_h` 重采样，写回以 (dst_x0, dst_y0) 为左上角的新区域。
    ///
    /// 新尺寸由选区控制柄拖动决定，可大于 / 小于原区域；
    /// 超出图纸边界的格子跳过。返回实际改变的格数。
    #[allow(clippy::too_many_arguments)]
    pub fn scale_region(
        &mut self,
        x0: u32,
        y0: u32,
        x1: u32,
        y1: u32,
        dst_x0: u32,
        dst_y0: u32,
        new_w: u32,
        new_h: u32,
    ) -> CoreResult<u32> {
        if new_w == 0 || new_h == 0 {
            return Err(CoreError::InvalidArg(
                "scale region size must be > 0".to_string(),
            ));
        }
        if x1 >= self.width || y1 >= self.height || x1 < x0 || y1 < y0 {
            return Ok(0);
        }
        let w = self.width as usize;
        let rw = (x1 - x0 + 1) as usize;
        let rh = (y1 - y0 + 1) as usize;
        let src: Vec<CmGridCell> = self
            .cells
            .chunks(w)
            .skip(y0 as usize)
            .take(rh)
            .flat_map(|row| row.iter().skip(x0 as usize).take(rw).cloned())
            .collect();
        let mut changed = 0u32;
        for dy in 0..new_h as usize {
            let sy = ((dy * rh) / (new_h as usize)).min(rh - 1);
            let ty = dst_y0 as usize + dy;
            if ty >= self.height as usize {
                continue;
            }
            for dx in 0..new_w as usize {
                let sx = ((dx * rw) / (new_w as usize)).min(rw - 1);
                let tx = dst_x0 as usize + dx;
                if tx >= w {
                    continue;
                }
                let cell = &src[sy * rw + sx];
                let dst = &mut self.cells[ty * w + tx];
                if dst.color_index != cell.color_index {
                    changed += 1;
                }
                *dst = *cell;
            }
        }
        Ok(changed)
    }

    /// 移动选区内容（剪切平移）：把闭区间 [x0,y0,x1,y1] 的内容平移到
    /// (x0+dx, y0+dy)，原位置清空（移动 = 剪切粘贴，不缩放、不重采样）。
    ///
    /// dx/dy 为格子偏移，可正可负；目标越界的格子跳过。
    /// 返回实际变化的格数（含清除与写入）。
    pub fn move_region(
        &mut self,
        x0: u32,
        y0: u32,
        x1: u32,
        y1: u32,
        dx: i32,
        dy: i32,
    ) -> CoreResult<u32> {
        if x1 >= self.width || y1 >= self.height || x1 < x0 || y1 < y0 {
            return Ok(0);
        }
        let w = self.width as i64;
        let h = self.height as i64;
        let rw = (x1 - x0 + 1) as usize;
        let rh = (y1 - y0 + 1) as usize;
        // 先快照源区域，再清空 + 平移写入（避免重叠移动时读写互相污染）
        let src: Vec<CmGridCell> = self
            .cells
            .chunks(self.width as usize)
            .skip(y0 as usize)
            .take(rh)
            .flat_map(|row| row.iter().skip(x0 as usize).take(rw).cloned())
            .collect();
        let mut changed = 0u32;
        // 1) 清空源区域
        for dy in 0..rh {
            for dx in 0..rw {
                let cell =
                    &mut self.cells[(y0 as usize + dy) * self.width as usize + (x0 as usize + dx)];
                if cell.color_index != COLOR_NONE {
                    cell.color_index = COLOR_NONE;
                    changed += 1;
                }
            }
        }
        // 2) 按偏移写入目标（越界跳过）
        let (ox, oy) = (x0 as i64 + dx as i64, y0 as i64 + dy as i64);
        for dy in 0..rh {
            for dx in 0..rw {
                let tx = ox + dx as i64;
                let ty = oy + dy as i64;
                if tx < 0 || ty < 0 || tx >= w || ty >= h {
                    continue;
                }
                let cell = &src[dy * rw + dx];
                let dst = &mut self.cells[(ty as usize) * self.width as usize + (tx as usize)];
                if dst.color_index != cell.color_index {
                    changed += 1;
                }
                *dst = *cell;
            }
        }
        Ok(changed)
    }

    /// 全局换色。
    pub fn replace_color(&mut self, from: u32, to: u32) -> CoreResult<u32> {
        self.check_color(from)?;
        self.check_color(to)?;
        Ok(replace_color(&mut self.cells, from, to))
    }

    /// 删除颜色：该色所有针清空（变回空格），并从调色板中移除。
    ///
    /// 与「合并」不同：合并把针转给目标色，删除直接把针抹掉。
    /// 返回被清空的针数。
    pub fn delete_color(&mut self, color_index: u32) -> CoreResult<u32> {
        self.check_color(color_index)?;
        let mut cleared = 0u32;
        for c in self.cells.iter_mut() {
            if c.color_index == color_index {
                c.color_index = COLOR_NONE;
                c.symbol_index = 0;
                cleared += 1;
            }
        }
        // 针已清空，remove_palette_color 不再拒绝
        self.remove_palette_color(color_index);
        self.sync_cstrings();
        Ok(cleared)
    }

    /// 更改某色的符号（`new_symbol` 为用户从符号集中挑选的字符）。
    ///
    /// 目标符号已被其他颜色占用时，两个颜色交换符号，保证符号一一对应。
    /// 返回被交换的另一色索引（无冲突返回 None）。
    pub fn set_color_symbol(
        &mut self,
        color_index: u32,
        new_symbol: char,
    ) -> CoreResult<Option<u32>> {
        self.check_color(color_index)?;
        let idx = color_index as usize;
        if self.symbols[idx] == new_symbol {
            return Ok(None); // 本来就是这个符号
        }
        // 找到占用目标符号的颜色，与之交换；无人占用则直接替换
        let swapped = self
            .symbols
            .iter()
            .position(|&s| s == new_symbol)
            .filter(|&i| i != idx)
            .map(|i| i as u32);
        let old = self.symbols[idx];
        self.symbols[idx] = new_symbol;
        if let Some(other) = swapped {
            self.symbols[other as usize] = old;
        }
        self.sync_cstrings();
        Ok(swapped)
    }

    /// 合并颜色：`from` 的所有针改用 `to`，随后把 `from` 从调色板中彻底移除。
    ///
    /// 这是拖拽合并的语义——「黑色拖到白色上」表示黑色全部改用白色，
    /// 因此黑色色块本身也应该消失，而不是变成一个用量为 0 的空色块。
    /// 返回被改动的针数（0 = from 本就没有针，或索引非法）。
    pub fn merge_color_into(&mut self, from: u32, to: u32) -> CoreResult<u32> {
        if from == to {
            return Ok(0);
        }
        self.check_color(from)?;
        self.check_color(to)?;
        let changed = replace_color(&mut self.cells, from, to);
        self.remove_palette_color(from);
        Ok(changed)
    }

    /// 从调色板移除指定颜色并压缩调色板 / 符号表 / 单元格索引。
    ///
    /// 颜色若仍有针在用则拒绝移除（否则会丢失图案内容）。
    /// 返回是否真的移除了。
    pub fn remove_palette_color(&mut self, index: u32) -> bool {
        let idx = index as usize;
        if idx >= self.palette.len() {
            return false;
        }
        if color_histogram(&self.cells, self.palette.len())[idx] > 0 {
            return false;
        }
        // 保留符号分配：只把被删项之后的下标前移，其余颜色的符号不变
        self.palette.remove(idx);
        if idx < self.symbols.len() {
            self.symbols.remove(idx);
        }
        for c in self.cells.iter_mut() {
            if c.color_index != COLOR_NONE && (c.color_index as usize) > idx {
                c.color_index -= 1;
                c.symbol_index -= 1;
            }
        }
        self.sync_cstrings();
        true
    }

    /// 把指定色号加入调色板（已存在则直接返回已有索引）。
    /// 色号可在 DMC 绣线或 Artkal 拼豆两套色卡中查找（按模式生成时取对应卡）。
    /// 返回新索引，未找到或失败返回 -1。
    pub fn add_palette_color(&mut self, code: &str) -> i32 {
        if let Some(idx) = self.palette.iter().position(|c| c.code == code) {
            return idx as i32;
        }
        let found = dmc_data::DMC_COLORS
            .iter()
            .chain(perler_data::PERLER_COLORS.iter())
            .find(|c| c.code == code);
        let Some(dc) = found.copied() else {
            return -1;
        };
        self.palette.push(dc);
        // 新色用量 0，按用量重排符号不会改变已有颜色的符号，仅补一个符号即可
        self.redistribute_symbols();
        self.sync_cstrings();
        (self.palette.len() - 1) as i32
    }

    /// 去杂点。
    pub fn cleanup(&mut self, strength: u32) -> u32 {
        remove_isolated(&mut self.cells, self.width, self.height, strength)
    }

    /// 稀有色合并（并同步压缩调色板与符号表）。返回被合并掉的颜色种数。
    pub fn merge_rare_colors(&mut self, ppt: u32) -> u32 {
        let remap = merge_rare_colors(&mut self.cells, &self.palette, ppt);
        let merged = remap.iter().enumerate().filter(|(i, r)| *i != **r).count();
        if merged == 0 {
            return 0;
        }
        let (palette, symbols, new_index) = compact_palette(&self.palette, &self.symbols, &remap);
        remap_cells(&mut self.cells, &remap, &new_index);
        self.palette = palette;
        self.symbols = symbols;
        self.sync_cstrings();
        merged as u32
    }

    /// 清理未使用颜色：删除调色板中「一针都没用到」的颜色，并同步压缩符号表。
    ///
    /// 编辑图纸时用户可能先加入若干颜色再改用别的，最终这些颜色用量为 0，
    /// 需要从调色板里剔除。返回被删除的颜色种数（0 = 无可清理）。
    pub fn prune_unused_colors(&mut self) -> u32 {
        let counts = color_histogram(&self.cells, self.palette.len());
        let kept = counts.iter().filter(|c| **c > 0).count();
        // 全空图纸（kept == 0）或全部在用时无需处理；避免把调色板清空
        if kept == 0 || kept == self.palette.len() {
            return 0;
        }
        let remap: Vec<usize> = (0..self.palette.len())
            .map(|i| if counts[i] > 0 { i } else { usize::MAX })
            .collect();
        let removed = (self.palette.len() - kept) as u32;
        let (palette, symbols, new_index) = compact_palette(&self.palette, &self.symbols, &remap);
        remap_cells(&mut self.cells, &remap, &new_index);
        self.palette = palette;
        self.symbols = symbols;
        self.redistribute_symbols();
        self.sync_cstrings();
        removed
    }

    /// 切换符号集（保持「用量 → 清晰度」映射）。
    pub fn set_symbol_set(&mut self, symbol_set: u32) -> bool {
        if self.options.symbol_set == symbol_set {
            return false;
        }
        self.options.symbol_set = symbol_set;
        self.redistribute_symbols();
        true
    }

    /// 按当前用量重新分配符号（编辑后让最常用色重新拿到最清晰符号）。
    pub fn redistribute_symbols(&mut self) {
        let counts = color_histogram(&self.cells, self.palette.len());
        self.symbols = grid_gen::assign_symbols_by_usage(&counts, self.options.symbol_set);
    }

    /// 修改单元格符号（画布编辑入口，经命令栈调用）。
    /// 符号与调色板一一对应，因此改符号等价于改该格的绣色。
    pub fn set_symbol(&mut self, x: u32, y: u32, symbol_index: u32) -> CoreResult<()> {
        let i = self.check_cell(x, y)?;
        if symbol_index as usize >= self.palette.len() {
            return Err(CoreError::InvalidArg(format!(
                "symbol index {symbol_index} out of palette"
            )));
        }
        self.cells[i].symbol_index = symbol_index;
        self.cells[i].color_index = symbol_index;
        Ok(())
    }

    /// 序列化为工程数据块（bincode）。
    pub fn to_bytes(&self) -> CoreResult<Vec<u8>> {
        let data = ChartData::from_chart(self)?;
        Ok(bincode::serialize(&data)?)
    }

    /// 从工程数据块反序列化（自动迁移旧版本 schema）。
    pub fn from_bytes(bytes: &[u8]) -> CoreResult<Self> {
        match bincode::deserialize::<ChartData>(bytes) {
            Ok(data) if data.schema_version == ChartData::SCHEMA_VERSION => data.into_chart(),
            _ => {
                // schema v1（无 options 字段）→ 用默认值迁移
                let v1: ChartDataV1 = bincode::deserialize(bytes)
                    .map_err(|_| CoreError::project_corrupt("unrecognized chart data block"))?;
                v1.migrate().into_chart()
            }
        }
    }
}

/// 稀有色合并后压缩调色板与符号表。
/// 返回 (新调色板, 新符号表, 旧索引 → 新索引；usize::MAX 表示该旧索引已被淘汰)。
fn compact_palette(
    palette: &[DmcColor],
    symbols: &[char],
    remap: &[usize],
) -> (Vec<DmcColor>, Vec<char>, Vec<usize>) {
    let mut new_index = vec![usize::MAX; palette.len()];
    let mut new_palette = Vec::new();
    let mut new_symbols = Vec::new();
    for i in 0..palette.len() {
        if remap[i] == i {
            new_index[i] = new_palette.len();
            new_palette.push(palette[i]);
            new_symbols.push(symbols.get(i).copied().unwrap_or('?'));
        }
    }
    (new_palette, new_symbols, new_index)
}

/// 把 cells 的旧调色板索引重映射到压缩后的连续区间。
fn remap_cells(cells: &mut [CmGridCell], remap: &[usize], new_index: &[usize]) {
    for c in cells.iter_mut() {
        if c.color_index == COLOR_NONE {
            continue;
        }
        let old = c.color_index as usize;
        let Some(&survived) = remap.get(old) else {
            continue;
        };
        let Some(&n) = new_index.get(survived) else {
            continue;
        };
        if n == usize::MAX {
            continue;
        }
        c.color_index = n as u32;
        c.symbol_index = n as u32;
    }
}

/// 空 C 字符串（以 NUL 结尾，可直接转为 *const c_char）。
static EMPTY_STR: &[u8] = b"\0";

/// 工程数据块 schema（.bean 内 data/cross_stitch.bin）。
/// 版本演进通过 migrate 链处理（开发大纲第 14 章）。
#[derive(serde::Serialize, serde::Deserialize)]
pub struct ChartData {
    pub schema_version: u32,
    pub params: CmCrossStitchParams,
    /// 高级选项（schema v2 引入）
    pub options: CmStitchOptions,
    pub width: u32,
    pub height: u32,
    pub cells: Vec<CmGridCell>,
    /// DMC 色号索引（相对 dmc_data::DMC_COLORS）
    pub palette_codes: Vec<String>,
    pub symbols: Vec<char>,
    pub source_name: String,
}

/// schema v1 数据块（无 options），用于旧工程迁移。
#[derive(serde::Serialize, serde::Deserialize)]
struct ChartDataV1 {
    schema_version: u32,
    params: CmCrossStitchParams,
    width: u32,
    height: u32,
    cells: Vec<CmGridCell>,
    palette_codes: Vec<String>,
    symbols: Vec<char>,
    source_name: String,
}

impl ChartDataV1 {
    /// v1 → v2：新增字段取默认值，保证旧工程可直接打开。
    fn migrate(self) -> ChartData {
        let _ = self.schema_version;
        ChartData {
            schema_version: ChartData::SCHEMA_VERSION,
            params: self.params,
            options: CmStitchOptions::default(),
            width: self.width,
            height: self.height,
            cells: self.cells,
            palette_codes: self.palette_codes,
            symbols: self.symbols,
            source_name: self.source_name,
        }
    }
}

impl ChartData {
    pub const SCHEMA_VERSION: u32 = 2;

    pub fn from_chart(chart: &CrossStitchChart) -> CoreResult<Self> {
        let codes = chart
            .palette
            .iter()
            .map(|c| {
                let in_card = dmc_data::DMC_COLORS
                    .iter()
                    .chain(perler_data::PERLER_COLORS.iter())
                    .any(|d| d.code == c.code);
                if in_card {
                    Ok(c.code.to_string())
                } else {
                    Err(CoreError::Internal(format!(
                        "palette color {} not in DMC or perler table",
                        c.code
                    )))
                }
            })
            .collect::<CoreResult<Vec<_>>>()?;
        Ok(Self {
            schema_version: Self::SCHEMA_VERSION,
            params: chart.params,
            options: chart.options,
            width: chart.width,
            height: chart.height,
            cells: chart.cells.clone(),
            palette_codes: codes,
            symbols: chart.symbols.clone(),
            source_name: chart.source_name.clone(),
        })
    }

    pub fn into_chart(self) -> CoreResult<CrossStitchChart> {
        let palette = self
            .palette_codes
            .iter()
            .map(|code| {
                dmc_data::DMC_COLORS
                    .iter()
                    .chain(perler_data::PERLER_COLORS.iter())
                    .find(|d| d.code == *code)
                    .copied()
                    .ok_or_else(|| CoreError::project_corrupt(format!("unknown color code {code}")))
            })
            .collect::<CoreResult<Vec<_>>>()?;
        let mut chart = CrossStitchChart {
            params: self.params,
            options: self.options,
            width: self.width,
            height: self.height,
            cells: self.cells,
            palette,
            symbols: self.symbols,
            source_name: self.source_name,
            cstrings: Vec::new(),
            legend_cache: Vec::new(),
            symbol_cache: Vec::new(),
        };
        chart.sync_cstrings();
        Ok(chart)
    }
}

/// 生成十字绣图纸（FFI 入口对应的纯 Rust 实现）。
pub fn generate_chart(
    rgba: &[u8],
    width: u32,
    height: u32,
    params: CmCrossStitchParams,
    options: CmStitchOptions,
    progress: &mut Progress,
) -> CoreResult<CrossStitchChart> {
    if params.grid_width == 0
        || params.grid_height == 0
        || params.grid_width > MAX_GRID_SIDE
        || params.grid_height > MAX_GRID_SIDE
    {
        return Err(CoreError::InvalidArg(format!(
            "grid size {}x{} out of range",
            params.grid_width, params.grid_height
        )));
    }
    if params.max_colors == 0 || params.max_colors > MAX_COLORS {
        return Err(CoreError::InvalidArg(format!(
            "max_colors {} out of range 1..={MAX_COLORS}",
            params.max_colors
        )));
    }

    progress.report(stage::START, 0.0);
    progress.report(stage::DOWNSCALE, 0.1);
    let grid = image_pixel::downscale(
        rgba,
        width,
        height,
        params.grid_width,
        params.grid_height,
        options.sample_mode,
    )?;
    progress.report(stage::QUANTIZE, 0.3);

    // 按手作品类选择目标色卡：拼豆用 Artkal S 色卡，十字绣 / 珠绣用 DMC 色卡
    let card = if options.craft_mode == craft::PERLER {
        color_quant::Card::Perler
    } else {
        color_quant::Card::Dmc
    };
    let result = color_quant::quantize_with_options(
        &grid,
        params.grid_width,
        params.grid_height,
        params.max_colors as usize,
        params.dither != 0,
        color_quant::CLUSTER_SAMPLE_CAP,
        card,
    )?;
    progress.report(stage::MATCH, 0.7);

    let mut cells = build_cells(params.grid_width, &result.assignments, result.palette.len());
    let mut palette = result.palette;

    // 后处理：去杂点 → 稀有色合并（合并会压缩调色板，须在符号分配之前）
    progress.report(stage::GRID, 0.85);
    let cleaned = remove_isolated(
        &mut cells,
        params.grid_width,
        params.grid_height,
        options.cleanup,
    );
    let _ = cleaned; // 数量由 UI 侧按需通过统计差值呈现

    let remap = merge_rare_colors(&mut cells, &palette, options.rare_color_ppt);
    if remap.iter().enumerate().any(|(i, &r)| i != r) {
        let (new_palette, _, new_index) = compact_palette(&palette, &[], &remap);
        remap_cells(&mut cells, &remap, &new_index);
        palette = new_palette;
    }

    // 符号：按用量降序分配，最常用的色拿到最好认的符号
    let counts = color_histogram(&cells, palette.len());
    let symbols = grid_gen::assign_symbols_by_usage(&counts, options.symbol_set);

    progress.report(stage::GRID, 0.95);

    let mut chart = CrossStitchChart {
        params,
        options,
        width: params.grid_width,
        height: params.grid_height,
        cells,
        palette,
        symbols,
        source_name: String::new(),
        cstrings: Vec::new(),
        legend_cache: Vec::new(),
        symbol_cache: Vec::new(),
    };
    chart.sync_cstrings();
    progress.report(stage::DONE, 1.0);
    Ok(chart)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_image(w: u32, h: u32, f: impl Fn(u32, u32) -> [u8; 4]) -> Vec<u8> {
        let mut v = Vec::new();
        for y in 0..h {
            for x in 0..w {
                v.extend_from_slice(&f(x, y));
            }
        }
        v
    }

    #[test]
    fn generate_simple_chart() {
        // 16x16 左红右蓝 → 8x8 网格
        let rgba = test_image(16, 16, |x, _| {
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
        let mut p = Progress::noop();
        let chart =
            generate_chart(&rgba, 16, 16, params, CmStitchOptions::default(), &mut p).unwrap();
        assert_eq!(chart.cells.len(), 64);
        assert!(chart.palette.len() >= 2);
        assert_eq!(chart.total_stitches(), 64);
        let legend = chart.legend();
        assert_eq!(legend.len(), chart.palette.len());
        let total: u32 = legend.iter().map(|e| e.stitch_count).sum();
        assert_eq!(total, 64);
    }

    #[test]
    fn generate_bead_mode() {
        let rgba = test_image(8, 8, |_, _| [0, 200, 0, 255]);
        let params = CmCrossStitchParams {
            grid_width: 4,
            grid_height: 4,
            max_colors: 4,
            bead_mode: 1,
            ..Default::default()
        };
        let mut p = Progress::noop();
        let chart =
            generate_chart(&rgba, 8, 8, params, CmStitchOptions::default(), &mut p).unwrap();
        assert_eq!(chart.total_stitches(), 16);
        assert!(chart.legend()[0].bead_count == 16);
    }

    #[test]
    fn chart_serialization_roundtrip() {
        let rgba = test_image(8, 8, |x, _| {
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
        let options = CmStitchOptions {
            symbol_set: symbol::DIGITS,
            sample_mode: sample::MEDOID,
            cleanup: cleanup::STRONG,
            rare_color_ppt: 10,
            craft_mode: craft::PERLER,
        };
        let mut p = Progress::noop();
        let chart = generate_chart(&rgba, 8, 8, params, options, &mut p).unwrap();
        let bytes = chart.to_bytes().unwrap();
        let restored = CrossStitchChart::from_bytes(&bytes).unwrap();
        assert_eq!(restored.cells, chart.cells);
        assert_eq!(restored.palette.len(), chart.palette.len());
        assert_eq!(restored.total_stitches(), chart.total_stitches());
        assert_eq!(restored.options, options, "高级选项必须随工程持久化");
    }

    #[test]
    fn legacy_schema_v1_migrates() {
        // 手工构造 v1 数据块（无 options 字段），验证旧工程仍可打开
        let rgba = test_image(8, 8, |x, _| {
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
        let mut p = Progress::noop();
        let chart =
            generate_chart(&rgba, 8, 8, params, CmStitchOptions::default(), &mut p).unwrap();

        let v1 = ChartDataV1 {
            schema_version: 1,
            params: chart.params,
            width: chart.width,
            height: chart.height,
            cells: chart.cells.clone(),
            palette_codes: chart.palette.iter().map(|c| c.code.to_string()).collect(),
            symbols: chart.symbols.clone(),
            source_name: String::new(),
        };
        let bytes = bincode::serialize(&v1).unwrap();
        let migrated = CrossStitchChart::from_bytes(&bytes).unwrap();
        assert_eq!(migrated.options, CmStitchOptions::default());
        assert_eq!(migrated.total_stitches(), chart.total_stitches());
    }

    #[test]
    fn invalid_params_rejected() {
        let rgba = test_image(4, 4, |_, _| [0, 0, 0, 255]);
        let mut p = Progress::noop();
        let bad = CmCrossStitchParams {
            grid_width: 0,
            ..Default::default()
        };
        assert!(generate_chart(&rgba, 4, 4, bad, CmStitchOptions::default(), &mut p).is_err());
        let bad2 = CmCrossStitchParams {
            max_colors: 0,
            ..Default::default()
        };
        assert!(generate_chart(&rgba, 4, 4, bad2, CmStitchOptions::default(), &mut p).is_err());
    }

    #[test]
    fn generated_symbols_are_unique_per_color() {
        let rgba = test_image(32, 32, |x, y| {
            let c = ((x / 8 + y / 8) % 4) as u8;
            [c * 60, 255 - c * 60, 128, 255]
        });
        let params = CmCrossStitchParams {
            grid_width: 16,
            grid_height: 16,
            max_colors: 6,
            ..Default::default()
        };
        let mut p = Progress::noop();
        let chart =
            generate_chart(&rgba, 32, 32, params, CmStitchOptions::default(), &mut p).unwrap();
        assert_eq!(chart.symbols.len(), chart.palette.len());
        // 同一调色板索引在 cells 中必须对应同一个符号
        for c in &chart.cells {
            if c.color_index == COLOR_NONE {
                continue;
            }
            assert_eq!(c.symbol_index, c.color_index);
        }
    }

    #[test]
    fn edit_paint_and_fill() {
        let rgba = test_image(8, 8, |x, _| {
            if x < 4 {
                [255, 0, 0, 255]
            } else {
                [0, 0, 255, 255]
            }
        });
        let params = CmCrossStitchParams {
            grid_width: 8,
            grid_height: 8,
            max_colors: 4,
            ..Default::default()
        };
        let mut p = Progress::noop();
        let mut chart =
            generate_chart(&rgba, 8, 8, params, CmStitchOptions::default(), &mut p).unwrap();
        assert!(chart.palette.len() >= 2);

        chart.paint_cell(0, 0, 1).unwrap();
        assert_eq!(chart.cells[0].color_index, 1);

        chart.rect_fill(0, 0, 7, 7, 0).unwrap();
        assert!(chart.cells.iter().all(|c| c.color_index == 0));

        let changed = chart.flood_fill(0, 0, 1).unwrap();
        assert_eq!(changed, 64);
        assert!(chart.cells.iter().all(|c| c.color_index == 1));

        assert!(chart.paint_cell(99, 99, 0).is_err(), "越界应报错");
        assert!(chart.paint_cell(0, 0, 999).is_err(), "非法色索引应报错");
    }

    #[test]
    fn prune_unused_colors_drops_zero_usage_palette() {
        let rgba = test_image(8, 8, |x, _| {
            if x < 4 {
                [255, 0, 0, 255]
            } else {
                [0, 0, 255, 255]
            }
        });
        let params = CmCrossStitchParams {
            grid_width: 8,
            grid_height: 8,
            max_colors: 4,
            ..Default::default()
        };
        let mut p = Progress::noop();
        let mut chart =
            generate_chart(&rgba, 8, 8, params, CmStitchOptions::default(), &mut p).unwrap();
        let before = chart.palette.len();

        // 加入一个从未落针的颜色 → 用量为 0
        let unused_code = {
            let used: Vec<&str> = chart.palette.iter().map(|c| c.code).collect();
            dmc_data::DMC_COLORS
                .iter()
                .find(|c| !used.contains(&c.code))
                .expect("色卡里应存在未使用的 DMC 色")
                .code
                .to_string()
        };
        let unused_idx = chart.add_palette_color(&unused_code);
        assert!(unused_idx >= 0, "应能加入新颜色");
        assert_eq!(chart.palette.len(), before + 1);
        assert_eq!(
            chart.prune_unused_colors(),
            1,
            "应恰好清理掉 1 个未使用颜色"
        );
        assert_eq!(chart.palette.len(), before, "调色板应回到原长度");
        assert_eq!(chart.symbols.len(), before, "符号表应与调色板同步");
        assert!(
            !chart.palette.iter().any(|c| c.code == unused_code),
            "未使用颜色应已从调色板移除"
        );
        // 所有格子的索引都应落在新的连续区间内
        assert!(
            chart
                .cells
                .iter()
                .all(|c| c.color_index == COLOR_NONE || (c.color_index as usize) < before),
            "单元格索引应已重映射"
        );

        // 全部颜色都在用时不应有任何改动
        assert_eq!(chart.prune_unused_colors(), 0, "无未使用颜色时应返回 0");
    }

    #[test]
    fn merge_color_into_removes_source_palette_entry() {
        let params = CmCrossStitchParams {
            grid_width: 4,
            grid_height: 4,
            max_colors: 4,
            ..Default::default()
        };
        let mut chart = CrossStitchChart::empty(4, 4, params);
        for i in 0..3 {
            chart.add_palette_color(dmc_data::DMC_COLORS[i].code);
        }
        assert_eq!(chart.palette.len(), 3);
        // 用 0 号色铺满 2/3，用 1 号色铺剩余 1/3
        for y in 0..4u32 {
            for x in 0..4u32 {
                let color = if y < 3 { 0 } else { 1 };
                chart.paint_cell(x, y, color).unwrap();
            }
        }

        let merged_code = chart.palette[0].code;
        let changed = chart.merge_color_into(0, 1).unwrap();
        assert_eq!(changed, 12, "0 号色的 12 针应全部改用 1 号色");
        assert_eq!(chart.palette.len(), 2, "源颜色应从调色板中彻底移除");
        assert!(
            !chart.palette.iter().any(|c| c.code == merged_code),
            "被合并的颜色不应残留在调色板里"
        );
        assert_eq!(chart.symbols.len(), 2, "符号表应与调色板等长");
        assert!(
            chart.cells.iter().all(|c| c.color_index <= 1),
            "单元格索引应已压缩到有效区间"
        );
        assert_eq!(
            chart.legend().iter().map(|e| e.stitch_count).sum::<u32>(),
            16,
            "总针数应保持不变"
        );
        // 合并后图例应与调色板等长，且第一个条目的色号不能为空
        // （材料表第一行变空的问题封装在此检验）
        let after_legend = chart.legend();
        assert_eq!(
            after_legend.len(),
            chart.palette.len(),
            "图例应与调色板等长"
        );
        assert!(!after_legend.is_empty(), "合并后图例不应为空");
        let first_idx = after_legend[0].color_index as usize;
        assert!(first_idx < chart.palette.len(), "第一个图例条目索引应有效");
        let first_code = unsafe { std::ffi::CStr::from_ptr(chart.color_code(first_idx)) };
        assert!(
            !first_code.to_bytes().is_empty(),
            "第一个图例条目的色号不应为空"
        );

        // 同色合并是空操作
        assert_eq!(chart.merge_color_into(0, 0).unwrap(), 0);
        // 越界索引应报错
        assert!(chart.merge_color_into(0, 99).is_err());
    }

    #[test]
    fn merge_twice_keeps_first_legend_entry_valid() {
        let params = CmCrossStitchParams {
            grid_width: 8,
            grid_height: 8,
            max_colors: 24,
            ..Default::default()
        };
        let mut chart = CrossStitchChart::empty(8, 8, params);
        for i in 0..3 {
            chart.add_palette_color(dmc_data::DMC_COLORS[i].code);
        }
        // 与用户日志一致：空盘加 3 色后先合并 2->1（changed 0）
        chart.merge_color_into(2, 1).unwrap();
        for i in 3..12 {
            chart.add_palette_color(dmc_data::DMC_COLORS[i].code);
        }
        // 涂几格让第二次合并有真实改动
        for y in 0..8u32 {
            for x in 0..8u32 {
                let c = (x / 4 + y / 4) % 2;
                chart.paint_cell(x, y, c).unwrap();
            }
        }
        chart.merge_color_into(10, 9).unwrap();
        let l = chart.legend();
        assert_eq!(l.len(), chart.palette.len(), "图例应与调色板等长");
        assert!(!l.is_empty());
        assert!(
            (l[0].color_index as usize) < chart.palette.len(),
            "第一条图例索引应有效"
        );
        let c0 = unsafe { std::ffi::CStr::from_ptr(chart.color_code(l[0].color_index as usize)) };
        let c0s = c0.to_bytes().to_vec();
        assert!(!c0s.is_empty(), "连续合并后第一条图例色号不应为空");
        assert!(!chart.palette[0].code.is_empty(), "palette[0] 色号不应为空");
    }

    #[test]
    fn delete_color_clears_stitches_and_removes_entry() {
        let params = CmCrossStitchParams {
            grid_width: 4,
            grid_height: 4,
            max_colors: 4,
            ..Default::default()
        };
        let mut chart = CrossStitchChart::empty(4, 4, params);
        chart.add_palette_color(dmc_data::DMC_COLORS[0].code);
        chart.add_palette_color(dmc_data::DMC_COLORS[1].code);
        // 上半 0 号色、下半 1 号色
        for y in 0..4u32 {
            for x in 0..4u32 {
                chart.paint_cell(x, y, if y < 2 { 0 } else { 1 }).unwrap();
            }
        }
        let deleted_code = chart.palette[0].code;
        let cleared = chart.delete_color(0).unwrap();
        assert_eq!(cleared, 8, "0 号色的 8 针应被清空");
        assert_eq!(chart.palette.len(), 1, "调色板应只剩 1 色");
        assert!(!chart.palette.iter().any(|c| c.code == deleted_code));
        assert!(
            chart
                .cells
                .iter()
                .all(|c| c.color_index == COLOR_NONE || c.color_index == 0),
            "剩余单元格索引应压缩到 0"
        );
        // 上半部分应变为空格
        assert_eq!(chart.cells[0].color_index, COLOR_NONE);
        assert_eq!(chart.cells[8].color_index, 0);
        assert!(chart.delete_color(9).is_err(), "越界删除应报错");
    }

    #[test]
    fn set_color_symbol_swaps_on_conflict() {
        let params = CmCrossStitchParams {
            grid_width: 4,
            grid_height: 4,
            max_colors: 4,
            ..Default::default()
        };
        let mut chart = CrossStitchChart::empty(4, 4, params);
        chart.add_palette_color(dmc_data::DMC_COLORS[0].code);
        chart.add_palette_color(dmc_data::DMC_COLORS[1].code);
        let s0 = chart.symbols[0];
        let s1 = chart.symbols[1];
        assert_ne!(s0, s1, "两色符号应不同");

        // 0 号色改用 1 号色的符号 → 交换
        let swapped = chart.set_color_symbol(0, s1).unwrap();
        assert_eq!(swapped, Some(1), "符号冲突应报告被交换的颜色");
        assert_eq!(chart.symbols[0], s1);
        assert_eq!(chart.symbols[1], s0, "1 号色应拿到 0 号色的旧符号");

        // 改回一个未占用的符号 → 无交换
        let unused = "ABCDEFGHIJ"
            .chars()
            .find(|c| !chart.symbols.contains(c))
            .expect("应存在未占用符号");
        assert_eq!(chart.set_color_symbol(1, unused).unwrap(), None);
        assert_eq!(chart.symbols[1], unused);

        // 与自身相同 → 空操作
        assert_eq!(chart.set_color_symbol(1, unused).unwrap(), None);
        // 非法索引应报错
        assert!(chart.set_color_symbol(9, 'A').is_err());
    }

    #[test]
    fn remove_palette_color_refuses_colors_in_use() {
        let params = CmCrossStitchParams {
            grid_width: 2,
            grid_height: 2,
            max_colors: 2,
            ..Default::default()
        };
        let mut chart = CrossStitchChart::empty(2, 2, params);
        chart.add_palette_color(dmc_data::DMC_COLORS[0].code);
        chart.add_palette_color(dmc_data::DMC_COLORS[1].code);
        chart.paint_cell(0, 0, 0).unwrap();

        assert!(!chart.remove_palette_color(0), "仍有针在用的颜色不应被移除");
        assert_eq!(chart.palette.len(), 2);
        assert!(chart.remove_palette_color(1), "未使用的颜色应可移除");
        assert_eq!(chart.palette.len(), 1);
        assert_eq!(chart.cells[0].color_index, 0, "剩余单元格索引应保持有效");
    }

    #[test]
    fn prune_unused_colors_keeps_all_on_empty_chart() {
        let params = CmCrossStitchParams {
            grid_width: 4,
            grid_height: 4,
            max_colors: 4,
            ..Default::default()
        };
        let mut chart = CrossStitchChart::empty(4, 4, params);
        chart.add_palette_color(dmc_data::DMC_COLORS[0].code);
        assert_eq!(chart.palette.len(), 1);
        assert_eq!(chart.prune_unused_colors(), 0, "全空图纸不应把调色板清空");
        assert_eq!(chart.palette.len(), 1);
    }

    #[test]
    fn fabric_size_scales_with_count() {
        let params = CmCrossStitchParams {
            grid_width: 100,
            grid_height: 100,
            fabric_count: 14,
            ..Default::default()
        };
        let chart = CrossStitchChart::empty(100, 100, params);
        let s = chart.fabric_size();
        assert_eq!(s.width_mm, 181);
        let denser = CrossStitchChart::empty(
            100,
            100,
            CmCrossStitchParams {
                fabric_count: 18,
                ..params
            },
        );
        assert!(denser.fabric_size().width_mm < s.width_mm);
    }
}
