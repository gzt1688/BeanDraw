#include "icon_factory.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPen>
#include <QRectF>
#include <QtMath>

#include <cmath>

namespace {

/// 设计网格：所有图标都在 24×24 的坐标系里作画，`make()` 再等比缩放到实际像素。
constexpr qreal kGrid = 24.0;

/// 图标内容区：1.8 ~ 22.2，占网格约 85%。
///
/// 工具栏按钮的实际可见面积不大，图形尽量铺满按钮框。
/// 所有图形的极值都落在这两个数上。
constexpr qreal kInset = 1.8;
constexpr qreal kSpan = kGrid - kInset * 2; // 20.4

/// 统一描边宽度（逻辑像素）。所有线性图标共用，保证一组图标视觉重量一致。
constexpr qreal kStroke = 1.9;

/// 统一构造描边笔：圆头圆角，让线条在 32px 下不显生硬。
QPen strokePen(const QPainter& p, qreal width = kStroke)
{
    QPen pen(p.brush().color());
    pen.setWidthF(width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

/// 圆角矩形路径（用于色块视图、图片框这类"卡片"形图标）。
QPainterPath roundedRect(const QRectF& r, qreal radius)
{
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    return path;
}

/// 四角内收的"适应窗口"标记：一段折线，拐角在矩形内侧。
void drawCornerMarks(QPainter& p, const QRectF& box, qreal len, qreal gap)
{
    const qreal x0 = box.left() + gap;
    const qreal y0 = box.top() + gap;
    const qreal x1 = box.right() - gap;
    const qreal y1 = box.bottom() - gap;
    p.drawLine(QPointF(x0, y0 + len), QPointF(x0, y0));
    p.drawLine(QPointF(x0, y0), QPointF(x0 + len, y0));
    p.drawLine(QPointF(x1 - len, y0), QPointF(x1, y0));
    p.drawLine(QPointF(x1, y0), QPointF(x1, y0 + len));
    p.drawLine(QPointF(x0, y1 - len), QPointF(x0, y1));
    p.drawLine(QPointF(x0, y1), QPointF(x0 + len, y1));
    p.drawLine(QPointF(x1 - len, y1), QPointF(x1, y1));
    p.drawLine(QPointF(x1, y1 - len), QPointF(x1, y1));
}

/// 选择：经典箭头光标，尖端在左上，尾部带折角（标准鼠标指针轮廓）。
void drawSelect(QPainter& p)
{
    QPainterPath path;
    path.moveTo(6.4, 2.6);
    path.lineTo(6.4, 18.4);   // 左竖边
    path.lineTo(10.0, 14.9);
    path.lineTo(12.6, 20.4);  // 右下折角
    path.lineTo(15.3, 19.2);
    path.lineTo(12.8, 13.8);
    path.lineTo(17.4, 13.4);  // 右横边
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.fillPath(path, p.brush());
}

/// 平移：四向箭头（比手形轮廓在 30px 下更清晰，符合"移动视图"语义）。
void drawPan(QPainter& p)
{
    const QBrush brush = p.brush();
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    // 上下左右四支实心箭头 + 中心圆点
    const qreal cx = 12.0, cy = 12.0;
    const qreal head = 3.8;   // 箭头头部高度
    const qreal stem = 3.2;   // 从箭头根部到中心的杆长
    // 上
    QPainterPath up;
    up.moveTo(cx - head, cy - stem);
    up.lineTo(cx + head, cy - stem);
    up.lineTo(cx, cy - stem - head);
    up.closeSubpath();
    p.drawPath(up);
    // 下
    QPainterPath down;
    down.moveTo(cx - head, cy + stem);
    down.lineTo(cx + head, cy + stem);
    down.lineTo(cx, cy + stem + head);
    down.closeSubpath();
    p.drawPath(down);
    // 左
    QPainterPath left;
    left.moveTo(cx - stem, cy - head);
    left.lineTo(cx - stem, cy + head);
    left.lineTo(cx - stem - head, cy);
    left.closeSubpath();
    p.drawPath(left);
    // 右
    QPainterPath right;
    right.moveTo(cx + stem, cy - head);
    right.lineTo(cx + stem, cy + head);
    right.lineTo(cx + stem + head, cy);
    right.closeSubpath();
    p.drawPath(right);
    // 中心圆点
    p.drawEllipse(QPointF(cx, cy), 2.4, 2.4);
}

/// 缩放：放大镜 + 镜内加号。镜片做大，占满内容区。
void drawZoom(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    const QRectF lens(2.8, 2.8, 13.2, 13.2);
    p.setPen(strokePen(p, 2.1));
    p.drawEllipse(lens);
    // 手柄：外粗内细的锥形观感靠两点收尾即可
    p.setPen(strokePen(p, 2.4));
    p.drawLine(QPointF(15.0, 15.0), QPointF(20.6, 20.6));
    // 镜内加号
    p.setPen(strokePen(p, 1.7));
    const QPointF c = lens.center();
    p.drawLine(QPointF(c.x(), c.y() - 3.6), QPointF(c.x(), c.y() + 3.6));
    p.drawLine(QPointF(c.x() - 3.6, c.y()), QPointF(c.x() + 3.6, c.y()));
}

/// 缩放放大：放大镜 + 居中加号。
void drawZoomIn(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    const QRectF lens(2.8, 2.8, 13.2, 13.2);
    p.setPen(strokePen(p, 2.1));
    p.drawEllipse(lens);
    p.setPen(strokePen(p, 2.4));
    p.drawLine(QPointF(15.0, 15.0), QPointF(20.6, 20.6));
    // 镜内加号（两笔交叉）
    p.setPen(strokePen(p, 1.9));
    const QPointF c = lens.center();
    p.drawLine(QPointF(c.x(), c.y() - 3.4), QPointF(c.x(), c.y() + 3.4));
    p.drawLine(QPointF(c.x() - 3.4, c.y()), QPointF(c.x() + 3.4, c.y()));
}

/// 缩放缩小：放大镜 + 居中减号。
void drawZoomOut(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    const QRectF lens(2.8, 2.8, 13.2, 13.2);
    p.setPen(strokePen(p, 2.1));
    p.drawEllipse(lens);
    p.setPen(strokePen(p, 2.4));
    p.drawLine(QPointF(15.0, 15.0), QPointF(20.6, 20.6));
    // 镜内减号（单笔横线）
    p.setPen(strokePen(p, 1.9));
    p.drawLine(QPointF(lens.center().x() - 3.4, lens.center().y()),
               QPointF(lens.center().x() + 3.4, lens.center().y()));
}

/// 适应窗口：外框 + 四角内收标记，表示"缩放到恰好填满"。
void drawFitView(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    const QRectF box(kInset, kInset + 0.6, kSpan, kSpan - 1.2);
    p.setPen(strokePen(p, 1.8));
    p.drawRoundedRect(box, 2.2, 2.2);
    // 四角标记用更细的笔，避免和外框一样粗导致糊成一团
    p.setPen(strokePen(p, 1.7));
    drawCornerMarks(p, box, 4.0, 2.6);
}

/// 打印：打印机外形（上半进纸 + 中部机身 + 下半出纸 + 右侧指示灯）。
void drawPrinter(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.8));
    // 进纸口：顶部的纸张（略微偏窄）
    p.drawRect(QRectF(7.5, 2.6, 9.0, 6.4));
    // 机身：中间的宽体
    p.drawRect(QRectF(3.6, 9.0, 16.8, 8.2));
    // 出纸口：机身下方的纸张
    p.drawRect(QRectF(7.5, 17.2, 9.0, 4.8));
    // 指示灯：机身右上角的小圆点
    p.setPen(strokePen(p, 1.5));
    p.drawEllipse(QPointF(17.4, 11.2), 1.1, 1.1);
}

/// 绣笔（单格涂色）：一个绣格 + 满格十字针迹。
void drawStitch(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.8));
    p.drawRoundedRect(QRectF(kInset, kInset, kSpan, kSpan), 2.0, 2.0);
    p.setPen(strokePen(p, 2.2));
    const qreal pad = 6.6;
    p.drawLine(QPointF(pad, pad), QPointF(kGrid - pad, kGrid - pad));
    p.drawLine(QPointF(kGrid - pad, pad), QPointF(pad, kGrid - pad));
}

