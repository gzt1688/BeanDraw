#pragma once

#include "canvas/tools/tool_base.h"

#include <QPoint>
#include <QVector>

class QGraphicsPathItem;

/// 多边形填充工具：左键逐点添加顶点（橡皮筋预览折线），
/// 右键或双击闭合多边形，把多边形内部格子填为当前色。
///
/// 顶点为十字绣格子坐标（格子中心判定在 MainWindow 侧）。
class PolygonFillTool : public ToolBase
{
    Q_OBJECT

public:
    explicit PolygonFillTool(QObject* parent = nullptr);
    ~PolygonFillTool() override;

    QString toolId() const override { return QStringLiteral("tool.polygon_fill"); }
    QCursor cursor() const override;

    void activate(QGraphicsView* view, CraftScene* scene) override;
    void deactivate() override;

signals:
    /// 请求填充多边形内部格子（vertices 为格子坐标，至少 3 个）。
    void polygonFillRequested(const QVector<QPoint>& vertices);
    /// 请求清空多边形内部格子。
    void polygonEraseRequested(const QVector<QPoint>& vertices);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;
    void mouseDoubleClick(QMouseEvent* event, QGraphicsView* view) override;
    void rightPress(QMouseEvent* event, QGraphicsView* view) override;

private:
    void addVertex(const QPoint& cell);
    void updateRubberBand();
    void clearRubberBand();
    void finishPolygon(bool erase);

    QVector<QPoint> m_vertices;
    QGraphicsPathItem* m_rubberBand = nullptr;
    QPoint m_hover;            ///< 鼠标悬停格子（悬空端点预览）
    bool m_hasHover = false;
};
