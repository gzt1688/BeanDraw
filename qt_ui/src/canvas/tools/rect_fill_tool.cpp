#include "rect_fill_tool.h"

#include "canvas/items/cross_stitch_item.h"
#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPen>

namespace {
/// 橡皮筋颜色（深浅主题下都醒目的朱红）。
QColor rubberBandColor()
{
    return QColor(216, 30, 6);
}
}

RectFillTool::RectFillTool(QObject* parent)
    : ToolBase(parent)
{
}

RectFillTool::~RectFillTool()
{
    clearRubberBand();
}

QCursor RectFillTool::cursor() const
{
    return Qt::CrossCursor;
}

void RectFillTool::activate(QGraphicsView* view, CraftScene* scene)
{
    ToolBase::activate(view, scene);
    if (!m_scene) {
        return;
    }
    m_rubberBand = new QGraphicsRectItem();
    QPen pen(rubberBandColor());
    pen.setStyle(Qt::DashLine);
    pen.setWidth(0);
    m_rubberBand->setPen(pen);
    m_rubberBand->setBrush(QColor(216, 30, 6, 40));
    m_rubberBand->setZValue(10);
    m_rubberBand->setVisible(false);
    m_scene->addItem(m_rubberBand);
}

void RectFillTool::deactivate()
{
    clearRubberBand();
    ToolBase::deactivate();
}

void RectFillTool::clearRubberBand()
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

void RectFillTool::updateRubberBand()
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
    m_rubberBand->setRect(QRectF(tl, br));
    m_rubberBand->setVisible(true);
}

void RectFillTool::mousePress(QMouseEvent* event, QGraphicsView* view)
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

void RectFillTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
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

void RectFillTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
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
        emit rectEraseRequested(x0, y0, x1, y1);
    } else {
        emit rectFillRequested(x0, y0, x1, y1);
    }
    if (m_rubberBand) {
        m_rubberBand->setVisible(false);
    }
}

void RectFillTool::rightPress(QMouseEvent* event, QGraphicsView* view)
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
    emit rectEraseRequested(cell.x(), cell.y(), cell.x(), cell.y());
}
