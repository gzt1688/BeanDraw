#pragma once

#include "canvas/tools/tool_base.h"

#include <QLineF>
#include <QPointF>
#include <QRect>

class QGraphicsLineItem;
class QGraphicsRectItem;

/// 镜像工具（由参数面板驱动，镜像作用于当前选区）：
///
/// - TwoPoints 模式（自定义镜像）：单击第一点 → 移动显示预览线 → 单击第二点，
///   以两点连线为轴镜像选区（任意角度，最近邻映射）；
/// - XyAxis 模式（自定义轴）：鼠标悬停时最接近的贯穿格线（竖线 = 左右轴 /
///   横线 = 上下轴）高亮，贯穿整个画布、很细；单击以该线为轴镜像选区；
/// - 未框选选区时点轴/两点不生效，由 MainWindow 提示先框选；
/// - Esc = 取消第一点 / 清除高亮。
///
/// 轴坐标以格子单位表示（可半格：竖格线 x=k 即 (k,0)-(k,h)）。
class MirrorTool : public ToolBase
{
    Q_OBJECT

public:
    enum Mode {
        TwoPoints = 0, ///< 自定义镜像（两点连线轴）
        XyAxis = 1     ///< 自定义轴（XY 贯穿格线，单击选轴）
    };

    explicit MirrorTool(QObject* parent = nullptr);
    ~MirrorTool() override;

    QString toolId() const override { return QStringLiteral("tool.mirror"); }
    QCursor cursor() const override;

    /// 切换镜像模式（切换时清空交互状态）。
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    /// 设置当前选区（格子坐标，闭区间；仅只读显示范围，不修改内容）。
    /// 传入空矩形清除显示。
    void setSelection(const QRect& cells);

    void activate(QGraphicsView* view, CraftScene* scene) override;
    void deactivate() override;

    /// 清除交互状态（切换工具 / Esc）。
    void clearState();

    /// 键盘锁定轴方向：AxisNone = XY 自动识别；AxisHorizontal = 只识别 X（横线）；
    /// AxisVertical = 只识别 Y（竖线）。
    void setAxisLock(int kind);
    int axisLock() const { return m_axisLock; }

signals:
    /// 请求以 (ax0,ay0)-(ax1,ay1) 为轴镜像当前选区（格子单位，可半格）。
    void mirrorAxisRequested(double ax0, double ay0, double ax1, double ay1);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void keyPress(QKeyEvent* event, QGraphicsView* view) override;

private:
    enum AxisHitKind {
        AxisNone = 0,
        AxisVertical,  ///< 竖贯穿线（左右镜像）
        AxisHorizontal ///< 横贯穿线（上下镜像）
    };

    struct AxisHit {
        AxisHitKind kind = AxisNone;
        int k = 0;   ///< 格线序号（格子坐标，竖线 x=k / 横线 y=k）
        QLineF line; ///< 高亮线场景坐标（贯穿整个激活盘）
    };

    AxisHit detectAxis(const QPointF& scenePos);
    QPointF gridPos(const QPointF& scenePos) const;
    void updateAxisItem(const AxisHit& hit);
    void updateSelectionOverlay();
    void updatePreview(const QPointF& scenePos);
    void clearStateInternal();

    Mode m_mode = XyAxis;
    int m_axisLock = AxisNone; ///< 键盘锁定：AxisNone=XY 自动 / X 只横 / Y 只竖
    QGraphicsLineItem* m_axisItem = nullptr;    ///< 高亮贯穿线 / 预览线共用（两点模式复用）
    QGraphicsRectItem* m_pointMark = nullptr;   ///< 第一点十字标记
    QGraphicsLineItem* m_previewLine = nullptr; ///< 两点预览线
    QGraphicsRectItem* m_selOverlay = nullptr;  ///< 当前选区只读蒙层

    bool m_hasFirst = false;
    QPointF m_firstGrid; ///< 第一点（格子坐标 float）
    AxisHit m_hit;       ///< 当前高亮贯穿线
    QRect m_selection;   ///< 当前选区（格子坐标，只读显示）
};
