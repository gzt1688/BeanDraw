#pragma once

#include "canvas/tools/tool_base.h"

#include <QPoint>
#include <QRect>
#include <QVector>

class QGraphicsRectItem;

/// 选区工具：拖拽框选矩形区域（半透明蒙层 + 8 个控制柄）。
///
/// - 空白处拖拽 = 新建 / 替换选区；
/// - 拖动控制柄 = 仅调整选区范围（不修改画布内容）；
/// - Esc = 清除选区。
///
/// 删除 / 镜像由主工具栏触发（删除按钮无选区时置灰；
/// 镜像工具在画布上点选镜像轴）。
class SelectRegionTool : public ToolBase
{
    Q_OBJECT

public:
    explicit SelectRegionTool(QObject* parent = nullptr);
    ~SelectRegionTool() override;

    QString toolId() const override { return QStringLiteral("tool.select_region"); }
    QCursor cursor() const override;

    void activate(QGraphicsView* view, CraftScene* scene) override;
    void deactivate() override;

    /// 当前是否有有效选区。
    bool hasSelection() const { return m_active; }
    /// 当前选区（格子坐标，闭区间）。无选区返回空矩形。
    QRect selectionCells() const { return m_sel; }

    /// 设置选区（格子坐标，闭区间；外部调用如移动后选区跟随）。
    void setSelection(const QRect& sel);

    /// 清除选区（外部调用，如切换工具/删除盘）。
    void clearSelection();

signals:
    /// 选区建立 / 更新 / 清除（active=false 表示已清除）。
    void selectionChanged(int x0, int y0, int x1, int y1, bool active);
    /// 控制柄拖拽中：新选区预览（nx0..ny1 为当前选区）。
    void scaleRegionPreview(int nx0, int ny0, int nx1, int ny1);
    /// 方向键移动选区内容（dx/dy 为格子偏移，每次 1 格）。
    void moveSelectionRequested(int dx, int dy);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;
    void keyPress(QKeyEvent* event, QGraphicsView* view) override;

private:
    enum Handle {
        HandleNone = -1,
        HandleTL,
        HandleTR,
        HandleBL,
        HandleBR,
        HandleT,
        HandleB,
        HandleL,
        HandleR
    };

    void updateOverlay();
    void updateHandles();
    void createHandles();
    void destroyHandles();
    void emitSelectionChanged();
    int handleAt(const QPointF& scenePos) const;
    bool cellValid(const QPoint& cell) const;

    QRect m_sel;               ///< 当前选区（格子坐标，闭区间）
    bool m_active = false;
    bool m_dragging = false;   ///< 新建框选中
    bool m_resizing = false;   ///< 控制柄缩放中
    int m_handle = HandleNone;
    QPoint m_dragAnchor;       ///< 框选起点格子
    QGraphicsRectItem* m_overlay = nullptr;
    QVector<QGraphicsRectItem*> m_handles;
};
