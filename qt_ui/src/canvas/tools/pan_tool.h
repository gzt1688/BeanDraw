#pragma once

#include "canvas/tools/tool_base.h"

/// 平移工具：按住拖动平移视图。
class PanTool : public ToolBase
{
    Q_OBJECT

public:
    explicit PanTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.pan"); }
    QCursor cursor() const override;

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;

private:
    bool m_panning = false;
    QPoint m_lastViewportPos;
};
