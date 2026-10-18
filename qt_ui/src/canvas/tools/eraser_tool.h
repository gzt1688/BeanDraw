#pragma once

#include "canvas/tools/tool_base.h"

#include <QPoint>

/// 橡皮工具：拖拽把经过的单元格清空（置为留空），一次拖拽合并为一条撤销记录。
class EraserTool : public ToolBase
{
    Q_OBJECT

public:
    explicit EraserTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.eraser"); }
    QCursor cursor() const override;

signals:
    void cellEraseRequested(int x, int y);
    void strokeStarted();
    void strokeFinished();

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;

private:
    bool m_erasing = false;
    QPoint m_lastCell;
};
