#include "polygon_fill_tool.h"

#include "canvas/items/cross_stitch_item.h"
#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPen>

namespace {
/// 橡皮筋折线颜色（与矩形/形状填充一致）。
QColor rubberBandColor()
{
    return QColor(216, 30, 6);
}
} // namespace

PolygonFillTool::PolygonFillTool(QObject* parent)
    : ToolBase(parent)
{
}

PolygonFillTool::~PolygonFillTool()
{
    clearRubberBand();
}

QCursor PolygonFillTool::cursor() const
{
    return Qt::CrossCursor;
}

void PolygonFillTool::activate(QGraphicsView* view, CraftScene* scene)
{
    ToolBase::activate(view, scene);
    if (!m_scene) {
        return;
    }
    m_rubberBand = new QGraphicsPathItem();
    QPen pen(rubberBandColor());
    pen.setStyle(Qt::DashLine);
    pen.setWidth(0);
    m_rubberBand->setPen(pen);
    m_rubberBand->setBrush(Qt::NoBrush);
    m_rubberBand->setZValue(10);
    m_rubberBand->setVisible(false);
    m_scene->addItem(m_rubberBand);
    m_vertices.clear();
}

void PolygonFillTool::deactivate()
{
    m_vertices.clear();
    clearRubberBand();
    ToolBase::deactivate();
}

void PolygonFillTool::clearRubberBand()
{
    if (!m_rubberBand) {
        return;
    }
    if (m_scene) {
        m_scene->removeItem(m_rubberBand);
    }
    delete m_rubberBand;
    m_rubberBand = nullptr;
}

void PolygonFillTool::addVertex(const QPoint& cell)
{
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }
    // 相邻重复顶点忽略
    if (!m_vertices.isEmpty() && m_vertices.last() == cell) {
        return;
    }
    m_vertices.append(cell);
    updateRubberBand();
}

void PolygonFillTool::updateRubberBand()
{
    if (!m_rubberBand || !m_scene) {
        return;
    }
    CrossStitchItem* item = m_scene->crossStitchItem();
    if (!item || m_vertices.isEmpty()) {
        m_rubberBand->setVisible(false);
        return;
    }
    QPainterPath path;
    path.moveTo(item->cellCenter(m_vertices.first()));
    for (int i = 1; i < m_vertices.size(); ++i) {
        path.lineTo(item->cellCenter(m_vertices.at(i)));
    }
    // 悬空端点跟随鼠标（预览下一段折线，不落顶点）
    if (m_hasHover && m_hover.x() >= 0 && m_hover.y() >= 0) {
        path.lineTo(item->cellCenter(m_hover));
    }
    m_rubberBand->setPath(path);
    m_rubberBand->setVisible(true);
}

void PolygonFillTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    addVertex(cell);
}

void PolygonFillTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    // 悬空端点跟随鼠标（仅用于预览，不落顶点）
    m_hover = m_scene->cellAt(view->mapToScene(event->pos()));
    m_hasHover = m_hover.x() >= 0 && m_hover.y() >= 0;
    updateRubberBand();
}

void PolygonFillTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

void PolygonFillTool::mouseDoubleClick(QMouseEvent* event, QGraphicsView* view)
{
    // 双击 = 闭合（第一击已通过 mousePress 落顶点）
    Q_UNUSED(event)
    Q_UNUSED(view)
    finishPolygon(false);
}

void PolygonFillTool::rightPress(QMouseEvent* event, QGraphicsView* view)
{
    // 右键 = 闭合并清除多边形内部
    Q_UNUSED(event)
    Q_UNUSED(view)
    if (!m_scene) {
        return;
    }
    finishPolygon(true);
}

void PolygonFillTool::finishPolygon(bool erase)
{
    if (m_vertices.size() < 3) {
        // 顶点不足：重置本轮
        m_vertices.clear();
        updateRubberBand();
        return;
    }
    if (erase) {
        emit polygonEraseRequested(m_vertices);
    } else {
        emit polygonFillRequested(m_vertices);
    }
    m_vertices.clear();
    updateRubberBand();
}
