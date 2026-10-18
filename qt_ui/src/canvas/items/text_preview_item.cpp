#include "text_preview_item.h"

#include <QPainter>
#include <QPen>

TextPreviewItem::TextPreviewItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
    // 画在图纸图元之上；只是预览不接受鼠标（避免挡住画布交互）
    setZValue(20.0);
    setAcceptedMouseButtons(Qt::NoButton);
    setVisible(false);
}

void TextPreviewItem::setPreview(const QVector<QPoint>& relCells, qreal cellSize,
                                 const QColor& color)
{
    prepareGeometryChange();
    m_cells = relCells;
    m_cellSize = qMax(1.0, cellSize);
    m_color = color;
    m_bounds = QRectF();
    for (const QPoint& c : m_cells) {
        const qreal x = c.x() * m_cellSize;
        const qreal y = c.y() * m_cellSize;
        m_bounds = m_bounds.united(QRectF(x, y, m_cellSize, m_cellSize));
    }
    setVisible(!m_cells.isEmpty());
    update();
}

QRectF TextPreviewItem::boundingRect() const
{
    // 外扩 1px 容纳虚线描边
    return m_bounds.adjusted(-1.0, -1.0, 1.0, 1.0);
}

void TextPreviewItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    if (m_cells.isEmpty()) {
        return;
    }
    // 半透明豆子格：与真实涂色同形，但明显可辨为"预览"
    painter->setPen(Qt::NoPen);
    QColor fill = m_color;
    fill.setAlpha(110);
    painter->setBrush(fill);
    for (const QPoint& c : m_cells) {
        painter->drawRect(QRectF(c.x() * m_cellSize, c.y() * m_cellSize,
                                 m_cellSize, m_cellSize));
    }
    // 整体虚线框：强调这是待确认的预览
    QPen dash(m_color.isValid() ? m_color : QColor(60, 60, 60));
    dash.setStyle(Qt::DashLine);
    dash.setWidthF(1.5);
    painter->setPen(dash);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_bounds);
}
