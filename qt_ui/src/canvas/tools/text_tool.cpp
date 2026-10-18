#include "text_tool.h"

#include "canvas/scene.h"

#include <QCursor>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>

TextTool::TextTool(QObject* parent)
    : ToolBase(parent)
{
}

QCursor TextTool::cursor() const
{
    return Qt::IBeamCursor;
}

void TextTool::mousePress(QMouseEvent* event, QGraphicsView* view)
{
    if (!view || !m_scene) {
        return;
    }
    // 点击格 = 预览文字的左上角锚点（已有预览时点击其他格 = 移动预览）
    const QPoint cell = m_scene->cellAt(view->mapToScene(event->pos()));
    if (cell.x() < 0 || cell.y() < 0) {
        // 点击图纸外：取消预览
        emit textCancelRequested();
        return;
    }
    emit textAnchorRequested(cell.x(), cell.y());
}

void TextTool::keyPress(QKeyEvent* event, QGraphicsView* view)
{
    Q_UNUSED(view)
    if (event->key() == Qt::Key_Escape) {
        emit textCancelRequested();
    }
}
