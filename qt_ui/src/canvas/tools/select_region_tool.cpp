#include "select_region_tool.h"

#include "canvas/items/cross_stitch_item.h"
#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPen>

namespace {
/// 选区蒙层颜色（与填充橡皮筋一致的朱红，半透明）。
QColor overlayColor()
{
    return QColor(216, 30, 6);
}
const qreal kHandleSize = 4.0;   ///< 控制柄边长（场景单位，小巧不遮内容）
const qreal kHandleHitPad = 3.0; ///< 命中区外扩（视觉 4px 太小易误触，命中判定放宽）
} // namespace

SelectRegionTool::SelectRegionTool(QObject* parent)
    : ToolBase(parent)
{
}

SelectRegionTool::~SelectRegionTool()
{
    destroyHandles();
}

QCursor SelectRegionTool::cursor() const
{
    return Qt::CrossCursor;
}

void SelectRegionTool::activate(QGraphicsView* view, CraftScene* scene)
{
    ToolBase::activate(view, scene);
    if (!m_scene) {
        return;
    }
    m_overlay = new QGraphicsRectItem();
    QPen pen(overlayColor());
    pen.setStyle(Qt::DashLine);
    pen.setWidth(0);
    m_overlay->setPen(pen);
    m_overlay->setBrush(QColor(216, 30, 6, 30));
    m_overlay->setZValue(10);
    m_overlay->setVisible(false);
    m_scene->addItem(m_overlay);
    createHandles();
    if (m_active) {
        updateOverlay();
        updateHandles();
    }
}

void SelectRegionTool::deactivate()
{
    destroyHandles();
    if (m_overlay) {
        if (m_scene) {
            m_scene->removeItem(m_overlay);
        }
        delete m_overlay;
        m_overlay = nullptr;
    }
    ToolBase::deactivate();
}

void SelectRegionTool::createHandles()
{
    if (!m_scene || !m_handles.isEmpty()) {
        return;
    }
    for (int i = 0; i < 8; ++i) {
        auto* h = new QGraphicsRectItem();
        h->setRect(-kHandleSize / 2, -kHandleSize / 2, kHandleSize, kHandleSize);
        QPen pen(overlayColor());
        pen.setWidth(0);
        h->setPen(pen);
        h->setBrush(Qt::white);
        h->setZValue(11);
        h->setVisible(false);
        m_scene->addItem(h);
        m_handles.append(h);
    }
}

void SelectRegionTool::destroyHandles()
{
    for (auto* h : m_handles) {
        if (m_scene) {
            m_scene->removeItem(h);
        }
        delete h;
    }
    m_handles.clear();
}

void SelectRegionTool::clearSelection()
{
    if (!m_active && !m_dragging && !m_resizing) {
        return;
    }
    m_active = false;
    m_dragging = false;
    m_resizing = false;
    m_handle = HandleNone;
    if (m_overlay) {
        m_overlay->setVisible(false);
    }
    for (auto* h : m_handles) {
        h->setVisible(false);
    }
    emit selectionChanged(0, 0, 0, 0, false);
}

bool SelectRegionTool::cellValid(const QPoint& cell) const
{
    return cell.x() >= 0 && cell.y() >= 0;
}

void SelectRegionTool::setSelection(const QRect& sel)
{
    m_sel = sel.normalized();
    m_active = m_sel.isValid() && m_sel.width() > 0 && m_sel.height() > 0;
    updateOverlay();
    updateHandles();
    if (m_active) {
        emitSelectionChanged();
    }
}

void SelectRegionTool::updateOverlay()
{
    if (!m_overlay || !m_scene) {
        return;
    }
    if (!m_active) {
        m_overlay->setVisible(false);
        return;
    }
    CrossStitchItem* item = m_scene->crossStitchItem();
    if (!item) {
        m_overlay->setVisible(false);
        return;
    }
    const QPointF tl = item->cellRectTopLeft(m_sel.left(), m_sel.top());
    const QPointF br = item->cellRectBottomRight(m_sel.right(), m_sel.bottom());
    m_overlay->setRect(QRectF(tl, br));
    m_overlay->setVisible(true);
}

void SelectRegionTool::updateHandles()
{
    if (!m_scene || m_handles.size() != 8) {
        return;
    }
    CrossStitchItem* item = m_scene->crossStitchItem();
    if (!item || !m_active) {
        for (auto* h : m_handles) {
            h->setVisible(false);
        }
        return;
    }
    const QPointF tl = item->cellRectTopLeft(m_sel.left(), m_sel.top());
    const QPointF br = item->cellRectBottomRight(m_sel.right(), m_sel.bottom());
    const QPointF mid((tl.x() + br.x()) / 2.0, (tl.y() + br.y()) / 2.0);
    const QPointF pos[8] = {
        tl,                            // TL
        QPointF(br.x(), tl.y()),       // TR
        QPointF(tl.x(), br.y()),       // BL
        br,                            // BR
        QPointF(mid.x(), tl.y()),      // T
        QPointF(mid.x(), br.y()),      // B
        QPointF(tl.x(), mid.y()),      // L
        QPointF(br.x(), mid.y()),      // R
    };
    for (int i = 0; i < 8; ++i) {
        m_handles.at(i)->setPos(pos[i]);
        m_handles.at(i)->setVisible(true);
    }
}