/// 油漆桶（区域填充）：梯形桶身 45° 斜置——右上宽口（桶口）、左下窄底，
/// 左侧瓶把 + 桶口右侧滴落一滴漆。
void drawFill(QPainter& p)
{
    const QBrush brush = p.brush();
    p.save();
    p.translate(12.0, 12.0);
    p.scale(1.3, 1.3); // 放大：桶身绕中心撑大
    p.rotate(45.0);
    // 梯形桶身（修长）：上边（-y，屏幕右上）宽 = 桶口，下边（+y，屏幕左下）窄 = 桶底
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.9));
    QPainterPath body;
    body.moveTo(-5.0, -4.6);   // 桶口左端
    body.lineTo(5.0, -4.6);    // 桶口右端（宽）
    body.lineTo(2.4, 5.0);     // 桶底右端（窄）
    body.lineTo(-2.4, 5.0);    // 桶底左端
    body.closeSubpath();
    p.drawPath(body);
    // 左侧瓶把（把手弧线，向 -x 凸出）
    p.drawArc(QRectF(-7.2, -1.8, 4.4, 5.2), 90 * 16, 180 * 16);
    p.restore();
    // 滴液：桶口（右上端）外侧滴落
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawEllipse(QPointF(22.5, 16.6), 1.5, 1.5);
}

