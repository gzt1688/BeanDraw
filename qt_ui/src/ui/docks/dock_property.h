#pragma once

#include "ffi_bind/ffi_types.h"
#include "ffi_bind/rust_api.h"

#include "DockManager.h"

#include <QImage>

#include <array>
#include <functional>

class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QRadioButton;
class QLabel;
class QProgressBar;
class QGroupBox;
class QToolButton;
class QFormLayout;
template<typename T> class QFutureWatcher;
class ProjectDoc;

/// 属性面板（开发大纲 ui/docks/dock_property）：随模块切换参数。
///
/// 十字绣模块分组：源图 → 图纸尺寸 → 颜色 → 符号与去杂点 → 生成。
/// 生成时三种手作品类（十字绣 / 珠绣 / 拼豆）各出一张图纸；
/// 材料表 / 色卡面板切换模式即切换显示对应图纸（材料与颜色同步更新）。
/// 所有参数在生成时一次生效（无独立的应用/重排按钮）。
class DockProperty : public ads::CDockWidget
{
    Q_OBJECT

public:
    DockProperty(ProjectDoc* project, ads::CDockManager* manager, QWidget* parent = nullptr);
    ~DockProperty() override;

    /// 工程参数回填（打开工程 / 生成完成后同步面板）。
    void syncFromProject(const CmCrossStitchParams& params, const CmStitchOptions& options,
                         const CmFabricSize& size, bool hasChart);

    /// 三种模式一次生成的结果（chartsGenerated 后取走，所有权移交主窗口）。
    /// 注意：MSVC 下 unique_ptr<void, 函数指针deleter> 无默认构造函数，
    /// 不能放进 std::array 默认构造，故句柄以裸指针 + cm_chart_free 传递。
    struct ModeChartsResult
    {
        std::array<void*, 3> charts;   // 0 十字绣 / 1 珠绣 / 2 拼豆
        std::array<CmCrossStitchParams, 3> params;
        std::array<CmStitchOptions, 3> options;
    };
    bool hasModeCharts() const { return m_hasModeCharts; }
    ModeChartsResult takeModeCharts();

    /// 主窗口同步当前激活盘（0..2），「生成到当前盘」用它作为目标。
public:
    /// 生成守卫（主窗口注入）：进入后台生成前调用，返回 false = 取消本次生成
    /// （用于目标盘锁定拦截 / 已有图纸覆盖确认）。
    void setGenerateGuard(std::function<bool(int targetPlate)> guard)
    {
        m_generateGuard = std::move(guard);
    }

    void setCurrentPlateHint(int plate) { m_currentPlateHint = plate; }

    /// 当前源图（重生成某盘图纸时按新格子数重新生成用）。
    QImage sourceImage() const { return m_sourceImage; }

signals:
    void statusMessage(const QString& text, int timeoutMs);
    /// 三种模式的图纸已生成完毕，主窗口据此接管并激活当前模式。
    /// target >= 0 = 生成到该盘（当前盘）；target = -1 = 生成到新盘（盘数 +1）。
    void chartsGenerated(int target);

private:
    void buildUi();
    QGroupBox* buildSourceGroup();
    QGroupBox* buildSizeGroup();
    QGroupBox* buildColorGroup();
    QGroupBox* buildProcessGroup();
    QGroupBox* buildGenerateGroup();

    void onImportImage();
    void onGenerate(); // 按 m_pendingTarget 执行实际生成（两个盘按钮共用）
    void onGenerateToCurrentPlate();
    void onGenerateToNewPlate();
    void onGridWidthChanged(int value);
    void onSizeToGrid();
    void setBusy(bool busy);
    void updateSizeReadout();
    void applySizeModeUi();
    void retranslateUi();

    /// 单格物理尺寸（mm）：按面料 ct 换算（25.4 / ct）。
    double cellMm() const;

    CmCrossStitchParams collectParams() const;
    CmStitchOptions collectOptions() const;

    ProjectDoc* m_project = nullptr;
    bool m_lockAspect = true;
    double m_aspectRatio = 1.0;

    QImage m_sourceImage;

    // 源图（图片框：预览 + 信息 + 导入）
    QLabel* m_imagePreview = nullptr;
    QLabel* m_imageInfoLabel = nullptr;
    QPushButton* m_importButton = nullptr;

    // 图纸尺寸（单一计算方式：按网格数 或 按成品尺寸，单选框）
    QFormLayout* m_sizeForm = nullptr;
    QGroupBox* m_sourceBox = nullptr;   // 语言切换 retranslateUi 用
    QGroupBox* m_sizeBox = nullptr;
    QGroupBox* m_colorBox = nullptr;
    QGroupBox* m_processBox = nullptr;
    QGroupBox* m_generateBox = nullptr;
    QFormLayout* m_colorForm = nullptr;
    QFormLayout* m_processForm = nullptr;
    int m_sizeModeRowIdx = -1;
    int m_gridRowIdx = -1;
    int m_dimRowIdx = -1;
    int m_sizeReadoutRowIdx = -1;
    int m_targetRowIdx = -1;
    QRadioButton* m_sizeByGridRadio = nullptr;
    QRadioButton* m_sizeByTargetRadio = nullptr;
    QSpinBox* m_gridWidthSpin = nullptr;
    QSpinBox* m_gridHeightSpin = nullptr;
    QToolButton* m_lockAspectButton = nullptr;
    QSpinBox* m_fabricCountSpin = nullptr;
    QLabel* m_sizeReadoutLabel = nullptr;
    QSpinBox* m_targetWidthSpin = nullptr;
    QSpinBox* m_targetHeightSpin = nullptr;

    // 颜色
    QSpinBox* m_maxColorsSpin = nullptr;
    QComboBox* m_sampleModeCombo = nullptr;
    QCheckBox* m_ditherCheck = nullptr;
    QLabel* m_cardHintLabel = nullptr;   // 色卡提示（DMC 绣线 / Artkal 拼豆）

    // 符号与去杂点
    QComboBox* m_symbolSetCombo = nullptr;
    QComboBox* m_cleanupCombo = nullptr;
    QSpinBox* m_rareColorSpin = nullptr;

    // 生成
    QPushButton* m_generateButton = nullptr;    // 生成到当前盘
    QPushButton* m_newPlateButton = nullptr;    // 生成到新盘
    QProgressBar* m_progressBar = nullptr;
    int m_currentPlateHint = 0;                 // 主窗口同步的当前激活盘（0..2）
    int m_pendingTarget = 0;                    // 本次生成的目标（-1 = 新盘）
    std::function<bool(int)> m_generateGuard;   // 生成前守卫（false = 取消）

    // 三种模式一次生成的结果缓存（chartsGenerated 信号后由主窗口取走）
    bool m_hasModeCharts = false;
    ModeChartsResult m_modeCharts;
    std::array<CmCrossStitchParams, 3> m_modeParams;
    std::array<CmStitchOptions, 3> m_modeOptions;

    // 返回 void* 而非 ChartHandle：QFuture 要求结果可拷贝，句柄为 move-only
    QFutureWatcher<void*>* m_watcher = nullptr;
};
