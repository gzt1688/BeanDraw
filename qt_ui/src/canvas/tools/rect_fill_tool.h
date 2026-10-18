#pragma once

#include "canvas/tools/tool_base.h"

#include <QPoint>
#include <QRect>

class QGraphicsRectItem;

/// 矩形填充工具：拖拽框选一个矩形区域并整体填色，带橡皮筋预览。
///
/// 松开鼠标时提交一次命令栈记录（宏命令语义，开发大纲第 13 章）。
class RectFillTool : public ToolBase
{
    Q_OBJECT

public:
    explicit RectFillTool(QObject* parent = nullptr);
    ~RectFillTool() override;

    QString toolId() const override { return QStringLiteral("tool.rect_fill"); }
    QCursor cursor() const override;

    void activate(QGraphicsView* view, CraftScene* scene) override;
    void deactivate() override;

signals:
    /// 请求填充闭区间矩形（含端点）。
    void rectFillRequested(int x0, int y0, int x1, int y1);
    /// 请求清除闭区间矩形。
    void rectEraseRequested(int x0, int y0, int x1, int y1);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;
    void rightPress(QMouseEvent* event, QGraphicsView* view) override;

private:
    void updateRubberBand();
    void clearRubberBand();

    bool m_dragging = false;
    bool m_erase = false;
    QPoint m_anchor;
    QPoint m_current;
    QGraphicsRectItem* m_rubberBand = nullptr;
};
