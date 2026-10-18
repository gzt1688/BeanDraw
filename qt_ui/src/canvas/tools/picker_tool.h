#pragma once

#include "canvas/tools/tool_base.h"

/// 吸管工具：拾取单元格的绣色并设为当前色（不修改图纸，不进命令栈）。
class PickerTool : public ToolBase
{
    Q_OBJECT

public:
    explicit PickerTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.picker"); }
    QCursor cursor() const override;

signals:
    /// 拾取到 (x,y) 处的颜色（索引为 UINT32_MAX 表示空格）。
    void colorPicked(int x, int y);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
};
