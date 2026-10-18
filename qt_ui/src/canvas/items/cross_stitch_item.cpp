#include "cross_stitch_item.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QToolTip>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyleOptionGraphicsItem>
#include <QTransform>

namespace {
/// 每格基准场景单位（mm）：14ct Aida 单格 ≈ 1.814mm，此处取整为可视密度。
constexpr qreal kBaseCellMm = 1.814;
constexpr int kBaseFabricCount = 14;
/// 空盘（尚未生成图纸）的默认图案格数，仅用于盘面尺寸占位。
constexpr int kDefaultPlateGrid = 100;
/// 盘外顶部标题栏高度（本地坐标）：盘名 | 格子数 | X，画在盘外左上角。
constexpr qreal kTitleBand = 18.0;
/// 多盘每行盘数（多了自动换行）。
constexpr int kPlatesPerRow = 5;
/// 盘间水平间距（本地坐标）。
constexpr qreal kGapX = 16.0;
/// 盘行间垂直间距（本地坐标，容纳标题栏 + 间隙）。
constexpr qreal kGapY = 24.0;
/// 盘数上限（类拓竹多平台）。
constexpr int kMaxPlateCount = 25;

/// 面料底色（Aida 本白）。
const QColor& fabricColor()
{
    static const QColor c(250, 248, 242);
    return c;
}
} // namespace

