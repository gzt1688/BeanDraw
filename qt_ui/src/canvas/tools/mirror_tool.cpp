#include "mirror_tool.h"

#include "canvas/items/cross_stitch_item.h"
#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPen>

namespace {
/// 高亮色：与选区橡皮筋一致的朱红（深浅主题下都醒目）。
QColor axisColor()
{
    return QColor(216, 30, 6);
}
/// 贯穿线命中阈值（格边长比例；半格内算命中）。
const qreal kAxisSnapRatio = 0.35;
/// 贯穿线宽：很细（场景单位，贴格子线不遮挡内容）。
const qreal kAxisLineWidth = 1.0;
} // namespace

MirrorTool::MirrorTool(QObject* parent)
    : ToolBase(parent)
{
}

MirrorTool::~MirrorTool()
{
    clearStateInternal();
}

QCursor MirrorTool::cursor() const
{
    return Qt::CrossCursor;
}

void MirrorTool::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    clearStateInternal();
}

void MirrorTool::activate(QGraphicsView* view, CraftScene* scene)
{
    ToolBase::activate(view, scene);
    if (!m_scene) {
        return;
    }
    m_axisItem = new QGraphicsLineItem();
    QPen pen(axisColor());
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(kAxisLineWidth);
    m_axisItem->setPen(pen);
    m_axisItem->setZValue(12);
    m_axisItem->setVisible(false);
    m_scene->addItem(m_axisItem);

    m_pointMark = new QGraphicsRectItem();
    m_pointMark->setRect(-4, -4, 8, 8);
    QPen mp(axisColor());
    mp.setWidthF(1.5);
    m_pointMark->setPen(mp);
    m_pointMark->setBrush(Qt::white);
    m_pointMark->setZValue(12);
    m_pointMark->setVisible(false);
    m_scene->addItem(m_pointMark);

    m_previewLine = new QGraphicsLineItem();
    QPen pp(axisColor());
    pp.setStyle(Qt::DashLine);
    pp.setWidthF(1.5);
    m_previewLine->setPen(pp);
    m_previewLine->setZValue(12);
    m_previewLine->setVisible(false);
    m_scene->addItem(m_previewLine);

    // 当前选区只读蒙层（提示镜像范围；镜像基于选区，不选范围则点轴不生效）
    m_selOverlay = new QGraphicsRectItem();
    QPen op(axisColor());
    op.setStyle(Qt::DashLine);
    op.setWidth(0);
    m_selOverlay->setPen(op);
    m_selOverlay->setBrush(QColor(216, 30, 6, 30));
    m_selOverlay->setZValue(10);
    m_selOverlay->setVisible(false);
    m_scene->addItem(m_selOverlay);
}

void MirrorTool::deactivate()
{
    if (m_scene) {
        if (m_axisItem) {
            m_scene->removeItem(m_axisItem);
        }
        if (m_pointMark) {
            m_scene->removeItem(m_pointMark);
        }
        if (m_previewLine) {
            m_scene->removeItem(m_previewLine);
        }
        if (m_selOverlay) {
            m_scene->removeItem(m_selOverlay);
        }
    }
    delete m_axisItem;
    delete m_pointMark;
    delete m_previewLine;
    delete m_selOverlay;
    m_axisItem = nullptr;
    m_pointMark = nullptr;
    m_previewLine = nullptr;
    m_selOverlay = nullptr;
    clearStateInternal();
    ToolBase::deactivate();
}

void MirrorTool::setSelection(const QRect& cells)
{
    m_selection = cells.isValid() && cells.width() > 0 && cells.height() > 0
                      ? cells.normalized()
                      : QRect();
    updateSelectionOverlay();
}