void SelectRegionTool::emitSelectionChanged()
{
    emit selectionChanged(m_sel.left(), m_sel.top(), m_sel.right(), m_sel.bottom(), true);
}

int SelectRegionTool::handleAt(const QPointF& scenePos) const
{
    for (int i = 0; i < m_handles.size(); ++i) {
        if (m_handles.at(i)->isVisible()
            && m_handles.at(i)
                   ->sceneBoundingRect()
                   .adjusted(-kHandleHitPad, -kHandleHitPad, kHandleHitPad, kHandleHitPad)
                   .contains(scenePos)) {
            return i;
        }
    }
    return HandleNone;
}

void SelectRegionTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    const QPointF scenePos = view->mapToScene(event->pos());
    const int handle = handleAt(scenePos);
    if (handle != HandleNone) {
        // 控制柄：开始调整选区范围
        m_resizing = true;
        m_handle = handle;
        return;
    }
    // 画布外空白：取消选区（也可按 Esc 取消）
    const QPoint cell = m_scene->cellAt(scenePos);
    if (!cellValid(cell)) {
        if (m_sel.isValid()) {
            clearSelection();
        }
        return;
    }
    m_dragging = true;
    m_dragAnchor = cell;
    setSelection(QRect(cell, cell));
}

void SelectRegionTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    const QPointF scenePos = view->mapToScene(event->pos());
    if (m_resizing) {
        const QPoint cell = m_scene->cellAt(scenePos);
        if (!cellValid(cell)) {
            return;
        }
        QRect next = m_sel;
        switch (m_handle) {
        case HandleTL:
            next.setLeft(qMin(cell.x(), m_sel.right()));
            next.setTop(qMin(cell.y(), m_sel.bottom()));
            break;
        case HandleTR:
            next.setRight(qMax(cell.x(), m_sel.left()));
            next.setTop(qMin(cell.y(), m_sel.bottom()));
            break;
        case HandleBL:
            next.setLeft(qMin(cell.x(), m_sel.right()));
            next.setBottom(qMax(cell.y(), m_sel.top()));
            break;
        case HandleBR:
            next.setRight(qMax(cell.x(), m_sel.left()));
            next.setBottom(qMax(cell.y(), m_sel.top()));
            break;
        case HandleT:
            next.setTop(qMin(cell.y(), m_sel.bottom()));
            break;
        case HandleB:
            next.setBottom(qMax(cell.y(), m_sel.top()));
            break;
        case HandleL:
            next.setLeft(qMin(cell.x(), m_sel.right()));
            break;
        case HandleR:
            next.setRight(qMax(cell.x(), m_sel.left()));
            break;
        default:
            break;
        }
        if (next != m_sel) {
            m_sel = next;
            updateOverlay();
            updateHandles();
            emit scaleRegionPreview(m_sel.left(), m_sel.top(), m_sel.right(), m_sel.bottom());
        }
        return;
    }
    if (m_dragging) {
        const QPoint cell = m_scene->cellAt(scenePos);
        if (!cellValid(cell)) {
            return;
        }
        setSelection(QRect(m_dragAnchor, cell));
    }
}

void SelectRegionTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
    if (m_resizing) {
        m_resizing = false;
        m_handle = HandleNone;
        return;
    }
    if (m_dragging) {
        m_dragging = false;
        // 选区已通过 setSelection 发过 selectionChanged
    }
}

void SelectRegionTool::keyPress(QKeyEvent* event, QGraphicsView* view)
{
    if (event->key() == Qt::Key_Escape) {
        clearSelection();
        event->accept();
        return;
    }
    // 方向键移动选区内容（每按一次 1 格；无选区时不响应）
    if (m_active && !m_dragging && !m_resizing) {
        int dx = 0;
        int dy = 0;
        switch (event->key()) {
        case Qt::Key_Left:
            dx = -1;
            break;
        case Qt::Key_Right:
            dx = 1;
            break;
        case Qt::Key_Up:
            dy = -1;
            break;
        case Qt::Key_Down:
            dy = 1;
            break;
        default:
            break;
        }
        if (dx != 0 || dy != 0) {
            emit moveSelectionRequested(dx, dy);
            event->accept();
            return;
        }
    }
    ToolBase::keyPress(event, view);
}
