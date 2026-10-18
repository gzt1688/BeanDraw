#include "piece_item.h"

#include <QPainter>
#include <QPen>

PieceItem::PieceItem(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    setZValue(1);
}

void PieceItem::setPaths(const QVector<CmPoint>& outline, const QVector<CmPoint>& seam)
{
    m_outline.clear();
    m_seam.clear();
    // 内部 mm → 场景单位：1mm = 1 场景单位（视图缩放适配纸样尺度）
    for (const CmPoint& p : outline) {
        m_outline << QPointF(p.x, p.y);
    }
    for (const CmPoint& p : seam) {
        m_seam << QPointF(p.x, p.y);
    }
    update();
}

QRectF PieceItem::boundingRect() const
{
    QRectF r;
    for (const QPointF& p : m_outline) {
        r |= QRectF(p, p);
    }
    for (const QPointF& p : m_seam) {
        r |= QRectF(p, p);
    }
    return r.adjusted(-5, -5, 5, 5);
}

void PieceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // 缝份路径（虚线）
    if (m_seam.size() >= 3) {
        QPen seamPen(QColor(120, 120, 200));
        seamPen.setStyle(Qt::DashLine);
        seamPen.setWidthF(0.5);
        painter->setPen(seamPen);
        painter->drawPolyline(m_seam);
        painter->drawLine(m_seam.last(), m_seam.first());
    }

    // 轮廓（实线 + 半透明填充）
    if (m_outline.size() >= 3) {
        QPen outlinePen(QColor(30, 30, 30));
        outlinePen.setWidthF(0.8);
        painter->setPen(outlinePen);
        painter->setBrush(QColor(200, 200, 220, 80));
        painter->drawPolygon(m_outline);
    }
}
