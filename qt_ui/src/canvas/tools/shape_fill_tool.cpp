#include "shape_fill_tool.h"

#include "canvas/items/cross_stitch_item.h"
#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPen>

namespace {
/// 橡皮筋颜色（深浅主题下都醒目的朱红，与矩形填充一致）。
QColor rubberBandColor()
{
    return QColor(216, 30, 6);
}
} // namespace

ShapeFillTool::ShapeFillTool(Shape shape, QObject* parent)
    : ToolBase(parent)
    , m_shape(shape)
{
}

ShapeFillTool::~ShapeFillTool()
{
    clearRubberBand();
}

QString ShapeFillTool::toolId() const
{
    return m_shape == Circle ? QStringLiteral("tool.circle_fill")
                             : QStringLiteral("tool.ellipse_fill");
}

QCursor ShapeFillTool::cursor() const
{
    return Qt::CrossCursor;
}

void ShapeFillTool::activate(QGraphicsView* view, CraftScene* scene)
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
    m_rubberBand->setBrush(QColor(216, 30, 6, 40));
    m_rubberBand->setZValue(10);
    m_rubberBand->setVisible(false);
    m_scene->addItem(m_rubberBand);
}

void ShapeFillTool::deactivate()
{
    clearRubberBand();
    ToolBase::deactivate();
}

void ShapeFillTool::clearRubberBand()
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

void ShapeFillTool::updateRubberBand()
{
    if (!m_rubberBand || !m_scene) {
        return;
    }
    CrossStitchItem* item = m_scene->crossStitchItem();
    if (!item) {
        m_rubberBand->setVisible(false);
        return;
    }
    const QPointF tl = item->cellRectTopLeft(qMin(m_anchor.x(), m_current.x()),
                                             qMin(m_anchor.y(), m_current.y()));
    const QPointF br = item->cellRectBottomRight(qMax(m_anchor.x(), m_current.x()),
                                                 qMax(m_anchor.y(), m_current.y()));
    QPainterPath path;
    if (m_shape == Circle) {
        // 圆形 = 外接正方形内切圆（半径 = 短边一半，与 MainWindow::circleCells 一致）
        const qreal w = br.x() - tl.x();
        const qreal h = br.y() - tl.y();
        const qreal r = qMin(w, h) / 2.0;
        const QPointF c((tl.x() + br.x()) / 2.0, (tl.y() + br.y()) / 2.0);
        path.addEllipse(QRectF(c.x() - r, c.y() - r, r * 2.0, r * 2.0));
    } else {
        // 椭圆 = 外接矩形内完整椭圆（与 MainWindow::ellipseCells 一致）
        path.addEllipse(QRectF(tl, br));
    }
    m_rubberBand->setPath(path);
    m_rubberBand->setVisible(true);
}

void ShapeFillTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }
    m_dragging = true;
    m_erase = false;
    m_anchor = cell;
    m_current = cell;
    updateRubberBand();
}

void ShapeFillTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (!m_dragging) {
        ToolBase::mouseMove(event, view);
        return;
    }
    if (!view || !m_scene) {
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }
    m_current = cell;
    updateRubberBand();
}

void ShapeFillTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    const int x0 = qMin(m_anchor.x(), m_current.x());
    const int y0 = qMin(m_anchor.y(), m_current.y());
    const int x1 = qMax(m_anchor.x(), m_current.x());
    const int y1 = qMax(m_anchor.y(), m_current.y());
    if (m_erase) {
        emit shapeEraseRequested(static_cast<int>(m_shape), x0, y0, x1, y1);
    } else {
        emit shapeFillRequested(static_cast<int>(m_shape), x0, y0, x1, y1);
    }
    if (m_rubberBand) {
        m_rubberBand->setVisible(false);
    }
}

void ShapeFillTool::rightPress(QMouseEvent* event, QGraphicsView* view)
{
    // 右键拖拽：整块清除
    if (!view || !m_scene) {
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }
    m_anchor = cell;
    m_current = cell;
    m_erase = true;
    emit shapeEraseRequested(static_cast<int>(m_shape), cell.x(), cell.y(), cell.x(), cell.y());
}
