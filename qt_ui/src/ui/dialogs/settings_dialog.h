#pragma once

#include "ui/cross_stitch_settings.h"

#include <QColor>
#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGridLayout;
class QKeySequenceEdit;
class QLabel;
class QPushButton;
class QListWidget;
class QStackedWidget;
class QSpinBox;

/// 单套配色（6 个角色色）
struct PaletteSet
{
    QColor appBackground;      // 软件背景色（主窗口 / 对话框）
    QColor dockTitle;          // 面板标题颜色
    QColor canvasBackground;   // 画布背景色
    QColor menuBar;            // 菜单栏 / 标题栏颜色
    QColor toolBar;            // 工具栏颜色
    QColor accent;             // 高亮色（单选/复选框选中、悬停等强调色，默认深红 #A11740）

    /// 该配色对应主题（false=浅色）的默认值
    static PaletteSet defaults(bool dark);
};

/// 主题配置：三套可编辑配色 + 当前选中模式。
/// mode：0 = 浅色主题，1 = 深色主题，2 = 自定义（沿用 darkBase 作为基础样式）。
struct ThemeConfig
{
    int mode = 0;
    bool darkBase = false;        // 自定义模式的基础主题
    PaletteSet palettes[3];       // 0 浅色 / 1 深色 / 2 自定义
    int startupMode = 2;          // 启动默认手作品类：0 十字绣 / 1 珠绣 / 2 拼豆（默认拼豆）

    bool isCustom() const { return mode == 2; }
    const PaletteSet& active() const { return palettes[mode]; }

    /// 出厂默认：三套均为对应主题默认色
    static ThemeConfig defaults();
};

/// 统一设置对话框（独立手作软件：通用设置与拼豆设置已合并）：
/// 左侧分类目录 + 右侧内容页，目录覆盖
/// 主题 / 启动状态（原通用设置）与
/// 用量 / 布料 / 图案 / 快捷键（原拼豆设置）。
/// 两类配置均支持实时预览，取消时由主窗口恢复进入前的备份。
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const ThemeConfig& current, const CrossStitchSettings& cross,
                            QWidget* parent = nullptr);

    /// 点击确定后的最终配置
    ThemeConfig themeResult() const { return m_config; }
    CrossStitchSettings crossResult() const { return m_cross; }

signals:
    /// 主题任意变更（实时预览）
    void themePreviewed(const ThemeConfig& config);
    /// 拼豆用量/显示等任意变更实时预览（面板用量随之刷新）
    void crossPreviewed(const CrossStitchSettings& config);

private:
    // 主题页
    void emitPreview();
    QPushButton* makeSwatch(QWidget* parent, const QColor& color);
    void updateSwatchStyles();
    static QColor* rolePtr(PaletteSet& palette, int role);

    // 拼豆设置页（原拼豆设置对话框全部页面并入）
    void buildCrossPages();
    void emitCrossChanged();
    /// 把 m_cross 同步回各控件（重置按钮用）。
    void syncCrossWidgets();

    ThemeConfig m_config;
    CrossStitchSettings m_cross;

    // 左侧目录 + 右侧内容页
    QListWidget* m_categories = nullptr;
    QStackedWidget* m_pages = nullptr;

    // 三列：列标题即主题选择按钮（浅色 / 自定义；深色已移除）
    QVector<QPushButton*> m_headerButtons;

    // 三列色块均 editable：m_swatches[列][角色]
    QPushButton* m_swatches[3][6];

    // 拼豆：用线量
    QDoubleSpinBox* m_stitchLen = nullptr;
    QSpinBox* m_strands = nullptr;
    QDoubleSpinBox* m_skeinMeters = nullptr;
    QSpinBox* m_flossMargin = nullptr;
    QLabel* m_flossPreview = nullptr;

    // 拼豆：珠绣
    QSpinBox* m_beadMargin = nullptr;
    QDoubleSpinBox* m_beadSize = nullptr;
    QCheckBox* m_countBeads = nullptr;

    // 拼豆：布料
    QSpinBox* m_fabricCount = nullptr;
    QSpinBox* m_borderMargin = nullptr;
    QSpinBox* m_extraStitches = nullptr;
    QSpinBox* m_coverage = nullptr;

    // 拼豆：图案与显示
    QSpinBox* m_maxColors = nullptr;
    QComboBox* m_symbolSet = nullptr;
    QComboBox* m_cleanup = nullptr;
    QSpinBox* m_rarePpt = nullptr;
    QCheckBox* m_dither = nullptr;
    QCheckBox* m_showSymbols = nullptr;
    QCheckBox* m_showGrid = nullptr;
    QSpinBox* m_majorGrid = nullptr;
    QSpinBox* m_plateCount = nullptr;
    QSpinBox* m_plateMargin = nullptr;

    // 拼豆：快捷键（吸管 / 看图模式）
    QKeySequenceEdit* m_keyPicker = nullptr;
    QKeySequenceEdit* m_keyViewMax = nullptr;

    // 拼豆：材料表面板列显示
    QCheckBox* m_showColorNameCol = nullptr;
    QCheckBox* m_showStitchesCol = nullptr;
    QCheckBox* m_showBeadsCol = nullptr;
    QCheckBox* m_showThreadCol = nullptr;
};
