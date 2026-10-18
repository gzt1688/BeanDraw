#pragma once

#include "canvas/tools/tool_base.h"

#include <QPoint>

/// 油漆桶工具：点击单元格，填充 4-连通同色区域（经命令栈记录）。
class FillTool : public ToolBase
{
    Q_OBJECT

public:
    explicit FillTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.fill"); }
    QCursor cursor() const override;

signals:
    /// 请求从 (x,y) 开始区域填充。
    void fillRequested(int x, int y);
    /// 右键：从 (x,y) 开始清除整个同色区域。
    void fillEraseRequested(int x, int y);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void rightPress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;

private:
    void dispatch(QMouseEvent* event, QGraphicsView* view, bool erase);
};