CrossStitchItem::CrossStitchItem(const QVector<ChartSnapshot>& plates, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_plates(plates)
{
    if (m_plates.isEmpty()) {
        m_plates.append(ChartSnapshot{});
    }
    m_cellSize = kBaseCellMm;
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setAcceptHoverEvents(true); // 标题栏按钮悬停 → 光标 + 气泡提示
    setZValue(0);
}

QRectF CrossStitchItem::boundingRect() const
{
    if (m_plates.isEmpty() || m_plateCount <= 0) {
        return QRectF();
    }
    // 恒为总览：所有盘多行排列（每行 5 个），包围盒取所有盘外接
    qreal right = 0.0;
    qreal bottom = 0.0;
    for (int i = 0; i < m_plateCount; ++i) {
        const QRectF pr = plateRect(i);
        right = qMax(right, pr.right());
        bottom = qMax(bottom, pr.bottom());
    }
    return QRectF(0, 0, right, bottom).adjusted(-2, -2, 2, 2);
}

void CrossStitchItem::setFabricCount(int fabricCount)
{
    const int count = qBound(6, fabricCount, 32);
    m_cellSize = kBaseCellMm * static_cast<qreal>(kBaseFabricCount) / static_cast<qreal>(count);
    update();
}

void CrossStitchItem::setDisplayMode(int mode)
{
    if (m_displayMode == mode) {
        return;
    }
    m_displayMode = mode;
    update();
}

void CrossStitchItem::setPalette(const QVector<QColor>& palette, const QVector<QChar>& symbols)
{
    if (m_activePlate >= 0 && m_activePlate < m_plates.size()) {
        m_plates[m_activePlate].palette = palette;
        m_plates[m_activePlate].symbols = symbols;
        update();
    }
}

void CrossStitchItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(widget)

    if (m_plates.isEmpty() || m_plateCount <= 0) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, false);
    // 拼豆圆环颗粒数量随可见格数增长，开抗锯齿会导致大图逐格绘制卡死，
    // 关闭抗锯齿换流畅；颗粒放大后（>=8px）锯齿并不明显。

    const qreal scale = painter->deviceTransform().m11();
    const qreal cellPx = m_cellSize * scale;

    // 只绘制暴露区域覆盖的格子（大图纸缩放下避免全量遍历）
    QRectF exposed = option ? option->exposedRect : boundingRect();
    if (!exposed.isValid() || exposed.isEmpty()) {
        exposed = boundingRect();
    }

    const QTransform viewXform = painter->worldTransform();
    const qreal marginPx = m_plateMargin * m_cellSize;
    constexpr int kMajorEvery = 10; // 粗线间隔（与设置页默认一致）

    // 恒以总览模式绘制全部盘；选中盘（currentPlate>=0）正常亮度，其余盘压暗
    for (int plate = 0; plate < m_plateCount; ++plate) {
        const ChartSnapshot& chart = plateChart(plate);
        const QRectF pr = plateRect(plate);
        const qreal ox = pr.left();
        // 图纸投影（CAD 式"纸张浮于画布"的层次感）
        painter->fillRect(pr.translated(3 / scale, 3 / scale), QColor(0, 0, 0, 70));
        painter->fillRect(pr.translated(1.5 / scale, 1.5 / scale), QColor(0, 0, 0, 70));
        // 盘底（面料米白，含四周留边格）
        painter->fillRect(pr, fabricColor());

        const int gw = platePatternCols(plate);
        const int gh = platePatternRows(plate);
        const bool hasData = chart.valid && chart.gridSize.width() > 0
                             && chart.gridSize.height() > 0
                             && chart.cells.size() == gw * gh;
        const qreal patLeft = ox + marginPx;
        const qreal patRight = patLeft + gw * m_cellSize;
        const qreal yBase = pr.top() + marginPx; // 图案区顶部（含盘名带偏移）

        // 图案区裁剪到暴露区域
        int gx0 = 0, gx1 = gw - 1, gy0 = 0, gy1 = gh - 1;
        const qreal expL = qMax(exposed.left(), pr.left());
        const qreal expR = qMin(exposed.right(), pr.right());
        const qreal expT = qMax(exposed.top(), pr.top());
        const qreal expB = qMin(exposed.bottom(), pr.bottom());
        if (expL > patLeft) {
            gx0 = static_cast<int>(qFloor((expL - patLeft) / m_cellSize));
        }
        if (expR < patRight) {
            gx1 = qMin(gx1, static_cast<int>(qCeil((expR - patLeft) / m_cellSize)) - 1);
        }
        if (expT > yBase) {
            gy0 = static_cast<int>(qFloor((expT - yBase) / m_cellSize));
        }
        if (expB < pr.bottom()) {
            gy1 = qMin(gy1, static_cast<int>(qCeil((expB - yBase) / m_cellSize)) - 1);
        }
        if (gx0 > gx1 || gy0 > gy1) {
            continue; // 该盘不在视口内
        }

        // ---- 色块 + 符号 + 拼豆圆环（该盘自己的图纸）----
        if (hasData) {
            const int paletteSize = chart.palette.size();
            int invalidColor = 0;
            // 高亮区域模式：区域外所有格子变暗（约 40% 亮度），区域内正常
            const bool hlActive = m_highlightValid && plate == m_highlightPlate;
            const auto dimColor = [](const QColor& c) {
                return QColor::fromRgbF(c.redF() * 0.4, c.greenF() * 0.4,
                                        c.blueF() * 0.4);
            };
            for (int gy = gy0; gy <= gy1; ++gy) {
                const int rowBase = gy * gw;
                for (int gx = gx0; gx <= gx1; ++gx) {
                    const CmGridCell& cell = chart.cells.at(rowBase + gx);
                    if (cell.color_index == UINT32_MAX) {
                        continue;
                    }
                    if (static_cast<int>(cell.color_index) >= paletteSize) {
                        continue;
                    }
                    const bool hlDim = hlActive && !m_highlightCells.contains(rowBase + gx);
                    const QColor color0 = chart.palette.at(static_cast<int>(cell.color_index));
                    const QColor color = hlDim ? dimColor(color0) : color0;
                    const QRectF r(patLeft + gx * m_cellSize,
                                   yBase + gy * m_cellSize,
                                   m_cellSize, m_cellSize);

                    if (m_displayMode == 2) {
                        // 十字针迹预览：底色留白 + 两条对角线
                        painter->fillRect(r, hlDim ? dimColor(fabricColor()) : fabricColor());
                        QPen stitchPen(color);
                        stitchPen.setWidthF(qMax(0.15, m_cellSize * 0.12));
                        painter->setPen(stitchPen);
                        const qreal pad = m_cellSize * 0.12;
                        painter->drawLine(r.topLeft() + QPointF(pad, pad),
                                          r.bottomRight() - QPointF(pad, pad));
                        painter->drawLine(r.topRight() + QPointF(-pad, pad),
                                          r.bottomLeft() + QPointF(pad, -pad));
                        continue;
                    }

                    if (m_displayMode == 3) {
                        // 拼豆颗粒：真圆环——只有圆环本身着色，
                        // 四角与中心孔都露出盘底（透明角）。
                        // 缩太小时退回整格色块，保证远看仍能辨认颗粒位置。
                        if (!color.isValid()) {
                            ++invalidColor;
                            continue; // 防御：调色板异常时跳过该格
                        }
                        if (cellPx >= 3.0) {
                            const QPointF c = r.center();
                            const qreal outer = m_cellSize * 0.48;
                            const qreal hole = m_cellSize * 0.22;
                            QPainterPath ring;
                            ring.setFillRule(Qt::OddEvenFill);
                            ring.addEllipse(c, outer, outer);
                            ring.addEllipse(c, hole, hole);
                            painter->setRenderHint(QPainter::Antialiasing, true);
                            painter->setPen(Qt::NoPen);
                            painter->setBrush(hlDim ? dimColor(color0) : color0);
                            painter->drawPath(ring);
                            painter->setBrush(Qt::NoBrush);
                            painter->setRenderHint(QPainter::Antialiasing, false);
                        } else {
                            painter->fillRect(r, color);
                        }
                        continue;
                    }

                    painter->fillRect(r, color);

                    // 格子文本：符号 / 色号 / 无 —— 由底部颜色栏"色号"按钮全局同步控制。
                    // 针迹预览(2)不画文本；拼豆(3)与色块视图均按 m_labelMode 显示。
                    if (m_displayMode == 2 || m_labelMode == 2) {
                        continue;
                    }
                    if (cellPx < 6.0) {
                        continue; // 格子太小，文本糊成一团，只留色块
                    }
                    QString text;
                    if (m_labelMode == 1) {
                        // 色号：DMC 色号（拼豆模式为色卡编号），随调色板顺序对应
                        const int ci = static_cast<int>(cell.color_index);
                        if (ci >= 0 && ci < chart.colorCodes.size()) {
                            text = chart.colorCodes.at(ci);
                        }
                    } else {
                        const int symIdx = static_cast<int>(cell.symbol_index);
                        if (symIdx >= 0 && symIdx < chart.symbols.size()) {
                            text = QString(chart.symbols.at(symIdx));
                        }
                    }
                    if (text.isEmpty()) {
                        continue;
                    }
                    // 在视口逻辑坐标系下绘制文本，避免 QFont 像素尺寸与视图缩放叠加导致溢出格子
                    const QRectF viewRect = viewXform.mapRect(r);
                    QFont txtFont = painter->font();
                    // 色号多位数字字号小一档，符号大一号（符号笔画少需更醒目）
                    txtFont.setPixelSize(qMax(2, static_cast<int>(
                        viewRect.height() * (m_labelMode == 1 ? 0.5 : 0.6))));
                    txtFont.setBold(true);
                    // 宽度约束：过宽文本（如 W/M/8 或多位色号）缩放至不超过格宽的 88%
                    const QFontMetrics fm(txtFont);
                    const int adv = fm.horizontalAdvance(text);
                    const int maxAdv = static_cast<int>(viewRect.width() * 0.88);
                    if (adv > maxAdv) {
                        txtFont.setPixelSize(
                            qMax(2, static_cast<int>(txtFont.pixelSize() * maxAdv
                                                    / static_cast<qreal>(adv))));
                    }
                    painter->save();
                    painter->setWorldTransform(QTransform());
                    painter->setFont(txtFont);
                    painter->setPen(color.lightness() < 128 ? Qt::white : Qt::black);
                    painter->drawText(viewRect, Qt::AlignCenter, text);
                    painter->restore();
                }
            }
        }

        // ---- 网格线（铺满整盘，含四周留边格；只画可见区，避免大图全量卡顿）----
        // 线宽随缩放增粗：放大后格线依然清晰可数格
        // 阈值 2.0：fitView 默认视图（cellPx≈3px）也要画出每格细线，
        // 否则只靠每 10 格粗线会显得“没有格子线”
        if (cellPx >= 2.0) {
            QPen thin(QColor(140, 138, 130)); // 比原 180 更深，放大后格线更清晰
            const qreal thinW = qBound(1.0, cellPx * 0.03, 2.2);
            thin.setWidthF(thinW / scale);
            thin.setCosmetic(true);
            painter->setPen(thin);
            const qreal yTop = pr.top();
            const qreal yBot = pr.bottom();
            const int gx0l = qMax(0, gx0);
            const int gx1l = qMin(gw, gx1 + 1);
            const int gy0l = qMax(0, gy0);
            const int gy1l = qMin(gh, gy1 + 1);
            for (int gx = gx0l; gx <= gx1l; ++gx) {
                const qreal x = patLeft + gx * m_cellSize;
                painter->drawLine(QPointF(x, yTop), QPointF(x, yBot));
            }
            for (int gy = gy0l; gy <= gy1l; ++gy) {
                const qreal y = yBase + gy * m_cellSize;
                painter->drawLine(QPointF(pr.left(), y), QPointF(pr.right(), y));
            }
        }
        if (cellPx >= 1.2) {
            // 每 10 格加粗（绣制时的定位基准线），按该盘自己的图案坐标对齐
            QPen thick(QColor(55, 55, 55));
            const qreal thickW = qBound(1.6, cellPx * 0.08, 4.0);
            thick.setWidthF(thickW / scale);
            thick.setCosmetic(true);
            painter->setPen(thick);
            const int gx0l = qMax(0, gx0);
            const int gx1l = qMin(gw, gx1 + 1);
            const int gy0l = qMax(0, gy0);
            const int gy1l = qMin(gh, gy1 + 1);
            for (int gcol = gx0l; gcol <= gx1l; ++gcol) {
                if (gcol % kMajorEvery != 0) {
                    continue;
                }
                const qreal x = patLeft + gcol * m_cellSize;
                painter->drawLine(QPointF(x, pr.top()), QPointF(x, pr.bottom()));
            }
            for (int gy = gy0l; gy <= gy1l; ++gy) {
                if (gy % kMajorEvery != 0) {
                    continue;
                }
                const qreal y = pr.top() + marginPx + gy * m_cellSize;
                painter->drawLine(QPointF(pr.left(), y), QPointF(pr.right(), y));
            }
        }

        // ---- 坐标刻度：底部列号（左→右 0 起）+ 左侧行号（下→上 0 起），左下角 = (0,0) ----
        // 画在图案区外侧留边带内（留边 >= 1 格才有空间）
        if (m_plateMargin >= 1) {
            const qreal patBottom = yBase + gh * m_cellSize; // 图案区底边
            const bool bottomSpace = (pr.bottom() - patBottom) >= m_cellSize;
            const bool leftSpace = (patLeft - pr.left()) >= m_cellSize;
            if (bottomSpace || leftSpace) {
                // 步长按格子屏幕像素自适应：
                // cellPx < 10：每 10 格（对齐粗线）；10~16：每 5 格；> 16：每格
                const int step = cellPx < 10.0 ? 10 : (cellPx <= 16.0 ? 5 : 1);
                // 数字用固定屏幕像素字号（不随视图缩放），保证任何缩放下可读
                const qreal pxFont = qBound(9.0, cellPx * 0.65, 18.0);
                QFont rulerFont = painter->font();
                rulerFont.setPixelSize(static_cast<int>(pxFont));
                rulerFont.setBold(false);
                painter->save();
                painter->setWorldTransform(QTransform()); // 之后都在 viewport 像素坐标绘制
                painter->setFont(rulerFont);
                painter->setPen(QPen(QColor(100, 98, 92, 235), 0));
                if (bottomSpace) {
                    // 列号显示 1..gw（左下角 = 1），步长对齐 v % step == 1
                    const int gxStart = ((gx0 + step - 1) / step) * step;
                    for (int gx = gxStart; gx <= gx1; gx += step) {
                        const int v = gx + 1;
                        if (step > 1 && v % step != 1) {
                            continue;
                        }
                        const QRectF cellRect(patLeft + gx * m_cellSize, patBottom,
                                              m_cellSize, m_cellSize);
                        painter->drawText(viewXform.mapRect(cellRect),
                                          Qt::AlignCenter, QString::number(v));
                    }
                }
                if (leftSpace) {
                    // 行号紧贴图案区左边一格；显示 1..gh，底部 = 1 向上递增，步长对齐 v % step == 1
                    for (int gy = gy0; gy <= gy1; ++gy) {
                        const int v = gh - gy;
                        if (step > 1 && v % step != 1) {
                            continue;
                        }
                        const QRectF cellRect(patLeft - m_cellSize,
                                              yBase + gy * m_cellSize,
                                              m_cellSize, m_cellSize);
                        painter->drawText(viewXform.mapRect(cellRect),
                                          Qt::AlignCenter, QString::number(v));
                    }
                }
                painter->restore();
            }
        }

        // ---- 盘外框（每个盘都独立描边）----
        // drawRect 会用「当前画笔」填充矩形：先显式置 NoBrush，确保只描边、
        // 不会把前面绘制的色块 / 格子线盖掉（拼豆孔绘制会留下画笔状态）。
        painter->setBrush(Qt::NoBrush);
        QPen border(QColor(40, 40, 40));
        border.setWidthF(1.5 / scale);
        border.setCosmetic(true);
        painter->setPen(border);
        painter->drawRect(pr);

        // ---- 选中盘高亮：未选中的盘整体压暗 ----
        if (m_currentPlate >= 0 && plate != m_currentPlate) {
            painter->fillRect(pr, QColor(0, 0, 0, 85));
        }

        // ---- 标题栏（盘外上方）：盘名 | 格子数 | 锁定 | 适应窗口 | X 删除 ----
        {
            const QRectF bar = titleBarRect(plate);
            const bool locked = plateLocked(plate);
            painter->save();
            // 锁定盘的标题栏染淡橙底，一眼区分"这块盘不许动"
            painter->setPen(QPen(QColor(150, 148, 140), 0.4));
            painter->setBrush(locked ? QColor(255, 233, 205, 235) : QColor(255, 255, 255, 235));
            painter->drawRoundedRect(bar, 3.0, 3.0);
            painter->setPen(QColor(40, 40, 40));
            painter->drawText(
                QRectF(bar.left() + bar.width() * 0.03, bar.top(),
                       bar.width() * 0.36, bar.height()),
                Qt::AlignLeft | Qt::AlignVCenter, plateName(plate));
            painter->drawText(titleGridRect(plate), Qt::AlignRight | Qt::AlignVCenter,
                              QStringLiteral("%1\u00d7%2")
                                  .arg(platePatternCols(plate))
                                  .arg(platePatternRows(plate)));
            // 适应窗口按钮（X 左侧）：细框 + 四角向外箭头
            const QRectF fr = titleFitRect(plate);
            painter->setPen(QPen(QColor(110, 108, 102), 0.4));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(fr);
            const qreal pad = fr.width() * 0.22;
            const QRectF inner = fr.adjusted(pad, pad, -pad, -pad);
            const qreal al = fr.width() * 0.18;
            painter->setPen(QPen(QColor(90, 90, 90), 0.5));
            painter->drawRect(inner);
            painter->drawLine(fr.topLeft(), fr.topLeft() + QPointF(al, 0));
            painter->drawLine(fr.topLeft(), fr.topLeft() + QPointF(0, al));
            painter->drawLine(fr.topRight(), fr.topRight() + QPointF(-al, 0));
            painter->drawLine(fr.topRight(), fr.topRight() + QPointF(0, al));
            painter->drawLine(fr.bottomLeft(), fr.bottomLeft() + QPointF(al, 0));
            painter->drawLine(fr.bottomLeft(), fr.bottomLeft() + QPointF(0, -al));
            painter->drawLine(fr.bottomRight(), fr.bottomRight() + QPointF(-al, 0));
            painter->drawLine(fr.bottomRight(), fr.bottomRight() + QPointF(0, -al));
            // 锁定按钮（适应窗口左侧）：挂锁图标 —— 圆角锁体 + 锁眼 + U 形锁梁；
            // 锁定 = 锁梁竖直扣合（橙），解锁 = 锁梁绕铰点向一侧翘起（灰）
            const QRectF lr = titleLockRect(plate);
            const bool hoverLock = (m_hoverPlate == plate && m_hoverButton == 0);
            // 平时不画按钮框；悬停只给一层很淡的灰底提示（避免黑底）
            if (hoverLock) {
                painter->setPen(QPen(QColor(150, 147, 139, 140), 0.4));
                painter->setBrush(QColor(118, 115, 108, 26));
                painter->drawRoundedRect(lr, 2.0, 2.0);
            }
            const QColor lockColor = locked ? QColor(206, 112, 28)
                                            : (hoverLock ? QColor(96, 94, 88)
                                                         : QColor(130, 128, 122));
            painter->setPen(QPen(lockColor, 0.5));
            painter->setBrush(Qt::NoBrush); // 图标本身只描边

            const qreal h = lr.height();
            const qreal cx = lr.center().x();
            // 收紧比例，保证含翘起的整把锁都落在按钮框内：
            // 锁梁半径 0.16h，锁体高 0.54h，上、下各留 0.07h 边距
            const qreal shR = h * 0.16;                    // 锁梁（U 形）半径
            const qreal bodyW = h * 0.52;                  // 锁体宽
            const qreal bodyH = h * 0.54;                  // 锁体高
            const QRectF body(cx - bodyW / 2.0,
                              lr.top() + h * 0.07 + 2.0 * shR,
                              bodyW, bodyH);
            painter->drawRoundedRect(body, bodyW * 0.16, bodyW * 0.16);

            // 锁梁：上半圆 + 两侧竖腿；解锁时整体绕右腿底部铰点向外翻起
            QPainterPath shackle;
            shackle.moveTo(0.0, 0.0);              // 右腿脚（铰点）
            shackle.lineTo(0.0, -shR);
            shackle.arcTo(QRectF(-2 * shR, -2 * shR, 2 * shR, 2 * shR), 0, 180);
            shackle.lineTo(-2 * shR, 0.0);         // 左腿
            painter->save();
            painter->translate(cx + shR, body.top());
            if (!locked) {
                painter->rotate(30.0); // 解锁：锁梁翘起，一眼看出"没锁"
            }
            painter->drawPath(shackle);
            painter->restore();

            // 锁眼：圆点 + 短杆（缩得太小时省略，避免糊成一团）
            if (bodyH * scale >= 9.0) {
                const QPointF keyTop(cx, body.top() + bodyH * 0.30);
                painter->setBrush(lockColor);
                painter->setPen(Qt::NoPen);
                painter->drawEllipse(keyTop, bodyW * 0.12, bodyW * 0.12);
                painter->setPen(QPen(lockColor, bodyW * 0.13));
                painter->drawLine(keyTop,
                                  keyTop + QPointF(0.0, bodyH * 0.36));
            }
            const QRectF xr = titleXRect(plate);
            painter->setPen(QPen(QColor(130, 128, 122), 0.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(xr);
            painter->drawLine(xr.topLeft() + QPointF(3, 3),
                              xr.bottomRight() - QPointF(3, 3));
            painter->drawLine(xr.topRight() + QPointF(-3, 3),
                              xr.bottomLeft() + QPointF(3, -3));
            painter->restore();
        }
    }
}

int CrossStitchItem::plateAt(const QPointF& scenePos) const
{
    const QPointF local = mapFromScene(scenePos);
    for (int i = 0; i < m_plateCount; ++i) {
        if (plateRect(i).contains(local)) {
            return i;
        }
    }
    return -1;
}

void CrossStitchItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    // 双击标题栏盘名区 → 改名（兼容场景级双击；工具层下单击改名见 handlePress）
    const QPointF local = mapFromScene(event->scenePos());
    for (int plate = 0; plate < m_plateCount; ++plate) {
        if (titleNameRect(plate).contains(local)) {
            renamePlate(plate);
            event->accept();
            return;
        }
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void CrossStitchItem::renamePlate(int plate)
{
    if (plate < 0 || plate >= m_plateCount || plateLocked(plate)) {
        return; // 锁定盘禁止改名（与 handlePress 一致的口径）
    }
    const QString oldName = plateName(plate);
    bool ok = false;
    const QString newName = QInputDialog::getText(
        nullptr, tr("Rename plate"), tr("Plate name:"),
        QLineEdit::Normal, oldName, &ok);
    if (ok && !newName.trimmed().isEmpty() && newName != oldName) {
        if (m_plateNames.size() < m_plateCount) {
            m_plateNames.resize(m_plateCount);
        }
        m_plateNames[plate] = newName.trimmed();
        emit plateRenamed(plate, m_plateNames.at(plate));
        update();
    }
}

void CrossStitchItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    updateButtonHover(mapFromScene(event->scenePos()),
                      event->screenPos(), event->widget());
}

void CrossStitchItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    const QPointF local = mapFromScene(event->scenePos());
    updateButtonHover(local, event->screenPos(), event->widget());
    // 仍悬停在锁定按钮上但气泡已消失（如点击后自动隐藏）时补弹一次
    if (m_hoverPlate >= 0 && m_hoverButton == 0 && !QToolTip::isVisible()) {
        QToolTip::showText(event->screenPos(),
                           plateLocked(m_hoverPlate) ? tr("Unlock this plate")
                                                     : tr("Lock this plate"),
                           event->widget());
    }
}

void CrossStitchItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    m_hoverPlate = -1;
    m_hoverButton = -1;
    unsetCursor();
    update();
}

int CrossStitchItem::titleButtonAt(int plate, const QPointF& local) const
{
    if (titleLockRect(plate).contains(local)) {
        return 0;
    }
    if (titleFitRect(plate).contains(local)) {
        return 1;
    }
    if (titleXRect(plate).contains(local)) {
        return 2;
    }
    return -1;
}

void CrossStitchItem::updateButtonHover(const QPointF& local,
                                        const QPoint& screenPos, QWidget* widget)
{
    int plate = -1;
    for (int i = 0; i < m_plateCount; ++i) {
        if (titleBarRect(i).contains(local)) {
            plate = i;
            break;
        }
    }
    int button = plate >= 0 ? titleButtonAt(plate, local) : -1;
    // 盘名区不是按钮，但单击可改名：同样给小手光标 + 提示
    if (button < 0 && plate >= 0 && titleNameRect(plate).contains(local)) {
        button = 3;
    }
    if (plate == m_hoverPlate && button == m_hoverButton) {
        return;
    }
    m_hoverPlate = plate;
    m_hoverButton = button;
    if (button >= 0) {
        // 悬停按钮：小手光标 + 气泡提示（提示随锁定状态变化）
        setCursor(Qt::PointingHandCursor);
        QString tip;
        switch (button) {
        case 0:
            tip = plateLocked(plate) ? tr("Unlock this plate") : tr("Lock this plate");
            break;
        case 1:
            tip = tr("Fit this plate to view");
            break;
        case 2:
            tip = tr("Remove this plate");
            break;
        case 3:
            tip = plateLocked(plate) ? tr("Unlock this plate to rename")
                                     : tr("Click to rename this plate");
            break;
        }
        QToolTip::showText(screenPos, tip, widget);
        update(); // 锁定按钮悬停时压暗命中框（paint 里用 m_hoverButton）
    } else {
        unsetCursor();
        if (QToolTip::isVisible()) {
            QToolTip::hideText();
        }
    }
}

