#pragma once

#include "canvas/tools/tool_base.h"

/// 文字工具（文字转豆子）：点击画布选一个起始格 → 主窗口在该处放置
/// 文字预览（参数来自变换面板「文字」组）；预览可移动 / 实时调整，
/// 面板「确认生成」后才真正写入豆子格子。Esc 取消预览。
class TextTool : public ToolBase
{
    Q_OBJECT

public:
    explicit TextTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.text"); }
    QCursor cursor() const override;

signals:
    /// 请求在 (cx, cy) 格放置/移动文字预览（由主窗口处理）。
    void textAnchorRequested(int cx, int cy);
    /// 取消预览（Esc 或点击图纸外）。
    void textCancelRequested();

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void keyPress(QKeyEvent* event, QGraphicsView* view) override;
};