void MirrorTool::updateSelectionOverlay()
{
    if (!m_selOverlay || !m_scene) {
        return;
    }
    if (m_selection.isNull()) {
        m_selOverlay->setVisible(false);
        return;
    }
    CrossStitchItem* item = m_scene->crossStitchItem();
    if (!item) {
        m_selOverlay->setVisible(false);
        return;
    }
    const QPointF tl = item->cellRectTopLeft(m_selection.left(), m_selection.top());
    const QPointF br = item->cellRectBottomRight(m_selection.right(), m_selection.bottom());
    m_selOverlay->setRect(QRectF(tl, br));
    m_selOverlay->setVisible(true);
}

void MirrorTool::clearState()
{
    clearStateInternal();
}

void MirrorTool::setAxisLock(int kind)
{
    m_axisLock = kind;
    clearStateInternal();
}

void MirrorTool::clearStateInternal()
{
    m_hasFirst = false;
    m_hit = AxisHit();
    if (m_axisItem) {
        m_axisItem->setVisible(false);
    }
    if (m_pointMark) {
        m_pointMark->setVisible(false);
    }
    if (m_previewLine) {
        m_previewLine->setVisible(false);
    }
}

QPointF MirrorTool::gridPos(const QPointF& scenePos) const
{
    CrossStitchItem* item = m_scene ? m_scene->crossStitchItem() : nullptr;
    if (!item) {
        return QPointF(-1, -1);
    }
    const QPointF origin = item->cellRectTopLeft(0, 0);
    const qreal cs = item->cellSize();
    return QPointF((scenePos.x() - origin.x()) / cs, (scenePos.y() - origin.y()) / cs);
}

/// 最接近鼠标的贯穿格线（竖线贯穿整个激活盘上下、横线贯穿左右）。
MirrorTool::AxisHit MirrorTool::detectAxis(const QPointF& scenePos)
{
    AxisHit hit;
    if (!m_scene) {
        return hit;
    }
    CrossStitchItem* item = m_scene->crossStitchItem();
    const QPoint cell = m_scene->cellAt(scenePos);
    if (!item || cell.x() < 0 || cell.y() < 0) {
        return hit;
    }
    const int w = item->activeGridWidth();
    const int h = item->activeGridHeight();
    if (w <= 0 || h <= 0) {
        return hit;
    }
    const qreal cs = item->cellSize();
    const qreal snap = cs * kAxisSnapRatio;
    const QPointF topLeft = item->cellRectTopLeft(0, 0);
    const QPointF bottomRight = item->cellRectBottomRight(w - 1, h - 1);

    // 键盘锁定：只识别锁定方向的贯穿线（X=横线 / Y=竖线）
    if (m_axisLock == AxisHorizontal) {
        const qreal fy = (scenePos.y() - topLeft.y()) / cs;
        const int ky = qRound(fy);
        if (ky >= 0 && ky <= h) {
            const qreal ly = topLeft.y() + ky * cs;
            if (qAbs(scenePos.y() - ly) < snap) {
                hit.kind = AxisHorizontal;
                hit.k = ky;
                hit.line = QLineF(topLeft.x(), ly, bottomRight.x(), ly);
                return hit;
            }
        }
        return hit;
    }
    if (m_axisLock == AxisVertical) {
        const qreal fx = (scenePos.x() - topLeft.x()) / cs;
        const int kx = qRound(fx);
        if (kx >= 0 && kx <= w) {
            const qreal lx = topLeft.x() + kx * cs;
            if (qAbs(scenePos.x() - lx) < snap) {
                hit.kind = AxisVertical;
                hit.k = kx;
                hit.line = QLineF(lx, topLeft.y(), lx, bottomRight.y());
                return hit;
            }
        }
        return hit;
    }
    // 自动 XY：先竖线再横线
    // 最近竖线：格线 k 场景 x = topLeft.x() + k*cs
    const qreal fx = (scenePos.x() - topLeft.x()) / cs;
    const int kx = qRound(fx);
    if (kx >= 0 && kx <= w) {
        const qreal lx = topLeft.x() + kx * cs;
        if (qAbs(scenePos.x() - lx) < snap) {
            hit.kind = AxisVertical;
            hit.k = kx;
            hit.line = QLineF(lx, topLeft.y(), lx, bottomRight.y());
            return hit;
        }
    }
    // 最近横线
    const qreal fy = (scenePos.y() - topLeft.y()) / cs;
    const int ky = qRound(fy);
    if (ky >= 0 && ky <= h) {
        const qreal ly = topLeft.y() + ky * cs;
        if (qAbs(scenePos.y() - ly) < snap) {
            hit.kind = AxisHorizontal;
            hit.k = ky;
            hit.line = QLineF(topLeft.x(), ly, bottomRight.x(), ly);
            return hit;
        }
    }
    return hit;
}

