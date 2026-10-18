#include "shortcut_manager.h"

#include <QAction>

ShortcutManager::ShortcutManager()
{
    // 遵循平台标准键位（开发大纲第 15 章）
    m_defaults = {
        {QStringLiteral("file.new"), QKeySequence::New},
        {QStringLiteral("file.open"), QKeySequence::Open},
        {QStringLiteral("file.save"), QKeySequence::Save},
        {QStringLiteral("file.save_as"), QKeySequence::SaveAs},
        {QStringLiteral("edit.undo"), QKeySequence::Undo},
        {QStringLiteral("edit.redo"), QKeySequence::Redo},
        {QStringLiteral("edit.delete"), QKeySequence::Delete},
        {QStringLiteral("view.zoom_in"), QKeySequence::ZoomIn},
        {QStringLiteral("view.zoom_out"), QKeySequence::ZoomOut},
        {QStringLiteral("view.fit"), QKeySequence(Qt::CTRL | Qt::Key_0)},
        {QStringLiteral("tool.select"), QKeySequence(Qt::Key_V)},
        {QStringLiteral("tool.pan"), QKeySequence(Qt::Key_H)},
        {QStringLiteral("tool.zoom"), QKeySequence(Qt::Key_Z)},
        // 绘制类工具沿用图形软件惯例：B 画笔 / G 油漆桶 / U 矩形 / Q 吸管 / E 橡皮
        // （吸管原默认 I，用户偏好 Q，与 PS 大纲第 15 章调整一致）
        {QStringLiteral("tool.stitch"), QKeySequence(Qt::Key_B)},
        {QStringLiteral("tool.fill"), QKeySequence(Qt::Key_G)},
        {QStringLiteral("tool.rect_fill"), QKeySequence(Qt::Key_U)},
        {QStringLiteral("tool.picker"), QKeySequence(Qt::Key_Q)},
        // 看图模式（画布最大化）：默认空格（应用窗口：画布视图子树内触发）
        {QStringLiteral("view.max"), QKeySequence(Qt::Key_Space)},
    };
}

ShortcutManager& ShortcutManager::instance()
{
    static ShortcutManager instance;
    return instance;
}

QKeySequence ShortcutManager::sequence(const QString& action) const
{
    return m_defaults.value(action);
}

void ShortcutManager::applyTo(QAction* action) const
{
    if (!action) {
        return;
    }
    const QKeySequence seq = m_defaults.value(action->objectName());
    if (!seq.isEmpty()) {
        action->setShortcut(seq);
    }
}
