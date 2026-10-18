#include "picker_tool.h"

#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsView>
#include <QMouseEvent>

PickerTool::PickerTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor PickerTool::cursor() const
{
    return Qt::WhatsThisCursor;
}

void PickerTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }
    emit colorPicked(cell.x(), cell.y());
}

void PickerTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    // 按住拖动时连续拾取（（x,y）未变时由接收方自行去重）
    ToolBase::mouseMove(event, view);
}