/// 矩形填充：虚线选区 + 中心实心块。
void drawRectFill(QPainter& p)
{
    const QBrush brush = p.brush();
    p.setBrush(Qt::NoBrush);
    QPen dash = strokePen(p, 1.8);
    dash.setStyle(Qt::DashLine);
    dash.setDashPattern({5.0, 3.4});
    p.setPen(dash);
    p.drawRoundedRect(QRectF(kInset, kInset, kSpan, kSpan), 2.0, 2.0);
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawRoundedRect(QRectF(8.6, 8.6, 6.8, 6.8), 1.2, 1.2);
}

/// 吸管（拾取绣色）：45° 斜置——右上 T 形实心把手（一横臂 + 竖臂朝上，
/// 下面不凸出）+ 左下粗描边空心玻璃管，两部分相连，尖端带实心圆（吸取色）。
void drawPicker(QPainter& p)
{
    const QBrush brush = p.brush();
    p.save();
    p.translate(17.0, 10.0);
    p.scale(1.25, 1.25); // 放大：吸管比其他图标视觉更细小，额外加大一号
    p.rotate(45.0);
    // 右上 T 形把手：竖臂沿 -y（屏幕右上），横臂沿 x（屏幕 45°）
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawRoundedRect(QRectF(-1.6, -6.2, 3.2, 4.6), 1.2, 1.2);   // 竖臂（朝上）
    p.drawRoundedRect(QRectF(-4.2, -1.8, 8.4, 3.6), 1.2, 1.2);   // 横臂
    // 左下空心玻璃管（加粗双线，从横臂底端伸出，相连）
    p.setBrush(Qt::NoBrush);
    QPen tube = strokePen(p, 1.6);
    p.setPen(tube);
    p.drawLine(QPointF(-1.9, 1.6), QPointF(-1.9, 7.4));   // 管左沿
    p.drawLine(QPointF(1.9, 1.6), QPointF(1.9, 7.4));     // 管右沿
    // 尖端：两条线收拢成一个尖（左下）
    p.drawLine(QPointF(-1.9, 7.4), QPointF(0.0, 10.2));
    p.drawLine(QPointF(1.9, 7.4), QPointF(0.0, 10.2));
    // 尖端实心圆：吸取的颜色
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawEllipse(QPointF(0.0, 11.6), 1.5, 1.5);
    p.restore();
}

/// 文字（添加文字）：大写字母 A——两道斜线 + 中间横杠。
void drawLetter(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 2.4));
    // 左斜线 / 右斜线，顶点在 (12, 3)，两脚落在底边 (5, 21) (19, 21)
    p.drawLine(QPointF(5.0, 21.0), QPointF(12.0, 3.0));
    p.drawLine(QPointF(12.0, 3.0), QPointF(19.0, 21.0));
    // 中间横杠：在 y = 14.6 处，脚展开宽度 (12±4.5)，横杠略内收
    p.drawLine(QPointF(7.9, 14.6), QPointF(16.1, 14.6));
}

