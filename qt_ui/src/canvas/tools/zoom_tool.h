#pragma once

#include "canvas/tools/tool_base.h"

/// 缩放工具：滚轮缩放，左键单击放大（配合 Alt 缩小）。
class ZoomTool : public ToolBase
{
    Q_OBJECT

public:
    explicit ZoomTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.zoom"); }
    QCursor cursor() const override;

protected:
    void wheel(QWheelEvent* event, QGraphicsView* view) override;
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
};
