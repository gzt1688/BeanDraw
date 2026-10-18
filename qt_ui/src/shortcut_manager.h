#pragma once

#include <QHash>
#include <QKeySequence>
#include <QString>

class QAction;

/// 快捷键管理（开发大纲第 15 章）。
/// Phase 1：默认键位表 + 应用；自定义 / 冲突检测 / 导入导出在 Phase 3。
class ShortcutManager
{
public:
    static ShortcutManager& instance();

    /// 命名动作的默认键位。
    QKeySequence sequence(const QString& action) const;

    /// 为 QAction 应用默认键位（action->objectName() 作为键）。
    void applyTo(QAction* action) const;

    /// 全部默认键位（设置对话框用）。
    QHash<QString, QKeySequence> defaults() const { return m_defaults; }

private:
    ShortcutManager();

    QHash<QString, QKeySequence> m_defaults;
};