bool CrossStitchItem::handlePress(const QPointF& scenePos)
{
    const QPointF local = mapFromScene(scenePos);
    for (int plate = 0; plate < m_plateCount; ++plate) {
        const QRectF bar = titleBarRect(plate);
        if (!bar.contains(local)) {
            continue;
        }
        // 锁定按钮：切换该盘锁定状态（锁定 = 不能编辑、生成也不落此盘）
        if (titleLockRect(plate).contains(local)) {
            emit plateLockToggled(plate);
            return true;
        }
        // 适应窗口：把该盘缩放到窗口
        if (titleFitRect(plate).contains(local)) {
            emit plateFitRequested(plate);
            return true;
        }
        // X：删除该盘
        if (titleXRect(plate).contains(local)) {
            emit plateRemoveRequested(plate);
            return true;
        }
        // 格子数：画布裁剪/扩展（图案内容不变；选哪边就在哪边增减格子，每 10 格一步）
        // 锁定盘跳过对话框：格局变化也是改动，锁定即冻结
        if (titleGridRect(plate).contains(local) && !plateLocked(plate)) {
            QDialog dlg(nullptr);
            dlg.setWindowTitle(tr("Plate grid size"));
            auto* form = new QFormLayout(&dlg);

            auto* wSpin = new QSpinBox(&dlg);
            wSpin->setRange(1, 9999);
            wSpin->setSingleStep(10);
            wSpin->setValue(platePatternCols(plate));
            auto* wLeft = new QRadioButton(tr("Left"), &dlg);
            auto* wRight = new QRadioButton(tr("Right"), &dlg);
            wRight->setChecked(true); // 默认右边增减
            auto* wRow = new QWidget(&dlg);
            auto* wLay = new QHBoxLayout(wRow);
            wLay->setContentsMargins(0, 0, 0, 0);
            wLay->addWidget(wSpin, 1);
            wLay->addWidget(new QLabel(tr("Adjust:"), &dlg));
            wLay->addWidget(wLeft);
            wLay->addWidget(wRight);
            form->addRow(tr("Width:"), wRow);

            auto* hSpin = new QSpinBox(&dlg);
            hSpin->setRange(1, 9999);
            hSpin->setSingleStep(10);
            hSpin->setValue(platePatternRows(plate));
            auto* hUp = new QRadioButton(tr("Up"), &dlg);
            auto* hDown = new QRadioButton(tr("Down"), &dlg);
            hDown->setChecked(true); // 默认下边增减
            auto* hRow = new QWidget(&dlg);
            auto* hLay = new QHBoxLayout(hRow);
            hLay->setContentsMargins(0, 0, 0, 0);
            hLay->addWidget(hSpin, 1);
            hLay->addWidget(new QLabel(tr("Adjust:"), &dlg));
            hLay->addWidget(hUp);
            hLay->addWidget(hDown);
            form->addRow(tr("Height:"), hRow);

            auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                             Qt::Horizontal, &dlg);
            form->addRow(box);
            QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            if (dlg.exec() == QDialog::Accepted) {
                emit plateGridResized(plate, wSpin->value(), hSpin->value(),
                                      wRight->isChecked() ? 1 : 0, hDown->isChecked() ? 1 : 0);
            }
            return true;
        }
        // 盘名：单击直接改名（工具层拦截了双击事件，改名走单击最可靠）；
        // 锁定盘禁止改名（锁定 = 冻结盘上一切）
        if (titleNameRect(plate).contains(local)) {
            if (plateLocked(plate)) {
                QToolTip::showText(QCursor::pos(),
                                   tr("Unlock this plate to rename"),
                                   nullptr);
                return true;
            }
            renamePlate(plate);
            return true;
        }
        // 标题栏其余区域：吞掉，避免误触盘身
        return true;
    }
    return false;
}