void MirrorTool::updateAxisItem(const AxisHit& hit)
{
    if (!m_axisItem) {
        return;
    }
    if (hit.kind == AxisNone) {
        m_axisItem->setVisible(false);
        return;
    }
    m_axisItem->setLine(hit.line);
    m_axisItem->setVisible(true);
}

void MirrorTool::updatePreview(const QPointF& scenePos)
{
    if (!m_previewLine || !m_hasFirst) {
        return;
    }
    // 预览线从第一点连到当前鼠标（场景坐标）
    m_previewLine->setLine(QLineF(m_pointMark->pos(), scenePos));
    m_previewLine->setVisible(true);
}

void MirrorTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    // 镜像基于选区：未框选时不响应单击（MainWindow 激活时已提示）
    if (m_selection.isNull()) {
        return;
    }
    const QPointF scenePos = view->mapToScene(event->pos());
    const QPoint cell = m_scene->cellAt(scenePos);
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }

    if (m_mode == TwoPoints) {
        if (m_hasFirst) {
            // 第二点：两点连线为轴
            const QPointF g2 = gridPos(scenePos);
            if (g2.x() >= 0 && g2.y() >= 0 && (g2 - m_firstGrid).manhattanLength() > 0.05) {
                emit mirrorAxisRequested(m_firstGrid.x(), m_firstGrid.y(), g2.x(), g2.y());
            }
            clearStateInternal();
            return;
        }
        // 第一点
        m_hasFirst = true;
        m_firstGrid = gridPos(scenePos);
        if (m_pointMark) {
            m_pointMark->setPos(scenePos);
            m_pointMark->setVisible(true);
        }
        return;
    }

    // XyAxis：单击高亮贯穿线 → 以该线为轴（格子单位，竖线贯穿 0..h / 横线 0..w）
    if (m_hit.kind != AxisNone) {
        if (m_hit.kind == AxisVertical) {
            emit mirrorAxisRequested(m_hit.k, 0.0, m_hit.k, 1.0);
        } else {
            emit mirrorAxisRequested(0.0, m_hit.k, 1.0, m_hit.k);
        }
        clearStateInternal();
    }
}

void MirrorTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    if (!view || !m_scene) {
        ToolBase::mouseMove(event, view);
        return;
    }
    const QPointF scenePos = view->mapToScene(event->pos());
    if (m_mode == TwoPoints && m_hasFirst) {
        updatePreview(scenePos);
        return;
    }
    const AxisHit hit = detectAxis(scenePos);
    m_hit = hit;
    updateAxisItem(hit);
}

void MirrorTool::keyPress(QKeyEvent* event, QGraphicsView* view)
{
    if (event->key() == Qt::Key_Escape) {
        clearStateInternal();
        event->accept();
        return;
    }
    // X / Y 锁定轴方向：按 X 只识别横线（X 轴），按 Y 只识别竖线（Y 轴）；
    // 再按同一键取消锁定，恢复 XY 自动识别
    if (event->key() == Qt::Key_X) {
        m_axisLock = (m_axisLock == AxisHorizontal) ? AxisNone : AxisHorizontal;
        clearStateInternal();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Y) {
        m_axisLock = (m_axisLock == AxisVertical) ? AxisNone : AxisVertical;
        clearStateInternal();
        event->accept();
        return;
    }
    ToolBase::keyPress(event, view);
}
