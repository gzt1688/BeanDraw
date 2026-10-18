#include "stitch_tool.h"

#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsView>
#include <QMouseEvent>

StitchTool::StitchTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor StitchTool::cursor() const
{
    return Qt::PointingHandCursor;
}

bool StitchTool::emitCellAt(QMouseEvent* event, QGraphicsView* view, bool erase)
{
    if (!view || !m_scene) {
        return false;
    }
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        return false;
    }
    // 拖拽时同一格不重复上报，避免大量无效命令与重绘
    if (m_strokeActive && cell == m_lastCell && erase == m_lastWasErase) {
        return true;
    }
    m_lastCell = cell;
    m_lastWasErase = erase;

    if (erase) {
        emit cellEraseRequested(cell.x(), cell.y());
    } else {
        emit cellPaintRequested(cell.x(), cell.y());
    }
    return true;
}

void StitchTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    m_strokeActive = true;
    m_lastCell = QPoint(-1, -1);
    m_lastWasErase = false;
    if (emitCellAt(event, view, false)) {
        emit strokeStarted();
    }
}

void StitchTool::mouseMove(QMouseEvent* event, QGraphicsView* view)
{
    if (!m_strokeActive) {
        ToolBase::mouseMove(event, view); // 未落笔：仅上报悬停坐标
        return;
    }
    emitCellAt(event, view, false);
}

void StitchTool::mouseRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
    if (!m_strokeActive) {
        return;
    }
    m_strokeActive = false;
    emit strokeFinished();
}

void StitchTool::rightPress(QMouseEvent* event, QGraphicsView* view)
{
    // 右键擦除：与左键涂色对称的笔画（拖拽期间持续擦除，松开时合并一条撤销记录）
    m_strokeActive = true;
    m_lastCell = QPoint(-1, -1);
    m_lastWasErase = true;
    if (emitCellAt(event, view, true)) {
        emit strokeStarted();
    }
}

void StitchTool::rightMove(QMouseEvent* event, QGraphicsView* view)
{
    if (m_strokeActive) {
        emitCellAt(event, view, true);
    }
}

void StitchTool::rightRelease(QMouseEvent* event, QGraphicsView* view)
{
    Q_UNUSED(event)
    Q_UNUSED(view)
    if (!m_strokeActive) {
        return;
    }
    m_strokeActive = false;
    emit strokeFinished();
}