QPoint CrossStitchItem::cellAt(const QPointF& scenePos) const
{
    const QPointF local = mapFromScene(scenePos);
    int plate = -1, px = -1, py = -1;
    if (!localToPattern(local, &plate, &px, &py)) {
        return QPoint(-1, -1);
    }
    // 仅激活盘可编辑：点击其他盘返回无效（避免编辑错盘）
    if (plate != m_activePlate) {
        return QPoint(-1, -1);
    }
    return QPoint(px, py);
}

QPointF CrossStitchItem::cellCenter(int x, int y) const
{
    const QRectF pr = plateRect(m_activePlate);
    const qreal lx = pr.left() + (m_plateMargin + x + 0.5) * m_cellSize;
    const qreal ly = pr.top() + (m_plateMargin + y + 0.5) * m_cellSize;
    return mapToScene(QPointF(lx, ly));
}

QPointF CrossStitchItem::cellRectTopLeft(int x, int y) const
{
    const QRectF pr = plateRect(m_activePlate);
    const qreal lx = pr.left() + (m_plateMargin + x) * m_cellSize;
    const qreal ly = pr.top() + (m_plateMargin + y) * m_cellSize;
    return mapToScene(QPointF(lx, ly));
}

QPointF CrossStitchItem::cellRectBottomRight(int x, int y) const
{
    const QRectF pr = plateRect(m_activePlate);
    const qreal lx = pr.left() + (m_plateMargin + x + 1.0) * m_cellSize;
    const qreal ly = pr.top() + (m_plateMargin + y + 1.0) * m_cellSize;
    return mapToScene(QPointF(lx, ly));
}