/// 橡皮（擦除）：斜置圆角矩形（左下→右上方向）——右上实心、左下空心描边，
/// 左下一端画一条水平横线，代表"擦的平面"。
void drawEraser(QPainter& p)
{
    const QBrush brush = p.brush();
    p.save();
    p.translate(12.0, 12.0);
    p.scale(1.4, 1.4); // 放大
    p.rotate(-45.0);
    // 整体描边（空心矩形）
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.7));
    p.drawRoundedRect(QRectF(-6.4, -3.4, 12.8, 6.8), 1.8, 1.8);
    // 右上实心块（贴矩形右上角）
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawRoundedRect(QRectF(1.2, -2.8, 4.8, 5.6), 1.5, 1.5);
    p.restore();
    // 左下水平横线：擦的平面（桌面 / 被擦表面）
    p.setPen(strokePen(p, 1.6));
    p.drawLine(QPointF(4.0, 21.0), QPointF(10.0, 21.0));
}

/// 去杂点：绣格网格 + 被圈出的孤立格。
void drawCleanup(QPainter& p)
{
    const QBrush brush = p.brush();
    p.setBrush(Qt::NoBrush);
    QPen grid = strokePen(p, 1.5);
    grid.setStyle(Qt::DotLine);
    p.setPen(grid);
    const QRectF box(kInset, kInset, kSpan, kSpan);
    p.drawRoundedRect(box, 2.0, 2.0);
    // 三分线（虚线网格不画 4 条实线，避免噪声）
    p.setPen(strokePen(p, 1.2));
    p.drawLine(QPointF(kInset + kSpan / 3.0, kInset), QPointF(kInset + kSpan / 3.0, kInset + kSpan));
    p.drawLine(QPointF(kInset + kSpan * 2.0 / 3.0, kInset), QPointF(kInset + kSpan * 2.0 / 3.0, kInset + kSpan));
    p.drawLine(QPointF(kInset, kInset + kSpan / 3.0), QPointF(kInset + kSpan, kInset + kSpan / 3.0));
    p.drawLine(QPointF(kInset, kInset + kSpan * 2.0 / 3.0), QPointF(kInset + kSpan, kInset + kSpan * 2.0 / 3.0));
    // 孤立杂点（实心）+ 圈选环
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    const qreal c = kInset + kSpan / 3.0;
    p.drawRect(QRectF(c - 1.7, c - 1.7, 3.4, 3.4));
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.6));
    p.drawEllipse(QPointF(c, c), 4.3, 4.3);
}

/// 稀有色合并：左侧多条色带汇聚成右侧一条。
void drawMerge(QPainter& p)
{
    const QBrush brush = p.brush();
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawRoundedRect(QRectF(2.6, 5.0, 6.8, 3.4), 1.2, 1.2);
    p.drawRoundedRect(QRectF(2.6, 10.3, 3.6, 3.4), 1.2, 1.2);
    p.drawRoundedRect(QRectF(2.6, 15.6, 6.8, 3.4), 1.2, 1.2);
    // 汇聚折线 → 单条
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.7));
    const QPointF tip(18.4, 12.0);
    p.drawLine(QPointF(10.0, 6.7), QPointF(14.2, 6.7));
    p.drawLine(QPointF(14.2, 6.7), tip);
    p.drawLine(QPointF(6.8, 12.0), tip);
    p.drawLine(QPointF(10.0, 17.3), QPointF(14.2, 17.3));
    p.drawLine(QPointF(14.2, 17.3), tip);
    // 结果：右侧一条更粗的带
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawRoundedRect(QRectF(18.4, 9.6, 3.0, 4.8), 1.2, 1.2);
}

/// 撤销：向左回退的弧形箭头（先画弧，再补箭头）。
void drawUndo(QPainter& p)
{
    const QBrush brush = p.brush();
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 2.0));
    // 从右下向左上回卷的弧
    QPainterPath arc;
    arc.moveTo(20.2, 19.2);
    arc.lineTo(20.2, 12.2);
    arc.cubicTo(QPointF(20.2, 6.4), QPointF(14.0, 6.4), QPointF(8.6, 6.4));
    p.drawPath(arc);
    // 箭头：指向左端
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    QPainterPath head;
    head.moveTo(3.4, 6.4);
    head.lineTo(9.6, 2.4);
    head.lineTo(9.6, 10.4);
    head.closeSubpath();
    p.fillPath(head, brush);
}

/// 重做：撤销的镜像。
void drawRedo(QPainter& p)
{
    p.save();
    p.translate(kGrid, 0);
    p.scale(-1, 1);
    drawUndo(p);
    p.restore();
}

