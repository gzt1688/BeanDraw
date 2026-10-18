#include "pan_tool.h"

#include <QCursor>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QScrollBar>

PanTool::PanTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor PanTool::cursor() const
{
    return Qt::OpenHandCursor;
}

void PanTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (view) {
        view->viewport()->setCursor(Qt::ClosedHandCursor);
    }
    m_panning = true;
    m_lastViewportPos = event->pos();
}

void PanTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (!m_panning || !view) {
        return;
    }
    const QPoint delta = event->pos() - m_lastViewportPos;
    m_lastViewportPos = event->pos();
    if (QScrollBar* hBar = view->horizontalScrollBar()) {
        hBar->setValue(hBar->value() - delta.x());
    }
    if (QScrollBar* vBar = view->verticalScrollBar()) {
        vBar->setValue(vBar->value() - delta.y());
    }
}

void PanTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    m_panning = false;
    if (view && view->viewport()) {
        view->viewport()->setCursor(cursor());
    }
}
