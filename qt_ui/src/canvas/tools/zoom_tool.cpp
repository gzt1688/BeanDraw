#include "zoom_tool.h"

#include <QCursor>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>

namespace {
constexpr double kZoomStep = 1.15;
}

ZoomTool::ZoomTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor ZoomTool::cursor() const
{
    return Qt::CrossCursor;
}

void ZoomTool::wheel(QWheelEvent* event, QGraphicsView* view)
{
    if (!view) {
        return;
    }
    const double factor = event->angleDelta().y() > 0 ? kZoomStep : 1.0 / kZoomStep;
    view->scale(factor, factor);
}

void ZoomTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view) {
        return;
    }
    const bool zoomIn = !(event->modifiers() & Qt::AltModifier);
    view->scale(zoomIn ? kZoomStep : 1.0 / kZoomStep, zoomIn ? kZoomStep : 1.0 / kZoomStep);
}