/// 符号视图：外框 + 居中字母 A（画布显示"色卡 + 符号"，A 代表符号字符）。
void drawSymbol(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.9));
    p.drawRoundedRect(QRectF(kInset, kInset, kSpan, kSpan), 2.0, 2.0);
    QFont f = p.font();
    f.setPixelSize(14);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QPen(p.brush().color(), 1.0));
    p.drawText(QRectF(kInset, kInset, kSpan, kSpan), Qt::AlignCenter, QStringLiteral("A"));
}

/// 色块视图：2×2 四个实心方块（仅显示色卡，色卡即色块阵列）。
void drawColorBlock(QPainter& p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(p.brush());
    const qreal gap = 1.4;
    const qreal w = (kSpan - gap) * 0.5;
    const qreal x0 = kInset;
    const qreal y0 = kInset;
    p.drawRect(QRectF(x0, y0, w, w));
    p.drawRect(QRectF(x0 + w + gap, y0, w, w));
    p.drawRect(QRectF(x0, y0 + w + gap, w, w));
    p.drawRect(QRectF(x0 + w + gap, y0 + w + gap, w, w));
}

/// 针迹预览：纯十字交叉（X），表示绣制后的针迹形态。
void drawStitchPreview(QPainter& p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(p.brush());
    QPen xpen = strokePen(p, 2.4);
    p.setPen(xpen);
    p.drawLine(QPointF(4.6, 19.4), QPointF(19.4, 4.6));
    p.drawLine(QPointF(4.6, 4.6), QPointF(19.4, 19.4));
}

/// 导入图片：圆角相框 + 地平线上的太阳与山峦。
void drawImport(QPainter& p)
{
    const QBrush brush = p.brush();
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.8));
    const QRectF frame(kInset, 3.0, kSpan, kSpan - 1.4);
    p.drawRoundedRect(frame, 2.4, 2.4);
    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    // 太阳
    p.drawEllipse(QPointF(8.0, 9.2), 2.0, 2.0);
    // 山峦（留出太阳位置的缺口）
    QPainterPath hills;
    hills.moveTo(frame.left() + 1.0, frame.bottom() - 1.0);
    hills.lineTo(10.6, 12.4);
    hills.lineTo(14.6, 17.0);
    hills.lineTo(17.6, 14.0);
    hills.lineTo(frame.right() - 1.0, frame.bottom() - 1.0);
    hills.closeSubpath();
    p.drawPath(hills);
}

/// 生成图纸：标准 cog 齿轮（梯形轮齿沿圆周排列，不用"放射线"凑数）。
void drawGenerate(QPainter& p)
{
    const QBrush brush = p.brush();
    const QPointF c(kGrid / 2.0, kGrid / 2.0);
    const int teeth = 8;
    const qreal pitch = 6.2831853 / teeth;
    const qreal rOuter = 10.0;
    const qreal rRoot = 7.7;
    // 每颗齿占 1/3 节距，剩下 2/3 是齿谷，形成清晰的梯形齿。
    const qreal toothRatio = 0.38;
    const qreal half = pitch * toothRatio / 2.0;

    QPainterPath cog;
    for (int i = 0; i < teeth; ++i) {
        const qreal a = pitch * i - 1.5707963; // 从正上方开始
        const qreal a0 = a - half;
        const qreal a1 = a + half;
        const QPointF p0(c.x() + std::cos(a0) * rRoot, c.y() + std::sin(a0) * rRoot);
        const QPointF p1(c.x() + std::cos(a0) * rOuter, c.y() + std::sin(a0) * rOuter);
        const QPointF p2(c.x() + std::cos(a1) * rOuter, c.y() + std::sin(a1) * rOuter);
        const QPointF p3(c.x() + std::cos(a1) * rRoot, c.y() + std::sin(a1) * rRoot);
        if (i == 0) {
            cog.moveTo(p0);
        } else {
            cog.lineTo(p0);
        }
        cog.lineTo(p1);
        cog.lineTo(p2);
        cog.lineTo(p3);
        // 齿谷：用根圆上的弧连接下一颗齿
        const qreal aNext = a + pitch - half;
        cog.arcTo(QRectF(c.x() - rRoot, c.y() - rRoot, rRoot * 2, rRoot * 2),
                  qRadiansToDegrees(aNext), -qRadiansToDegrees(pitch - toothRatio));
    }
    cog.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(brush);
    p.drawPath(cog);
    // 中心轴孔用背景挖空
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.7));
    p.drawEllipse(c, 3.5, 3.5);
}

