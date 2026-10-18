#pragma once

#include "DockManager.h"

#include "canvas/scene.h"
#include "ui/cross_stitch_settings.h"

#include <QColor>
#include <QTableWidget>

class QLabel;
class QMenu;
class QComboBox;
class QRadioButton;
class QToolButton;
class QPushButton;
class ProjectDoc;

/// 表格内拖拽合并用的 MIME（携带被拖行的调色板索引）。
constexpr char kLegendDragMime[] = "application/x-legend-color";

/// 支持「拖到行上即合并」的材料表。
///
/// 拖拽载荷携带被拖行的调色板索引，放下时取出目标行索引，
/// 两索引不同则发出 mergeRequested(from, to)——即「把 from 合并进 to」。
class LegendTable : public QTableWidget
{
    Q_OBJECT

public:
    explicit LegendTable(QWidget* parent = nullptr);

signals:
    void mergeRequested(int from, int to);

protected:
    void startDrag(Qt::DropActions actions) override;

    /// 拖拽载荷：携带被拖行的调色板索引（供 dropEvent 解析）。
    /// 注意 QTableWidget 的签名是 QList<QTableWidgetItem*>，不是 QModelIndexList。
    QMimeData* mimeData(const QList<QTableWidgetItem*>& items) const override;

    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
};

/// 材料表面板（开发大纲 ui/docks）：序号、符号、DMC 色号、针数、珠数、用线。
///
/// 统计全部取 Rust 图例的精确值（含珠数与用线长度），
/// 用线 / 珠子列按「十字绣设置」里的余量与股数换算。
/// 表格内可直接拖拽：把某一行拖到另一行上即「合并到该色」（源行随之消失）。
/// 行右键菜单可「设为当前绣色 / 合并到其他颜色」。
class DockLegend : public ads::CDockWidget
{
    Q_OBJECT

public:
    explicit DockLegend(ProjectDoc* project, ads::CDockManager* manager, QWidget* parent = nullptr);

    /// 图表刷新。
    void refresh(const ChartSnapshot& snapshot);

    /// 注入用量计算设置（单针线长 / 余量 / 股数等），随后自动重算表格。
    void setUsageSettings(const CrossStitchSettings& settings);

    /// 手作品类模式联动（0 十字绣 / 1 珠绣 / 2 拼豆）：切换顶部单选框，
    /// 并只显示该模式需要的材料参数列（珠绣/拼豆不显示"用线"）。
    void setCraftMode(uint32_t mode);

    /// 盘数（1..3）：刷新「盘」下拉框选项（总盘 / 盘1..盘N）。
    void setPlateCount(int count);
    /// 当前选中盘（-1 = 总盘）。
    int currentPlate() const { return m_currentPlate; }
    /// 选中某个盘（-1 = 总盘；0..N-1 = 盘N），触发 plateSelected。
    /// 选中某盘（下拉更新；emitChanged=false 时只更新显示不触发 plateSelected，
    /// 供删除盘等需要静默同步的场景使用）。
    void setCurrentPlateOption(int plate, bool emitChanged = true);

    /// 选中某个颜色（添加颜色后自动设为当前画笔色；表格会高亮对应行）。
    void selectColorIndex(int colorIndex);

    /// 注入主题高亮色（%ACCENT%，设置里可改）；材料表选中高亮/外框统一用它。
    void setAccentColor(const QColor& color);

    /// 打印材料表：把面板当前显示的表格（含列可见性/换算用量）打印出来。
    void printMaterialList();

    /// 语言切换：重设面板内全部 UI 文字（三模式单选 / 盘下拉 / 表头 / 按钮等）。
    void retranslateUi();

    /// 打印材料表 HTML（printMaterialList 预览与落纸共用）。
    QString materialListHtml() const;

    /// 当前主题高亮色（默认 #A11740）。
    QColor accentColor() const { return m_accentColor; }

signals:
    void statusMessage(const QString& text, int timeoutMs);
    /// 材料表打印预览里调整了整体缩放（%），主窗口写回并持久化该参数。
    void printLegendScaleChanged(int percent);
    /// 手作品类单选框切换（0 十字绣 / 1 珠绣 / 2 拼豆），主窗口据此切换图纸。
    void craftModeChanged(uint32_t mode);
    /// 盘下拉框切换：-1 = 总盘（全部并排），0..N-1 = 单个盘。
    void plateSelected(int plateIndex);
    /// 「新增盘」按钮：请求把盘数 +1（主窗口执行并激活新盘）。
    void plateAddRequested();
    /// 选中某色作为当前绣色（绣笔 / 油漆桶使用该色）。
    void colorActivated(int colorIndex);
    /// 合并颜色：from 的针全部改用 to，from 色块随之从调色板移除。
    void colorMerged(int from, int to);
    /// 从色卡库选新色号替换（from 色的针全部改用该新色，from 随之移除）。
    void colorReplaceWithCardRequested(int from, const QString& code);
    /// 删除颜色：该色所有针清空，色块从调色板移除。
    void colorDeleteRequested(int colorIndex);
    /// 更改符号：colorIndex 改用 symbol（码点）；冲突时内核自动交换。
    void symbolChangeRequested(int colorIndex, uint symbol);

private:
    void buildUi();
    void applyModeColumns();
    void showContextMenu(const QPoint& pos);
    void onMergeColor();
    void onChangeSymbol(int colorIndex);

    ProjectDoc* m_project = nullptr;
    LegendTable* m_table = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_footLabel = nullptr;
    QPushButton* m_printButton = nullptr;
    QRadioButton* m_crossRadio = nullptr;
    QRadioButton* m_beadRadio = nullptr;
    QRadioButton* m_perlerRadio = nullptr;
    QComboBox* m_plateCombo = nullptr;
    QToolButton* m_addPlateButton = nullptr;
    int m_plateCount = 1;
    int m_currentPlate = -1; // -1 = 总盘
    uint32_t m_craftMode = 0; // 0 十字绣 / 1 珠绣 / 2 拼豆
    ChartSnapshot m_snapshot;
    CrossStitchSettings m_usage;
    /// 记住上一次选中的调色板索引（刷新后保持选中，不打断当前画笔颜色）。
    int m_selectedIndex = -1;
    /// 主题高亮色（默认 #A11740 深红，随设置里主题高亮色联动）。
    QColor m_accentColor = QColor(161, 23, 64);
};
