#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QVector>
#include <QPoint>

/// 文字工具预览项：「文字转豆子」的格子掩码在画布上的半透明覆盖层
/// （不入库）。项位置锚在起始格左上角（项原点 = 锚点格左上）；
/// 掩码为相对锚点格的偏移。参数实时刷新，确认后由主窗口写入内核。
class TextPreviewItem : public QGraphicsItem
{
public:
    explicit TextPreviewItem(QGraphicsItem* parent = nullptr);

    /// 更新掩码（相对锚点格的偏移）、格子边长与预览颜色；空掩码 = 隐藏。
    void setPreview(const QVector<QPoint>& relCells, qreal cellSize, const QColor& color);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

private:
    QVector<QPoint> m_cells;   ///< 相对锚点格的偏移
    qreal m_cellSize = 5.0;
    QColor m_color;
    QRectF m_bounds;           ///< 掩码包围盒（项局部坐标）
};