/// 拼豆显示：2×2 四粒圆环，放大铺满内容区并居中（拼豆颗粒 = 带中心孔的圆环）。
void drawPerler(QPainter& p)
{
    p.setBrush(Qt::NoBrush);
    QPen ring = strokePen(p, 1.9);
    p.setPen(ring);
    const qreal center = 5.4;   // 圆心 5.4 / 18.6，左右上下对称居中
    const qreal span = 13.2;
    const qreal radius = 3.4;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 2; ++col) {
            const QPointF c(center + col * span, center + row * span);
            p.drawEllipse(c, radius, radius);
        }
    }
}

/// 新增盘：正方形盘 + 中间十字格（分成四格）+ 右上角加号。
void drawAddPlate(QPainter& p)
{
    // 整体绕盘中心 (9.5, 12) 放大 1.25：盘与加号一起撑大
    p.save();
    p.translate(9.5, 12.0);
    p.scale(1.25, 1.25);
    p.translate(-9.5, -12.0);
    const QRectF plate(3.5, 6.0, 12.0, 12.0);
    p.setBrush(Qt::NoBrush);
    p.setPen(strokePen(p, 1.6));
    p.drawRect(plate);
    const qreal cx = plate.center().x();
    const qreal cy = plate.center().y();
    p.drawLine(QPointF(plate.left(), cy), QPointF(plate.right(), cy));
    p.drawLine(QPointF(cx, plate.top()), QPointF(cx, plate.bottom()));
    const QPointF plusC(18.2, 5.6);
    QPen plusPen = strokePen(p, 2.6);
    p.setPen(plusPen);
    p.drawLine(plusC + QPointF(-2.6, 0), plusC + QPointF(2.6, 0));
    p.drawLine(plusC + QPointF(0, -2.6), plusC + QPointF(0, 2.6));
    p.restore();
}

/// 圆形填充：空心圆 + 中心点（提示"填充"）。
void drawCircleFill(QPainter& p)
{
    p.setPen(strokePen(p));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(3.6, 3.6, 16.8, 16.8));
    p.setPen(Qt::NoPen);
    p.setBrush(p.brush());
    p.drawEllipse(QPointF(12.0, 12.0), 1.7, 1.7);
}

/// 椭圆填充：空心椭圆 + 中心点。
void drawEllipseFill(QPainter& p)
{
    p.setPen(strokePen(p));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(2.8, 5.4, 18.4, 13.2));
    p.setPen(Qt::NoPen);
    p.setBrush(p.brush());
    p.drawEllipse(QPointF(12.0, 12.0), 1.6, 1.6);
}

/// 多边形填充：空心五边形 + 中心点。
void drawPolygonFill(QPainter& p)
{
    QPainterPath path;
    constexpr qreal cx = 12.0, cy = 12.0, r = 8.6;
    for (int i = 0; i < 5; ++i) {
        const qreal a = qDegreesToRadians(-90.0 + static_cast<qreal>(i) * 72.0);
        const QPointF pt(cx + r * qCos(a), cy + r * qSin(a));
        if (i == 0) {
            path.moveTo(pt);
        } else {
            path.lineTo(pt);
        }
    }
    path.closeSubpath();
    p.setPen(strokePen(p));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setPen(Qt::NoPen);
    p.setBrush(p.brush());
    p.drawEllipse(QPointF(12.0, 12.0), 1.6, 1.6);
}

/// 选区工具：虚线选框 + 四角实心控制柄。
void drawSelectRegion(QPainter& p)
{
    QPen pen = strokePen(p);
    pen.setStyle(Qt::DashLine);
    const QRectF box(3.6, 3.6, 16.8, 16.8);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(box);
    p.setPen(Qt::NoPen);
    p.setBrush(p.brush());
    constexpr qreal hs = 2.4;
    const QPointF corners[4] = {box.topLeft(), box.topRight(), box.bottomLeft(),
                                box.bottomRight()};
    for (const QPointF& c : corners) {
        p.drawRect(QRectF(c.x() - hs / 2.0, c.y() - hs / 2.0, hs, hs));
    }
}