// ---- 多盘布局（一盘一图） ----

const ChartSnapshot& CrossStitchItem::plateChart(int plate) const
{
    static const ChartSnapshot kEmpty;
    if (plate < 0 || plate >= m_plates.size()) {
        return kEmpty;
    }
    return m_plates.at(plate);
}

int CrossStitchItem::platePatternCols(int plate) const
{
    const int gw = plateChart(plate).gridSize.width();
    return gw > 0 ? gw : kDefaultPlateGrid;
}

int CrossStitchItem::platePatternRows(int plate) const
{
    const int gh = plateChart(plate).gridSize.height();
    return gh > 0 ? gh : kDefaultPlateGrid;
}

QVector<CrossStitchItem::ColorUsage> CrossStitchItem::pageColorUsage(int plate,
                                                                    int x0, int y0,
                                                                    int w, int h) const
{
    QVector<ColorUsage> out;
    const ChartSnapshot& snap = plateChart(plate);
    if (snap.cells.isEmpty() || w <= 0 || h <= 0) {
        return out;
    }
    // 按色号统计该页切片内的针数
    QVector<int> count;
    for (const CmGridCell& cell : snap.cells) {
        if (cell.color_index == UINT32_MAX) {
            continue; // 空格
        }
        const int cx = static_cast<int>(cell.x);
        const int cy = static_cast<int>(cell.y);
        if (cx < x0 || cx >= x0 + w || cy < y0 || cy >= y0 + h) {
            continue;
        }
        const int ci = static_cast<int>(cell.color_index);
        if (ci >= count.size()) {
            count.resize(ci + 1, 0);
        }
        count[ci]++;
    }
    for (int ci = 0; ci < count.size(); ++ci) {
        if (count.at(ci) <= 0) {
            continue;
        }
        ColorUsage u;
        u.color = ci < snap.palette.size() ? snap.palette.at(ci) : QColor(Qt::black);
        u.code = ci < snap.colorCodes.size() ? snap.colorCodes.at(ci) : QString();
        u.symbol = ci < snap.symbols.size() ? snap.symbols.at(ci) : QChar('?');
        u.count = count.at(ci);
        out.append(u);
    }
    return out;
}

