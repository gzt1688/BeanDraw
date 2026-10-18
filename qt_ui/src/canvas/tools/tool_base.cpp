#include "tool_base.h"

#include "canvas/scene.h"

#include <QCursor>
#include <QEvent>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QKeyEvent>

ToolBase::ToolBase(QObject* parent)
    : QObject(parent)
{
}

ToolBase::~ToolBase()
{
    deactivate();
}

void ToolBase::activate(QGraphicsView* view, CraftScene* scene)
{
    deactivate();
    m_view = view;
    m_scene = scene;
    if (view && view->viewport()) {
        view->viewport()->installEventFilter(this);
        // 键盘事件目标在 QGraphicsView 自身（QAbstractScrollArea 焦点），
        // 必须也在 view 上挂过滤器，才能先于"方向键滚动画布"处理按键。
        view->installEventFilter(this);
        view->viewport()->setCursor(cursor());
    }
}

void ToolBase::deactivate()
{
    if (m_view && m_view->viewport()) {
        m_view->viewport()->removeEventFilter(this);
        m_view->removeEventFilter(this);
    }
    m_view = nullptr;
    m_scene = nullptr;
}

QCursor ToolBase::cursor() const
{
    return Qt::ArrowCursor;
}

bool ToolBase::eventFilter(QObject* obj, QEvent* event)
{
    if (!m_view || !m_scene) {
        return false;
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::MiddleButton) {
            // CAD 式中键平移，任何工具下都可用
            m_middlePanning = true;
            m_lastPanPos = e->pos();
            if (m_view->viewport()) {
                m_view->viewport()->setCursor(Qt::ClosedHandCursor);
            }
            return true;
        }
        if (e->button() == Qt::RightButton) {
            m_rightDragging = true;
            rightPress(e, m_view);
            return true;
        }
        if (e->button() == Qt::LeftButton) {
            const QPointF pos = scenePos(m_view, e->pos());
            // 标题栏交互（X 删除 / 格子数修改）优先：图元消费后工具不再处理
            if (m_scene && m_scene->handleItemPress(pos)) {
                return true;
            }
            // 点击盘即激活：先上报点击所在盘（非激活盘 → 场景发出激活请求），
            // 再交给当前工具处理（激活后同一格点击仍会落到工具，可立即编辑）
            if (m_scene) {
                m_scene->reportPlateClick(pos);
            }
            mousePress(e, m_view);
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* e = static_cast<QMouseEvent*>(event);
        if (m_rightDragging) {
            rightMove(e, m_view);
            return false; // 不吞移动事件，保证 hover 正常
        }
        if (m_middlePanning) {
            const QPoint delta = e->pos() - m_lastPanPos;
            m_lastPanPos = e->pos();
            if (QScrollBar* hBar = m_view->horizontalScrollBar()) {
                hBar->setValue(hBar->value() - delta.x());
            }
            if (QScrollBar* vBar = m_view->verticalScrollBar()) {
                vBar->setValue(vBar->value() - delta.y());
            }
            return true;
        }
        mouseMove(e, m_view);
        return false; // 移动事件不吞掉，保证 hover 正常
    }
    case QEvent::MouseButtonRelease: {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::MiddleButton) {
            m_middlePanning = false;
            if (m_view->viewport()) {
                m_view->viewport()->setCursor(cursor());
            }
            return true;
        }
        if (e->button() == Qt::LeftButton) {
            mouseRelease(e, m_view);
            return true;
        }
        if (e->button() == Qt::RightButton) {
            m_rightDragging = false;
            rightRelease(e, m_view);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonDblClick: {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton) {
            mouseDoubleClick(e, m_view);
            return true;
        }
        break;
    }
    case QEvent::Wheel:
        wheel(static_cast<QWheelEvent*>(event), m_view);
        return true;
    case QEvent::KeyPress: {
        auto* e = static_cast<QKeyEvent*>(event);
        keyPress(e, m_view);
        // 工具已 accept 的按键视为处理完成，不再向下传递——
        // 否则方向键会落到 QGraphicsView 默认滚动画布（与移动选区冲突）
        return e->isAccepted();
    }
    default:
        break;
    }
    return false;
}

void ToolBase::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

void ToolBase::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (m_scene && view) {
        m_scene->reportHover(view->mapToScene(event->pos()));
    }
}

void ToolBase::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

void ToolBase::mouseDoubleClick(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

void ToolBase::wheel(QWheelEvent* event, QGraphicsView* view)
{
    if (!view) {
        return;
    }
    if (event->angleDelta().y() == 0) {
        return;
    }
    constexpr double kStep = 1.15;
    const double factor = event->angleDelta().y() > 0 ? kStep : 1.0 / kStep;
    // CAD 式锚点缩放：保持鼠标下的场景点不动。
    // 滚动条 AlwaysOff 时 AnchorUnderMouse 锚点失效（退化为视口中心缩放），
    // 因此手动记录锚点、缩放后微调滚动条把锚点拉回鼠标位置——内容不跳、不滑走。
    const QPointF anchor = view->mapToScene(event->position().toPoint());
    view->scale(factor, factor);
    const QPointF after = view->mapFromScene(anchor);
    const QPointF delta = event->position() - after;
    auto* hbar = view->horizontalScrollBar();
    auto* vbar = view->verticalScrollBar();
    hbar->setValue(hbar->value() + qRound(delta.x()));
    vbar->setValue(vbar->value() + qRound(delta.y()));
}

void ToolBase::rightPress(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

void ToolBase::rightMove(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

void ToolBase::rightRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

void ToolBase::keyPress(QKeyEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
}

QPointF ToolBase::scenePos(QGraphicsView* view, const QPoint& viewportPos)
{
    if (!view) {
        return QPointF();
    }
    return view->mapToScene(viewportPos);
}