/// 高亮区域：中心 3×3 亮网格 + 虚线外框（区域识别观察，区域内亮/区域外暗）。
void drawHighlight(QPainter& p)
{
    // 中心 3×3 网格（填充）
    p.setPen(Qt::NoPen);
    p.setBrush(p.brush());
    constexpr qreal gs = 3.6, gap = 0.9;
    const qreal gx0 = (kGrid - (gs * 3.0 + gap * 2.0)) / 2.0;
    const qreal gy0 = gx0;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            p.drawRect(QRectF(gx0 + col * (gs + gap), gy0 + row * (gs + gap), gs, gs));
        }
    }
    // 虚线外框（区域范围）
    QPen pen = strokePen(p);
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(1.6);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(2.8, 2.8, kGrid - 5.6, kGrid - 5.6), 2.0, 2.0);
}

/// 删除选区：粗叉。
void drawSelectionDelete(QPainter& p)
{
    p.setPen(strokePen(p, 2.3));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(5.6, 5.6), QPointF(18.4, 18.4));
    p.drawLine(QPointF(18.4, 5.6), QPointF(5.6, 18.4));
}

/// 镜像工具：十字虚轴（竖 + 横）+ 四向箭头（左右 / 上下镜像轴皆可点选）。
void drawMirror(QPainter& p)
{
    p.setPen(strokePen(p));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(12.0, 6.0), QPointF(12.0, 18.0));
    p.drawLine(QPointF(6.0, 12.0), QPointF(18.0, 12.0));
    // 左右箭头（底部）
    QPainterPath left;
    left.moveTo(10.2, 18.6);
    left.lineTo(4.8, 21.0);
    left.lineTo(10.2, 23.4);
    p.drawPath(left);
    QPainterPath right;
    right.moveTo(13.8, 18.6);
    right.lineTo(19.2, 21.0);
    right.lineTo(13.8, 23.4);
    p.drawPath(right);
    // 上下箭头（右侧）
    QPainterPath up;
    up.moveTo(18.6, 10.2);
    up.lineTo(21.0, 4.8);
    up.lineTo(23.4, 10.2);
    p.drawPath(up);
    QPainterPath down;
    down.moveTo(18.6, 13.8);
    down.lineTo(21.0, 19.2);
    down.lineTo(23.4, 13.8);
    p.drawPath(down);
}

/// 水平镜像：中轴虚线 + 左右箭头。
void drawMirrorH(QPainter& p)
{
    p.setPen(strokePen(p));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(12.0, 3.4), QPointF(12.0, 20.6));
    QPainterPath left;
    left.moveTo(10.2, 6.2);
    left.lineTo(4.8, 12.0);
    left.lineTo(10.2, 17.8);
    p.drawPath(left);
    QPainterPath right;
    right.moveTo(13.8, 6.2);
    right.lineTo(19.2, 12.0);
    right.lineTo(13.8, 17.8);
    p.drawPath(right);
}

/// 垂直镜像：中轴虚线 + 上下箭头。
void drawMirrorV(QPainter& p)
{
    p.setPen(strokePen(p));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(3.4, 12.0), QPointF(20.6, 12.0));
    QPainterPath up;
    up.moveTo(6.2, 10.2);
    up.lineTo(12.0, 4.8);
    up.lineTo(17.8, 10.2);
    p.drawPath(up);
    QPainterPath down;
    down.moveTo(6.2, 13.8);
    down.lineTo(12.0, 19.2);
    down.lineTo(17.8, 13.8);
    p.drawPath(down);
}