qreal CrossStitchItem::plateWidth(int plate) const
{
    return (platePatternCols(plate) + m_plateMargin * 2) * m_cellSize;
}

qreal CrossStitchItem::plateHeight(int plate) const
{
    return (platePatternRows(plate) + m_plateMargin * 2) * m_cellSize;
}

QRectF CrossStitchItem::plateRect(int plate) const
{
    const qreal w = plateWidth(plate);
    const qreal h = plateHeight(plate);
    // 多行总览：每行最多 kPlatesPerRow 个盘；行内 x 按各盘宽累计，行间 y 按该行最大盘高累计
    const int row = plate / kPlatesPerRow;
    const int col = plate % kPlatesPerRow;
    qreal x = 0.0;
    for (int i = plate - col; i < plate; ++i) {
        x += plateWidth(i);
    }
    x += col * kGapX;
    qreal y = kTitleBand;
    for (int r = 0; r < row; ++r) {
        qreal rowMax = 0.0;
        for (int i = r * kPlatesPerRow; i < (r + 1) * kPlatesPerRow && i < m_plateCount; ++i) {
            rowMax = qMax(rowMax, plateHeight(i));
        }
        y += rowMax + kGapY;
    }
    return QRectF(x, y, w, h);
}

QRectF CrossStitchItem::patternRectScene(int plate) const
{
    // 纯图案区 = plateRect 内缩四周盘边距（盘边距不参与打印切片，
    // 相邻页可直接拼接成完整网格）
    const qreal m = m_plateMargin * m_cellSize;
    const QRectF pr = plateRect(plate).adjusted(m, m, -m, -m);
    return mapRectToScene(pr);
}

QRectF CrossStitchItem::plateRectScene(int plate) const
{
    return mapToScene(plateRect(plate)).boundingRect();
}

QRectF CrossStitchItem::plateRectWithTitleScene(int plate) const
{
    return mapToScene(plateRect(plate).adjusted(0, -kTitleBand - 4.0, 0, 0)).boundingRect();
}

QRectF CrossStitchItem::titleBarRect(int plate) const
{
    const QRectF pr = plateRect(plate);
    return QRectF(pr.left(), pr.top() - kTitleBand, pr.width(), kTitleBand);
}

QRectF CrossStitchItem::titleXRect(int plate) const
{
    const QRectF bar = titleBarRect(plate);
    const qreal s = bar.height() * 0.72; // X 方框边长
    return QRectF(bar.right() - s - 3.0, bar.top() + (bar.height() - s) * 0.5, s, s);
}

QRectF CrossStitchItem::titleFitRect(int plate) const
{
    const QRectF bar = titleBarRect(plate);
    const qreal s = bar.height() * 0.72;
    const QRectF xr = titleXRect(plate);
    return QRectF(xr.left() - s - 3.0, bar.top() + (bar.height() - s) * 0.5, s, s);
}

QRectF CrossStitchItem::titleLockRect(int plate) const
{
    const QRectF bar = titleBarRect(plate);
    const qreal s = bar.height() * 0.72;
    const QRectF fitR = titleFitRect(plate);
    return QRectF(fitR.left() - s - 3.0, bar.top() + (bar.height() - s) * 0.5, s, s);
}

QRectF CrossStitchItem::titleGridRect(int plate) const
{
    const QRectF bar = titleBarRect(plate);
    const QRectF lockR = titleLockRect(plate);
    const qreal left = bar.left() + bar.width() * 0.40;
    return QRectF(left, bar.top(), lockR.left() - left - 4.0, bar.height());
}

