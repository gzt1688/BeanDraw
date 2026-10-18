//! 撤销 / 重做：命令栈（开发大纲第 13 章）。
//!
//! Phase 1 采用**状态快照**策略（各模块数据量小，实现简单）：
//! states[0] 为初始状态，states[i] 为第 i 次操作后的状态。
//! 重型模块可在后续迁移到 Command trait 逆操作模式。

use std::ops::Range;

/// 类型化命令（预留，供大模块迁移）。
pub trait Command: Send {
    fn label(&self) -> &str;
    fn execute(&mut self);
    fn undo(&mut self);
}

/// 一条历史记录。
#[derive(Clone, Debug)]
pub struct HistoryEntry {
    /// 操作标签（i18n key 或调试名）
    pub label: String,
    /// 该次操作后的完整状态
    pub state: Vec<u8>,
}

/// 快照式历史栈。
#[derive(Clone, Debug, Default)]
pub struct History {
    entries: Vec<HistoryEntry>,
    /// 当前状态索引（0 = 初始状态）
    cursor: usize,
    max_depth: usize,
}

impl History {
    pub fn new(max_depth: usize) -> Self {
        Self {
            entries: Vec::new(),
            cursor: 0,
            max_depth: max_depth.max(2),
        }
    }

    /// 记录初始状态（新建 / 打开工程时调用）。
    pub fn init(&mut self, state: Vec<u8>) {
        self.entries.clear();
        self.cursor = 0;
        self.entries.push(HistoryEntry {
            label: "initial".into(),
            state,
        });
    }

    /// 压入一次操作后的新状态；会截断重做分支。
    pub fn push(&mut self, label: impl Into<String>, state: Vec<u8>) {
        self.entries.truncate(self.cursor + 1);
        self.entries.push(HistoryEntry {
            label: label.into(),
            state,
        });
        // 深度控制：超出上限丢弃最旧（保留 initial）
        while self.entries.len() > self.max_depth {
            self.entries.remove(0);
        }
        self.cursor = self.entries.len() - 1;
    }

    /// 撤销：返回应恢复的状态（借用，直至下次变更）。
    pub fn undo(&mut self) -> Option<&[u8]> {
        if self.cursor == 0 {
            return None;
        }
        self.cursor -= 1;
        Some(&self.entries[self.cursor].state)
    }

    /// 重做：返回应恢复的状态。
    pub fn redo(&mut self) -> Option<&[u8]> {
        if self.cursor + 1 >= self.entries.len() {
            return None;
        }
        self.cursor += 1;
        Some(&self.entries[self.cursor].state)
    }

    pub fn can_undo(&self) -> bool {
        self.cursor > 0
    }

    pub fn can_redo(&self) -> bool {
        self.cursor + 1 < self.entries.len()
    }

    pub fn current(&self) -> Option<&[u8]> {
        self.entries.get(self.cursor).map(|e| e.state.as_slice())
    }

    pub fn clear(&mut self) {
        self.entries.clear();
        self.cursor = 0;
    }

    /// 撤销分支的标签（供 UI 显示 "Undo xxx"，最近操作在前，不含初始状态）。
    pub fn undo_labels(&self) -> Vec<&str> {
        if self.entries.is_empty() || self.cursor == 0 {
            return Vec::new();
        }
        self.entries[1..=self.cursor]
            .iter()
            .rev()
            .map(|e| e.label.as_str())
            .collect()
    }

    pub fn redo_labels(&self) -> Vec<&str> {
        self.entries[self.cursor + 1..]
            .iter()
            .map(|e| e.label.as_str())
            .collect()
    }

    /// 当前可撤销/重做区间长度（测试用）。
    pub fn debug_range(&self) -> Range<usize> {
        0..self.entries.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn init_undo_redo_flow() {
        let mut h = History::new(10);
        h.init(vec![0]);
        assert!(!h.can_undo());
        h.push("op1", vec![1]);
        h.push("op2", vec![2]);
        assert!(h.can_undo());
        assert!(!h.can_redo());

        assert_eq!(h.undo(), Some(&[1][..]));
        assert_eq!(h.undo(), Some(&[0][..]));
        assert!(h.undo().is_none());

        assert_eq!(h.redo(), Some(&[1][..]));
        assert_eq!(h.redo(), Some(&[2][..]));
        assert!(h.redo().is_none());
    }

    #[test]
    fn push_truncates_redo_branch() {
        let mut h = History::new(10);
        h.init(vec![0]);
        h.push("a", vec![1]);
        h.push("b", vec![2]);
        h.undo(); // 回到 1
        assert!(h.can_redo());
        h.push("c", vec![9]); // 新分支
        assert!(!h.can_redo());
        // 新分支的"上一步"是撤销前所在的状态 [1]
        assert_eq!(h.undo(), Some(&[1][..]));
        assert_eq!(h.undo(), Some(&[0][..]));
    }

    #[test]
    fn max_depth_trims_oldest() {
        let mut h = History::new(3);
        h.init(vec![0]);
        h.push("a", vec![1]);
        h.push("b", vec![2]);
        h.push("c", vec![3]);
        assert_eq!(h.debug_range(), 0..3);
        assert_eq!(h.undo(), Some(&[2][..]));
        assert_eq!(h.undo(), Some(&[1][..]));
        assert!(h.undo().is_none()); // 初始态已被裁剪
    }

    #[test]
    fn labels_for_ui() {
        let mut h = History::new(10);
        h.init(vec![0]);
        h.push("stitch", vec![1]);
        h.push("move", vec![2]);
        assert_eq!(h.undo_labels(), vec!["move", "stitch"]);
        assert!(h.redo_labels().is_empty());
    }
}
