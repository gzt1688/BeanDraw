#pragma once

#include "canvas/tools/tool_base.h"

/// 选择工具：框选 / 拖拽移动图元（Phase 1 基础版）。
class SelectTool : public ToolBase
{
    Q_OBJECT

public:
    explicit SelectTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.select"); }
    QCursor cursor() const override;

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;

private:
    bool m_dragging = false;
    QPoint m_lastViewportPos;
};