QRectF CrossStitchItem::titleNameRect(int plate) const
{
    const QRectF bar = titleBarRect(plate);
    return QRectF(bar.left(), bar.top(), bar.width() * 0.40, bar.height());
}

QString CrossStitchItem::plateName(int plate) const
{
    if (plate >= 0 && plate < m_plateNames.size()
        && !m_plateNames.at(plate).trimmed().isEmpty()) {
        return m_plateNames.at(plate);
    }
    return tr("Plate %1").arg(plate + 1);
}

bool CrossStitchItem::localToPattern(const QPointF& local, int* plate, int* px, int* py) const
{
    const int count = qBound(1, m_plateCount, kMaxPlateCount);
    int p = -1;
    for (int i = 0; i < count; ++i) {
        const QRectF pr = plateRect(i);
        if (local.x() >= pr.left() && local.x() < pr.right()) {
            p = i;
            break;
        }
    }
    if (p < 0) {
        return false;
    }
    const QRectF pr = plateRect(p);
    const int ix = static_cast<int>(qFloor((local.x() - pr.left()) / m_cellSize)) - m_plateMargin;
    const int iy = static_cast<int>(qFloor((local.y() - pr.top()) / m_cellSize)) - m_plateMargin;
    if (ix < 0 || iy < 0 || ix >= platePatternCols(p) || iy >= platePatternRows(p)) {
        return false;
    }
    if (plate) {
        *plate = p;
    }
    if (px) {
        *px = ix;
    }
    if (py) {
        *py = iy;
    }
    return true;
}

void CrossStitchItem::setPlateLayout(int count, int marginCells)
{
    const int c = qBound(1, count, kMaxPlateCount);
    const int m = qBound(0, marginCells, 50);
    m_plateCount = c;
    m_plateMargin = m;
    // 补齐 / 裁剪每盘快照（补出的盘为空盘）
    m_plates.resize(c);
    // 锁定标记对齐盘数（resize 保前缀，已锁状态不丢）
    if (m_plateLocks.size() < c) {
        m_plateLocks.resize(c);
    }
    if (m_currentPlate >= c) {
        m_currentPlate = -1;
    }
    if (m_activePlate >= c) {
        m_activePlate = 0;
    }
    prepareGeometryChange();
    update();
}

void CrossStitchItem::setPlates(const QVector<ChartSnapshot>& plates)
{
    m_plates = plates;
    // 数量与盘数对齐：不足补空盘，多余丢弃
    if (m_plates.size() < m_plateCount) {
        m_plates.resize(m_plateCount);
    } else if (m_plates.size() > m_plateCount) {
        m_plates.resize(m_plateCount);
    }
    if (m_plates.isEmpty()) {
        m_plates.append(ChartSnapshot{});
    }
    if (m_activePlate >= m_plateCount) {
        m_activePlate = 0;
    }
    if (m_currentPlate >= m_plateCount) {
        m_currentPlate = -1;
    }
    prepareGeometryChange();
    update();
}

void CrossStitchItem::setPlateNames(const QStringList& names)
{
    m_plateNames = names;
    update();
}

void CrossStitchItem::setPlateLocks(const QVector<bool>& locks)
{
    m_plateLocks = locks;
    update();
}

void CrossStitchItem::setPlateLocked(int plate, bool locked)
{
    if (plate < 0) {
        return;
    }
    if (m_plateLocks.size() <= plate) {
        m_plateLocks.resize(plate + 1);
    }
    m_plateLocks[plate] = locked;
    update();
}

void CrossStitchItem::removePlate(int plate)
{
    if (plate < 0 || plate >= m_plateCount || m_plateCount <= 1) {
        return;
    }
    m_plateCount -= 1;
    if (plate < m_plates.size()) {
        m_plates.removeAt(plate);
    }
    while (m_plates.size() < m_plateCount) {
        m_plates.append(ChartSnapshot{});
    }
    // 锁定标记同步前移
    if (plate < m_plateLocks.size()) {
        m_plateLocks.removeAt(plate);
    }
    while (m_plateLocks.size() < m_plateCount) {
        m_plateLocks.append(false);
    }
    if (m_plateNames.size() > m_plateCount) {
        m_plateNames.resize(m_plateCount);
    }
    if (m_activePlate >= m_plateCount) {
        m_activePlate = m_plateCount - 1;
    }
    if (m_currentPlate >= m_plateCount) {
        m_currentPlate = -1;
    }
    prepareGeometryChange();
    update();
}

void CrossStitchItem::updatePlate(int plate, const ChartSnapshot& chart)
{
    if (plate < 0 || plate >= m_plates.size()) {
        return;
    }
    m_plates[plate] = chart;
    prepareGeometryChange();
    update();
}

void CrossStitchItem::setCurrentPlate(int index)
{
    if (index == m_currentPlate) {
        return;
    }
    m_currentPlate = (index < -1 || index >= m_plateCount) ? -1 : index;
    prepareGeometryChange();
    update();
}

void CrossStitchItem::setHighlight(int plate, const QSet<int>& cells)
{
    m_highlightPlate = plate;
    m_highlightCells = cells;
    m_highlightValid = (plate >= 0 && plate < m_plateCount);
    update();
}

void CrossStitchItem::setActivePlate(int plate)
{
    const int p = (plate < 0 || plate >= m_plateCount) ? 0 : plate;
    if (p == m_activePlate) {
        return;
    }
    m_activePlate = p;
    update();
}

void CrossStitchItem::setCells(const QVector<CmGridCell>& cells)
{
    if (m_activePlate >= 0 && m_activePlate < m_plates.size()) {
        m_plates[m_activePlate].cells = cells;
        update();
    }
}
