#pragma once

#include "canvas/scene.h"

#include <QGraphicsObject>
#include <QSet>
#include <QTransform>

class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QWidget;

/// 十字绣网格图元：面料底色 + 色块 + 符号 + 网格线（每 10 格加粗）。
///
/// 多盘系统（类拓竹多平台）：每个盘持有一份独立的图纸快照（一盘一图），
/// 所有盘并排渲染（总览），下拉选中某盘时该盘正常亮度、其余盘压暗。
/// 渲染按缩放级别分层（LOD）：
/// - 格子像素 ≥ 3px：画细网格线
/// - 格子像素 ≥ 6px：画符号（再小则符号糊成一团，不如只留色块）
/// 且只绘制 `option->exposedRect` 覆盖的格子，大图纸缩放下不卡顿。
class CrossStitchItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit CrossStitchItem(const QVector<ChartSnapshot>& plates, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    /// 场景坐标 → 网格单元（越界返回 (-1,-1)）。
    /// 多盘下仅激活盘可编辑：点击盘不是当前激活盘时返回 (-1,-1)。
    QPoint cellAt(const QPointF& scenePos) const;

    /// 场景坐标 → 所在盘号（-1 = 未命中任何盘；用于"点击盘即激活"）。
    int plateAt(const QPointF& scenePos) const;

    /// 双击：命中某盘标题栏的盘名区域时弹出改名。
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

    /// 悬停标题栏交互按钮（锁定 / 适应窗口 / X）时光标变化并弹气泡提示。
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

    /// 左键按下预处理：命中标题栏交互区（X 删除 / 格子数修改）时消费事件。
    /// 由 ToolBase 在工具处理前调用；返回 true 表示已消费。
    bool handlePress(const QPointF& scenePos);

    /// 设置盘名列表（空项 = 用默认"盘 N"；双击盘名改名后回写设置）。
    void setPlateNames(const QStringList& names);

signals:
    /// 盘名被双击修改（plate 盘号，name 新名称）。
    void plateRenamed(int plate, const QString& name);
    /// 点击标题栏 X：请求删除该盘。
    void plateRemoveRequested(int plate);
    /// 点击标题栏适应窗口按钮：请求把该盘缩放到窗口。
    void plateFitRequested(int plate);
    /// 点击标题栏锁定按钮：请求切换该盘锁定状态（主窗口维护持久化）。
    void plateLockToggled(int plate);
    /// 点击标题栏格子数并确认：请求按新图案网格数重生成该盘。
    /// 标题栏格子数确认：裁剪/扩展盘画布（anchorLeft=1 左边固定即右边增减；
    /// anchorTop=1 上边固定即下边增减）。
    void plateGridResized(int plate, int gridW, int gridH, int anchorLeft, int anchorTop);

public:
    /// 网格单元 → 场景坐标（格中心，激活盘内）。
    QPointF cellCenter(int x, int y) const;
    /// 同上，QPoint 重载（工具代码按格坐标直接取中心点用）。
    QPointF cellCenter(const QPoint& cell) const { return cellCenter(cell.x(), cell.y()); }

    /// 单元格左上角 / 右下角场景坐标（矩形框选预览用，激活盘内）。
    QPointF cellRectTopLeft(int x, int y) const;
    QPointF cellRectBottomRight(int x, int y) const;

    /// 单元格边长（场景单位 = 1mm 映射的像素由视图缩放决定）。
    qreal cellSize() const { return m_cellSize; }

    /// 激活盘图案网格宽 / 高（格线贯穿预览等用；空盘返回 0）。
    int activeGridWidth() const { return platePatternCols(m_activePlate); }
    int activeGridHeight() const { return platePatternRows(m_activePlate); }

    /// 按面料计数联动格子物理尺寸（14ct → 1.814mm/格）。
    void setFabricCount(int fabricCount);

    /// 更新涂色状态（命令栈 apply 后调用，激活盘）。
    void setCells(const QVector<CmGridCell>& cells);

    /// 更新调色板 / 符号（换符号集后调用，激活盘）。
    void setPalette(const QVector<QColor>& palette, const QVector<QChar>& symbols);

    /// 显示模式：1 色块 / 2 十字针迹预览 / 3 拼豆圆环。
    /// （0 符号视图已并入"色块 + 底部色号按钮"，不再作为独立视图）
    int displayMode() const { return m_displayMode; }
    void setDisplayMode(int mode);

    /// 格子文本模式：0 符号 / 1 色号 / 2 无 —— 与底部颜色栏"色号"按钮全局同步。
    void setLabelMode(int mode) { m_labelMode = qBound(0, mode, 2); update(); }
    int labelMode() const { return m_labelMode; }

    // ---- 多盘系统（类拓竹多平台，一盘一图） ----
    /// 设置盘布局：盘数（1..3）与每盘四周留边格数。
    void setPlateLayout(int count, int marginCells);
    /// 替换全部盘的图纸快照（数量不足的空盘补空，多余盘丢弃）。
    void setPlates(const QVector<ChartSnapshot>& plates);
    /// 增量删除某盘（该盘及后续盘前移，实时更新画布，不重建其他盘）。
    void removePlate(int plate);
    /// 更新某一盘（索引越界忽略）的图纸快照。
    void updatePlate(int plate, const ChartSnapshot& chart);
    /// 当前盘：-1 = 总盘（所有盘并排总览，无高亮），0..count-1 = 选中盘（该盘高亮）。
    void setCurrentPlate(int index);
    /// 激活盘：当前可编辑盘（0..count-1），决定编辑坐标映射与材料表口径。
    void setActivePlate(int plate);
    /// 高亮某盘的同色相连区域（区域识别结果）：该区域外所有格子变暗，
    /// 方便观察区域范围。cells 为空集合时清除高亮。
    void setHighlight(int plate, const QSet<int>& cells);
    /// 清除高亮。
    void clearHighlight() { m_highlightValid = false; update(); }
    int plateCount() const { return m_plateCount; }
    int plateMargin() const { return m_plateMargin; }
    int currentPlate() const { return m_currentPlate; }
    int activePlate() const { return m_activePlate; }

    /// 盘锁定镜像：整体回填（刷新工程时）。
    void setPlateLocks(const QVector<bool>& locks);
    /// 单盘锁定状态设置（锁定后不能编辑，也不接收生成的图纸）。
    void setPlateLocked(int plate, bool locked);
    /// 盘 `plate` 是否锁定（缺省 false）。
    bool plateLocked(int plate) const { return plate >= 0 && plate < m_plateLocks.size()
        && m_plateLocks.at(plate); }

    /// 打印页脚用：盘 `plate` 在格子矩形 [x0,y0,w,h) 内的颜色用量明细。
    struct ColorUsage
    {
        QColor color;   // 色块颜色（调色板）
        QString code;   // DMC 色号
        QChar symbol;   // 画布符号
        int count = 0;  // 该页内针数/珠数
    };
    /// 按调色板序返回用到的颜色（空区域返回空）。
    QVector<ColorUsage> pageColorUsage(int plate, int x0, int y0, int w, int h) const;

    /// 盘 `plate` 的场景坐标矩形（适应窗口用）。
    QRectF plateRectScene(int plate) const;
    /// 盘 `plate` 的纯图案区（不含四周盘边距与标题栏）场景坐标矩形。打印分页切片基准。
    QRectF patternRectScene(int plate) const;
    /// 盘 `plate` 的图案列数（该盘自己的网格宽度）。
    int patternCols(int plate) const { return platePatternCols(plate); }
    /// 盘 `plate` 的图案行数。
    int patternRows(int plate) const { return platePatternRows(plate); }
    /// 盘 `plate` 连同标题栏的场景坐标矩形（适应窗口带标题栏）。
    QRectF plateRectWithTitleScene(int plate) const;

private:
    int platePatternCols(int plate) const;
    int platePatternRows(int plate) const;
    /// 盘 `plate` 的本地矩形（图案 + 四周留边）。
    QRectF plateRect(int plate) const;
    /// 本地坐标 → 盘号与盘内图案坐标；落在边距区或盘间空隙返回 false。
    bool localToPattern(const QPointF& local, int* plate, int* px, int* py) const;
    /// 盘 `plate` 的图纸快照（空盘返回空）。
    const ChartSnapshot& plateChart(int plate) const;
    /// 盘 `plate` 显示名（自定义名优先，否则默认"盘 N"）。
    QString plateName(int plate) const;
    /// 盘 `plate` 标题栏矩形（本地坐标：盘名 | 格子数 | 锁定 | 适应 | X）。
    QRectF titleBarRect(int plate) const;
    /// 单击盘名：弹出改名对话框（双击事件被工具层拦截，改名故走单击）。
    void renamePlate(int plate);
    /// 标题栏 X 删除按钮（本地坐标）。
    QRectF titleXRect(int plate) const;
    /// 标题栏适应窗口按钮（本地坐标，X 左侧）。
    QRectF titleFitRect(int plate) const;
    /// 标题栏锁定按钮（本地坐标，适应窗口左侧）。
    QRectF titleLockRect(int plate) const;
    /// 标题栏格子数点击区（本地坐标）。
    QRectF titleGridRect(int plate) const;
    /// 标题栏盘名区（双击改名命中区，本地坐标）。
    QRectF titleNameRect(int plate) const;
    /// 标题栏交互按钮命中：-1 无；0 = 锁定，1 = 适应窗口，2 = 删除。
    int titleButtonAt(int plate, const QPointF& local) const;
    /// 刷新标题栏按钮悬停状态（光标 + 气泡提示），状态不变时为空操作。
    void updateButtonHover(const QPointF& local, const QPoint& screenPos, QWidget* widget);
    /// 盘 `plate` 图案区宽度（含四周留边，本地坐标）。
    qreal plateWidth(int plate) const;
    /// 盘 `plate` 图案区高度（含四周留边，本地坐标）。
    qreal plateHeight(int plate) const;

    QVector<ChartSnapshot> m_plates;
    QStringList m_plateNames; // 盘名（空 = 用默认"盘 N"）
    QVector<bool> m_plateLocks; // 盘锁定状态（空 = 全部未锁定）
    qreal m_cellSize = 5.0;
    int m_displayMode = 1; // 默认色块视图（符号/色号文本由 m_labelMode 控制）
    int m_labelMode = 1;   // 格子文本：0 符号 / 1 色号 / 2 无（与底部色号按钮同步）

    int m_plateCount = 1;
    int m_plateMargin = 10;
    int m_currentPlate = -1; // -1 = 总盘（视图选择，无高亮）
    int m_activePlate = 0;   // 当前可编辑盘

    // 高亮区域（区域识别显示用）：区域内正常亮度，区域外变暗
    int m_highlightPlate = -1;
    QSet<int> m_highlightCells;
    bool m_highlightValid = false;

    // 标题栏按钮悬停命中：-1 无；0 = 锁定，1 = 适应窗口，2 = 删除。
    int m_hoverPlate = -1;
    int m_hoverButton = -1;
};
