#pragma once

#include <QObject>
#include <QPoint>

class QMouseEvent;
class QWheelEvent;
class QKeyEvent;
class QGraphicsView;
class CraftScene;

/// 画布工具基类（开发大纲 canvas/tools）。
/// 工具以事件过滤器形式挂在 QGraphicsView::viewport() 上，
/// 鼠标 / 触摸统一映射为抽象操作（第 6 章输入设备规范）。
class ToolBase : public QObject
{
    Q_OBJECT

public:
    explicit ToolBase(QObject* parent = nullptr);
    ~ToolBase() override;

    /// 工具标识（与 ShortcutManager 键位对应）。
    virtual QString toolId() const = 0;

    /// 激活 / 取消激活（安装或移除事件过滤器）。
    virtual void activate(QGraphicsView* view, CraftScene* scene);
    virtual void deactivate();

    /// 光标。
    virtual QCursor cursor() const;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

    virtual void mousePress(QMouseEvent* event, QGraphicsView* view);
    virtual void mouseMove(QMouseEvent* event, QGraphicsView* view);
    virtual void mouseRelease(QMouseEvent* event, QGraphicsView* view);
    virtual void mouseDoubleClick(QMouseEvent* event, QGraphicsView* view);
    virtual void wheel(QWheelEvent* event, QGraphicsView* view);
    virtual void keyPress(QKeyEvent* event, QGraphicsView* view);

    /// 右键按下（十字绣工具用它做「擦除」等反向操作）。
    virtual void rightPress(QMouseEvent* event, QGraphicsView* view);
    /// 右键按住拖动（连续擦除等反向拖拽操作）。
    virtual void rightMove(QMouseEvent* event, QGraphicsView* view);
    /// 右键松开（结束反向拖拽，合并撤销）。
    virtual void rightRelease(QMouseEvent* event, QGraphicsView* view);

    /// 视图坐标 → 场景坐标。
    static QPointF scenePos(QGraphicsView* view, const QPoint& viewportPos);

    QGraphicsView* m_view = nullptr;
    CraftScene* m_scene = nullptr;

    // 中键平移（CAD 式通用操作，任何工具下都可用）
    bool m_middlePanning = false;
    QPoint m_lastPanPos;
    // 右键按住状态（用于 rightMove/rightRelease 路由）
    bool m_rightDragging = false;
};
