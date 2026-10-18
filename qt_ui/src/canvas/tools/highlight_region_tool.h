#pragma once

#include "canvas/tools/tool_base.h"

/// 高亮区域工具：点击格子 → 高亮与该格同色的相连区域（4-连通），
/// 区域外所有格子变暗，方便观察区域范围（原理同油漆桶，但只观察不修改）。
///
/// - 左键点击格子：高亮该色连通区域（可反复点击切换观察目标）；
/// - 右键 / Esc：清除高亮；
/// - 切换工具时自动清除高亮。
class HighlightRegionTool : public ToolBase
{
    Q_OBJECT

public:
    explicit HighlightRegionTool(QObject* parent = nullptr);
    ~HighlightRegionTool() override;

    QString toolId() const override { return QStringLiteral("tool.highlight"); }
    QCursor cursor() const override;

    void deactivate() override;

signals:
    /// 请求高亮 (x,y) 所在同色连通区域（当前激活盘格子坐标）。
    void highlightRequested(int x, int y);

protected:
    void mousePress(QMouseEvent* event, QGraphicsView* view) override;
    void keyPress(QKeyEvent* event, QGraphicsView* view) override;

private:
    void clearHighlight();
};
