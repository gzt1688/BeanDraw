#include "eraser_tool.h"

#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsView>
#include <QMouseEvent>

EraserTool::EraserTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor EraserTool::cursor() const
{
    return Qt::CrossCursor;
}

void EraserTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }
    m_erasing = true;
    m_lastCell = cell;
    emit strokeStarted();
    emit cellEraseRequested(cell.x(), cell.y());
}

void EraserTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (!m_erasing || !view || !m_scene) {
        ToolBase::mouseMove(event, view);
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0 || cell == m_lastCell) {
        return;
    }
    m_lastCell = cell;
    emit cellEraseRequested(cell.x(), cell.y());
}

void EraserTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
    if (!m_erasing) {
        return;
    }
    m_erasing = false;
    emit strokeFinished();
}
