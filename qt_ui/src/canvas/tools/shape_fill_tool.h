#pragma once

#include "canvas/tools/tool_base.h"

#include <QPoint>
#include <QRect>

class QGraphicsPathItem;

/// 几何形状填充工具（圆形 / 椭圆共用）：
/// 拖拽定义外接矩形，松开时把形状内部的格子填为当前色。
/// 拖动中实时预览形状轮廓（圆形 = 外接正方形内切圆，椭圆 = 外接矩形内椭圆）。
///
/// 左键拖拽 = 填色；右键拖拽 = 清空（形状内格子置空格）。
/// 内部格子集合的判定在 MainWindow 侧（与 Rust 批量填格接口配合）。
class ShapeFillTool : public ToolBase
{
    Q_OBJECT

public:
    enum Shape {
        Circle = 0, ///< 圆形填充
        Ellipse = 1 ///< 椭圆填充
    };

    explicit ShapeFillTool(Shape shape, QObject* parent = nullptr);
    ~ShapeFillTool() override;

    QString toolId() const override;
    QCursor cursor() const override;

    void activate(QGraphicsView* view, CraftScene* scene) override;
    void deactivate() override;

signals:
    /// 请求填充闭区间外接矩形内的形状格子（含端点）。
    void shapeFillRequested(int shape, int x0, int y0, int x1, int y1);
    /// 请求清除闭区间外接矩形内的形状格子。
    void shapeEraseRequested(int shape, int x0, int y0, int x1, int y1);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;
    void rightPress(QMouseEvent* event, QGraphicsView* view) override;

private:
    void updateRubberBand();
    void clearRubberBand();

    Shape m_shape;
    bool m_dragging = false;
    bool m_erase = false;
    QPoint m_anchor;
    QPoint m_current;
    QGraphicsPathItem* m_rubberBand = nullptr;
};
