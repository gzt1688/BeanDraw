#pragma once

#include "ffi_bind/ffi_types.h"

#include <QGraphicsScene>
#include <QVector>

class CrossStitchItem;
class PieceItem;
class ToolBase;
class TextPreviewItem;

/// 画布数据快照（与 Rust 图表解耦，避免跨线程指针问题）。
struct ChartSnapshot {
    QSize gridSize;
    QVector<CmGridCell> cells;
    QVector<QColor> palette;   // 调色板颜色（与 legend index 对应）
    QStringList colorCodes;    // DMC 色号
    QStringList colorNames;
    QVector<QChar> symbols;    // 实际分配到的符号（按用量降序分配）
    /// 符号集全集（用户「更改符号」时的候选来源，按符号集顺序）
    QVector<QChar> symbolAlphabet;
    /// 图例 / 用量统计（Rust 精确值：针数、珠数、用线长度）
    QVector<CmLegendEntry> legend;
    /// 生成参数镜像（面料计数决定格子物理尺寸；珠绣模式决定统计口径）
    CmCrossStitchParams params{};
    /// 高级选项镜像（符号集等）
    CmStitchOptions options{};
    /// 成品尺寸（毫米）
    CmFabricSize fabricSize{};
    quint32 stitchTotal = 0;
    bool valid = false;
};

/// 裁片数据快照。
struct PieceSnapshot {
    QVector<CmPoint> outline;
    QVector<CmPoint> seam;
    bool valid = false;
};

/// QGraphicsScene 画布核心（开发大纲 canvas/scene）。
/// 负责：图元管理、工具事件分发、模块内容切换。
class CraftScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit CraftScene(QObject* parent = nullptr);

    /// 十字绣模块内容（重建图元；多盘：每盘一份快照，一盘一图）。
    void setPlates(const QVector<ChartSnapshot>& plates);
    /// 就地更新某一盘图纸内容（网格尺寸未变时复用图元，避免每次涂色都重建图元）。
    /// 返回 false 表示网格尺寸已变化，调用方需改用 setPlates 全量重建。
    bool updatePlate(int plate, const ChartSnapshot& chart);
    CrossStitchItem* crossStitchItem() const { return m_crossStitchItem; }

    /// 自由纸样模块内容。
    void setPiece(const PieceSnapshot& piece);
    PieceItem* pieceItem() const { return m_pieceItem; }

    /// 清空全部内容。
    void clearAll();

    /// 当前工具（事件过滤器装在视图上）。
    void setCurrentTool(ToolBase* tool);
    ToolBase* currentTool() const { return m_currentTool; }

    /// 坐标 → 网格单元（越界返回 invalid）。
    QPoint cellAt(const QPointF& scenePos) const;

    /// 坐标 → 所在盘号（-1 = 未命中任何盘；点击盘即激活用）。
    int plateAt(const QPointF& scenePos) const;

    /// 文字工具预览覆盖层（场景管理生命周期；clearAll 时销毁重建后为新项）。
    TextPreviewItem* ensureTextPreview();
    /// 销毁文字预览覆盖层（确认 / 取消 / 场景重建时）。
    void clearTextPreview();
    /// 临时隐藏/恢复文字预览（打印渲染时段不把半透明预览印到纸上）。
    void setTextPreviewVisible(bool on);

    /// 上报"点击了某个盘"（工具左键按下时调用；非激活盘 → 激活请求）。
    void reportPlateClick(const QPointF& scenePos);

    /// 左键按下预处理：让图元先消费标题栏交互（X 删除 / 格子数修改）。
    /// 返回 true = 已消费，工具不再处理。
    bool handleItemPress(const QPointF& scenePos);

    /// 网格单元 → 场景坐标（格中心）。
    QPointF cellCenter(int x, int y) const;

    /// 上报鼠标悬停位置（由工具调用，发出 cellHovered）。
    void reportHover(const QPointF& scenePos);

signals:
    /// 工具切换。
    void currentToolChanged();

    /// 单元格被激活（点击 / 涂色），供命令栈记录。
    void cellActivated(int x, int y);

    /// 鼠标悬停的单元格（CAD 式坐标读数；(-1,-1) = 离开图纸）。
    void cellHovered(int x, int y);

    /// 点击了某个盘（与当前激活盘不同），请求激活该盘。
    void plateActivationRequested(int plate);

private:
    CrossStitchItem* m_crossStitchItem = nullptr;
    PieceItem* m_pieceItem = nullptr;
    ToolBase* m_currentTool = nullptr;
    PieceSnapshot m_piece;
    TextPreviewItem* m_textPreview = nullptr;
};