void drawKind(IconFactory::Kind kind, QPainter& p)
{
    switch (kind) {
    case IconFactory::Kind::Select:
        drawSelect(p);
        break;
    case IconFactory::Kind::Pan:
        drawPan(p);
        break;
    case IconFactory::Kind::Zoom:
        drawZoom(p);
        break;
    case IconFactory::Kind::ZoomIn:
        drawZoomIn(p);
        break;
    case IconFactory::Kind::ZoomOut:
        drawZoomOut(p);
        break;
    case IconFactory::Kind::FitView:
        drawFitView(p);
        break;
    case IconFactory::Kind::Stitch:
        drawStitch(p);
        break;
    case IconFactory::Kind::Fill:
        drawFill(p);
        break;
    case IconFactory::Kind::RectFill:
        drawRectFill(p);
        break;
    case IconFactory::Kind::Picker:
        drawPicker(p);
        break;
    case IconFactory::Kind::Eraser:
        drawEraser(p);
        break;
    case IconFactory::Kind::Cleanup:
        drawCleanup(p);
        break;
    case IconFactory::Kind::Undo:
        drawUndo(p);
        break;
    case IconFactory::Kind::Redo:
        drawRedo(p);
        break;
    case IconFactory::Kind::Merge:
        drawMerge(p);
        break;
    case IconFactory::Kind::Symbol:
        drawSymbol(p);
        break;
    case IconFactory::Kind::ColorBlock:
        drawColorBlock(p);
        break;
    case IconFactory::Kind::StitchPreview:
        drawStitchPreview(p);
        break;
    case IconFactory::Kind::Import:
        drawImport(p);
        break;
    case IconFactory::Kind::Generate:
        drawGenerate(p);
        break;
    case IconFactory::Kind::Perler:
        drawPerler(p);
        break;
    case IconFactory::Kind::AddPlate:
        drawAddPlate(p);
        break;
    case IconFactory::Kind::CircleFill:
        drawCircleFill(p);
        break;
    case IconFactory::Kind::EllipseFill:
        drawEllipseFill(p);
        break;
    case IconFactory::Kind::PolygonFill:
        drawPolygonFill(p);
        break;
    case IconFactory::Kind::SelectRegion:
        drawSelectRegion(p);
        break;
    case IconFactory::Kind::SelectionDelete:
        drawSelectionDelete(p);
        break;
    case IconFactory::Kind::Mirror:
        drawMirror(p);
        break;
    case IconFactory::Kind::MirrorH:
        drawMirrorH(p);
        break;
    case IconFactory::Kind::MirrorV:
        drawMirrorV(p);
        break;
    case IconFactory::Kind::Highlight:
        drawHighlight(p);
        break;
    case IconFactory::Kind::Text:
        drawLetter(p);
        break;
    case IconFactory::Kind::Print:
        drawPrinter(p);
        break;
    }
}

} // namespace

QIcon IconFactory::make(Kind kind, const QColor& foreground, int logicalSize)
{
    // 4 倍超采样，缩小后边缘平滑；高 DPI 下也不需要额外位图。
    constexpr int kSamples = 4;
    const int pixel = qMax(1, logicalSize) * kSamples;
    QPixmap pixmap(pixel, pixel);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(foreground);
    painter.setPen(Qt::NoPen);
    const qreal s = static_cast<qreal>(pixel) / kGrid;
    painter.scale(s, s);
    drawKind(kind, painter);
    painter.end();

    pixmap.setDevicePixelRatio(static_cast<qreal>(kSamples));
    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

const char* IconFactory::name(Kind kind)
{
    switch (kind) {
    case Kind::Select:
        return "icon.select";
    case Kind::Pan:
        return "icon.pan";
    case Kind::Zoom:
        return "icon.zoom";
    case Kind::ZoomIn:
        return "icon.zoom_in";
    case Kind::ZoomOut:
        return "icon.zoom_out";
    case Kind::FitView:
        return "icon.fit_view";
    case Kind::Stitch:
        return "icon.stitch";
    case Kind::Fill:
        return "icon.fill";
    case Kind::RectFill:
        return "icon.rect_fill";
    case Kind::Picker:
        return "icon.picker";
    case Kind::Eraser:
        return "icon.eraser";
    case Kind::Cleanup:
        return "icon.cleanup";
    case Kind::Undo:
        return "icon.undo";
    case Kind::Redo:
        return "icon.redo";
    case Kind::Merge:
        return "icon.merge";
    case Kind::Symbol:
        return "icon.symbol";
    case Kind::ColorBlock:
        return "icon.color_block";
    case Kind::StitchPreview:
        return "icon.stitch_preview";
    case Kind::Import:
        return "icon.import";
    case Kind::Generate:
        return "icon.generate";
    case Kind::Perler:
        return "icon.perler";
    case Kind::AddPlate:
        return "icon.add_plate";
    case Kind::CircleFill:
        return "icon.circle_fill";
    case Kind::EllipseFill:
        return "icon.ellipse_fill";
    case Kind::PolygonFill:
        return "icon.polygon_fill";
    case Kind::SelectRegion:
        return "icon.select_region";
    case Kind::SelectionDelete:
        return "icon.selection_delete";
    case Kind::Mirror:
        return "icon.mirror";
    case Kind::MirrorH:
        return "icon.mirror_h";
    case Kind::MirrorV:
        return "icon.mirror_v";
    case Kind::Highlight:
        return "icon.highlight";
    case Kind::Text:
        return "icon.text";
    case Kind::Print:
        return "icon.print";
    }
    return "icon.unknown";
}
