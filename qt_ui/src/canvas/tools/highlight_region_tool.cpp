#include "highlight_region_tool.h"

#include "canvas/items/cross_stitch_item.h"
#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>

HighlightRegionTool::HighlightRegionTool(QObject* parent)
    : ToolBase(parent)
{
}

HighlightRegionTool::~HighlightRegionTool()
{
    clearHighlight();
}

QCursor HighlightRegionTool::cursor() const
{
    return Qt::PointingHandCursor;
}

void HighlightRegionTool::deactivate()
{
    clearHighlight();
    ToolBase::deactivate();
}

void HighlightRegionTool::clearHighlight()
{
    if (!m_scene) {
        return;
    }
    if (CrossStitchItem* item = m_scene->crossStitchItem()) {
        item->clearHighlight();
    }
}

void HighlightRegionTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (event->button() == Qt::LeftButton) {
        if (!m_scene) {
            return;
        }
        const QPointF scenePos = view->mapToScene(event->pos());
        const QPoint cell = m_scene->cellAt(scenePos);
        if (cell.x() < 0 || cell.y() < 0) {
            return; // 点击盘外/留边区：保持现有高亮
        }
        emit highlightRequested(cell.x(), cell.y());
        return;
    }
    if (event->button() == Qt::RightButton) {
        clearHighlight();
        if (view && view->viewport()) {
            view->viewport()->update();
        }
    }
}

void HighlightRegionTool::keyPress(QKeyEvent* event, QGraphicsView* view)
{
    if (event->key() == Qt::Key_Escape) {
        clearHighlight();
        if (view && view->viewport()) {
            view->viewport()->update();
        }
        event->accept();
        return;
    }
    ToolBase::keyPress(event, view);
}
