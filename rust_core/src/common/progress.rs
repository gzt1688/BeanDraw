//! 进度回调：Rust → C++ 函数指针 + user_data（开发大纲第 12 章）。

use crate::common::ffi_types::CmProgressFn;
use std::os::raw::c_void;

/// 阶段码，Qt 侧用于显示阶段文案（文案由 Qt 翻译）。
pub mod stage {
    pub const START: i32 = 0;
    pub const DOWNSCALE: i32 = 1;
    pub const QUANTIZE: i32 = 2;
    pub const MATCH: i32 = 3;
    pub const GRID: i32 = 4;
    pub const EXPORT: i32 = 5;
    pub const DONE: i32 = 100;
}

/// 进度上报器：持有回调与 user_data，内部保证进度单调不回退。
#[derive(Clone, Copy)]
pub struct Progress {
    cb: CmProgressFn,
    user_data: *mut c_void,
    last: f32,
}

// 进度对象可跨线程传递（user_data 由调用方保证线程安全）
unsafe impl Send for Progress {}
unsafe impl Sync for Progress {}

impl Progress {
    pub fn new(cb: CmProgressFn, user_data: *mut c_void) -> Self {
        Self {
            cb,
            user_data,
            last: -1.0,
        }
    }

    /// 无回调的哑进度器（测试 / 后台任务用）。
    pub fn noop() -> Self {
        Self::new(None, std::ptr::null_mut())
    }

    /// 上报进度；progress < 0 表示失败终止。
    pub fn report(&mut self, stage: i32, progress: f32) {
        let p = progress.clamp(0.0, 1.0);
        if p <= self.last {
            return;
        }
        self.last = p;
        if let Some(cb) = self.cb {
            cb(self.user_data, p, stage);
        }
    }

    /// 失败终止。
    pub fn fail(&mut self) {
        if let Some(cb) = self.cb {
            cb(self.user_data, -1.0, -1);
        }
    }
}

impl std::fmt::Debug for Progress {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Progress")
            .field("last", &self.last)
            .finish()
    }
}
