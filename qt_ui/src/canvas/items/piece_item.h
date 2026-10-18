#pragma once

#include "ffi_bind/ffi_types.h"

#include <QGraphicsObject>
#include <QVector>

/// 裁片图元：轮廓（实线）+ 缝份路径（虚线）。内部单位 mm。
class PieceItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit PieceItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void setPaths(const QVector<CmPoint>& outline, const QVector<CmPoint>& seam);

private:
    QPolygonF m_outline;
    QPolygonF m_seam;
};
