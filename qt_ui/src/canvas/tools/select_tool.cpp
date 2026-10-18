#include "select_tool.h"

#include <QCursor>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QScrollBar>

SelectTool::SelectTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor SelectTool::cursor() const
{
    return Qt::ArrowCursor;
}

void SelectTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    m_dragging = true;
    m_lastViewportPos = event->pos();
    Q_UNUSED(view)
}

void SelectTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (!m_dragging || !view) {
        return;
    }
    const QPoint delta = event->pos() - m_lastViewportPos;
    m_lastViewportPos = event->pos();

    // 平移视图滚动条（拖拽画布空白处平移）
    QScrollBar* hBar = view->horizontalScrollBar();
    QScrollBar* vBar = view->verticalScrollBar();
    if (hBar) {
        hBar->setValue(hBar->value() - delta.x());
    }
    if (vBar) {
        vBar->setValue(vBar->value() - delta.y());
    }
}

void SelectTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
    m_dragging = false;
}
