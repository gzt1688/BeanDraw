#pragma once

#include "canvas/tools/tool_base.h"

/// 绣笔工具：单格涂色 + 拖拽连续涂色，右键擦除。
///
/// 一次按下→拖动→松开视为**一笔**（stroke）：
/// 期间每一格都即时上报，但由 ProjectDoc 在 strokeStarted/strokeFinished
/// 之间只压一条命令栈记录，保证 Ctrl+Z 一次撤销整笔（开发大纲第 13 章）。
class StitchTool : public ToolBase
{
    Q_OBJECT

public:
    explicit StitchTool(QObject* parent = nullptr);

    QString toolId() const override { return QStringLiteral("tool.stitch"); }
    QCursor cursor() const override;

signals:
    /// 请求用当前绣色涂抹单元格（由 ProjectDoc 经命令栈落库）。
    void cellPaintRequested(int x, int y);
    /// 请求擦除单元格（置为留空）。
    void cellEraseRequested(int x, int y);
    /// 一笔开始 / 结束（用于合并撤销粒度）。
    void strokeStarted();
    void strokeFinished();

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void mouseMove(QMouseEvent* event, QGraphicsView* view) override;
    void mouseRelease(QMouseEvent* event, QGraphicsView* view) override;
    void rightPress(QMouseEvent* event, QGraphicsView* view) override;
    void rightMove(QMouseEvent* event, QGraphicsView* view) override;
    void rightRelease(QMouseEvent* event, QGraphicsView* view) override;

private:
    /// 上报单元格（返回 false 表示越界未处理）。
    bool emitCellAt(QMouseEvent* event, QGraphicsView* view, bool erase);

    bool m_strokeActive = false;
    bool m_lastWasErase = false;
    QPoint m_lastCell;
};
