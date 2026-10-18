#include "fill_tool.h"

#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsView>
#include <QMouseEvent>

FillTool::FillTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor FillTool::cursor() const
{
    return Qt::CrossCursor;
}

void FillTool::dispatch(QMouseEvent* event, QGraphicsView* view, bool erase)
{
    if (!view || !m_scene) {
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }
    if (erase) {
        emit fillEraseRequested(cell.x(), cell.y());
    } else {
        emit fillRequested(cell.x(), cell.y());
    }
}

void FillTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    dispatch(event, view, false);
}

void FillTool::rightPress(QMouseEvent* event, QGraphicsView* view)
{
    dispatch(event, view, true);
}

void FillTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    ToolBase::mouseMove(event, view);
}
