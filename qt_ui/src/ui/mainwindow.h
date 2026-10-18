#pragma once

#include "canvas/scene.h"
#include "ffi_bind/rust_api.h"
#include "project/project_doc.h"
#include "ui/cross_stitch_settings.h"
#include "ui/dialogs/settings_dialog.h"
#include <QMainWindow>

#include <array>

class QGraphicsView;
class QMenuBar;
class QToolBar;
class QToolButton;
class QLabel;
class QActionGroup;
class QPrinter;
class DockProperty;
class DockLegend;
class DockMaterial;
class DockColors;
class DockParams;
class SelectTool;
class StitchTool;
class FillTool;
class RectFillTool;
class ShapeFillTool;
class PolygonFillTool;
class SelectRegionTool;
class MirrorTool;
class HighlightRegionTool;
class PickerTool;
class EraserTool;
class TextTool;
class TextPreviewItem;

namespace ads {
class CDockManager;
class CDockWidget;
}

/// 主窗口（开发大纲 ui/mainwindow）：ADS 停靠布局 + 菜单 + 工具栏 + 模块联动。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    /// 禁用工具栏 / 停靠区的默认右键弹出菜单（颜色栏不需要，ADS 面板菜单不受影响）。
    QMenu* createPopupMenu() override;

private:
    void buildUi();
    void buildMenus();
    void buildToolbar();
    void buildDocks();
    void buildPanelMenu();
    void buildConnections();
    void retranslateUi();

    /// 底部状态栏命令提示（常驻左侧，不被临时消息覆盖）。
    void showHint(const QString& text);

    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveAsProject();
    void onAbout();
    void onUndo();
    void onRedo();
    void onZoomIn();
    void onZoomOut();
    void onFitView();
    void onToolSelected(QAction* action);
    void onCellPaintRequested(int x, int y);
    void onCellEraseRequested(int x, int y);
    void onTextAnchorRequested(int cx, int cy);
    void onTextParamsChanged();
    void onTextConfirmRequested();
    void onTextCancelPreview();
    /// 「放置」按钮：一键切到文字工具（入口不在工具栏）。
    void onTextPlaceRequested();
    /// 图纸打印预览：打印按钮入口，弹 QPrintPreviewDialog。
    void onPrintChart();
    /// 渲染当前激活盘到指定打印机（预览/实打共用，QPrintPreviewDialog 回调）。
    void onRenderPrintChart(QPrinter* printer, CrossStitchItem* item);
    /// 看图模式：隐藏全部面板 / 工具栏 / 菜单栏 / 状态栏，画布铺满整窗（再按还原）。
    void toggleViewMode();
    /// 把拼豆设置里的快捷键应用到动作（吸管 / 看图模式）。
    void applyShortcuts();
    void onFillRequested(int x, int y);
    void onFillEraseRequested(int x, int y);
    void onRectFillRequested(int x0, int y0, int x1, int y1);
    void onRectEraseRequested(int x0, int y0, int x1, int y1);
    void onShapeFillRequested(int shape, int x0, int y0, int x1, int y1);
    void onShapeEraseRequested(int shape, int x0, int y0, int x1, int y1);
    void onPolygonFillRequested(const QVector<QPoint>& vertices);
    void onPolygonEraseRequested(const QVector<QPoint>& vertices);
    void onSelectionChanged(int x0, int y0, int x1, int y1, bool active);
    void onScaleRegionPreview(int nx0, int ny0, int nx1, int ny1);
    void onDeleteSelection();
    void onMoveSelection(int dx, int dy);
    void onMirrorAxisRequested(double ax0, double ay0, double ax1, double ay1);
    void onHighlightRequested(int x, int y);
    void onMirrorWhole(bool horizontal);
    void onMirrorModeToggled(int mode, bool on);
    void onColorPicked(int x, int y);
    void onReplaceColorRequested(quint32 from, quint32 to);
    void onColorMerged(int from, int to);
    /// 从色卡库选新色号替换（chartAddColor + merge = 替换）。
    void onColorReplaceWithCard(int from, const QString& code);
    void onColorDeleted(int colorIndex);
    void onSymbolChanged(int colorIndex, uint symbol);
    void onCenterViewRequested(qreal sceneX, qreal sceneY);
    void onColorActivated(int index);
    void onAddColorFromCard(const QString& code);
    void onPruneUnusedColors();
    void onStrokeStarted();
    void onStrokeFinished();
    void onLanguageChanged(const QString& language);
    void onSwitchLanguage(const QString& language);
    void refreshFromProject();
    void updateUndoRedoActions();
    void fitView();
    /// 只把当前激活盘适应窗口（工具栏适应窗口按钮左键）。
    void fitActivePlate();
    /// 把指定盘适应窗口（标题栏适应窗口按钮；非法盘号回退全部）。
    void fitPlate(int plate);
    /// 总盘汇总：所有盘当前模式的材料按色号合并统计。
    ChartSnapshot aggregatedSnapshot() const;
    void checkAutosaveRecovery();
    void onToggleTheme();
    void applyTheme();
    void loadThemeConfig();
    void saveThemeConfig();
    void onSettings();
    void toggleMaximize();

    /// 自定义标题栏（菜单 + 功能按钮 + 模块下拉 + 窗口控制）
    void buildTitleBar();

    /// 主题切换后按新前景色重绘工具栏图标（矢量图标自动适配深浅主题）。
    void refreshToolIcons();

    /// 编辑后刷新画布与面板（网格尺寸未变时复用图元）。
    void syncCanvasAfterEdit(bool fullRebuild);

    /// 三种模式图纸全部生成完成：接管句柄并激活当前模式。
    void onChartsGenerated(int targetPlate);
    /// 切换当前手作品类（0 十字绣 / 1 珠绣 / 2 拼豆）：同步面板并显示对应图纸。
    void setActiveCraftMode(uint32_t mode);
    /// 盘切换（材料表下拉）：-1 总盘 / 0..N-1 选中盘（设为激活盘并刷新）。
    void onPlateSelected(int plate);
    /// 新增盘：盘数 +1（≤25）并选中新盘（空盘；生成组"新盘"= 加盘并生成）。
    void addPlateSlot();
    /// 删除盘（标题栏 X）：释放该盘句柄并前移后续盘，盘数 -1。
    void removePlate(int plate);
    /// 按新图案网格数重新生成该盘（标题栏格子数修改；三模式同源图重生成）。
    /// 单纯调整盘画布格子尺寸（裁剪/扩展，图案内容不变）。
    void resizePlate(int plate, int gridW, int gridH, int anchorLeft, int anchorTop);
    /// 切换盘锁定（标题栏锁按钮）：写设置 + 同步标题栏图标。
    void onPlateLockToggled(int plate);
    /// 盘 `plate` 是否锁定（设置镜像，缺省未锁定）。
    bool plateLocked(int plate) const;
    /// 激活盘已锁定 → 状态栏提示并返回 true（编辑类槽函数统一拦截）。
    bool lockedPlateGuard();
    /// 盘 `plate` 三模式任意快照里画过 != 空色 → 已有图纸（生成覆盖确认用）。
    bool plateHasContent(int plate) const;
    /// 打印分页：纸张尺寸（默认 A4）+ 方向 + 每格毫米，按图纸格子自动拆页。
    void askPrintLayout();
    /// 打印几何（设置 + 图纸 + 打印机 → 每格像素/标尺带/自动分页矩阵）。
    struct PrintLayout
    {
        qreal pageW = 0, pageH = 0;      // 整页设备像素
        qreal dpi = 600;
        qreal cellDev = 0;               // 每格设备像素
        qreal band = 0;                  // 上/左/右标尺带宽
        qreal bottom = 0;                // 底部带（标尺 + 页脚）
        qreal marginDev = 0;             // 页边距（毫米 → 设备像素）
        int gridW = 0, gridH = 0;        // 图纸格子数
        int colsPerPage = 0, rowsPerPage = 0;
        int pageCols = 0, pageRows = 0;  // 页矩阵（原图分块排布）
    };
    /// 按当前打印设置 + 目标打印机算分页几何。
    PrintLayout printLayoutFor(const QPrinter* printer, CrossStitchItem* item) const;
    /// 在打印页上画单页（切片 + 边框 + 四边格号标尺 + 页脚）。
    /// 坐标 = 页内 (0,0) 左上；预览缩略图与正式打印共用。
    void paintPrintPage(QPainter* painter, const PrintLayout& lay, CrossStitchItem* item,
                        int row, int col) const;

    /// 某一模式的图纸（MainWindow 持有裸句柄所有权，ProjectDoc 借用激活的那份）。
    struct ModeChart
    {
        CmCrossStitchParams params;
        CmStitchOptions options;
        void* handle = nullptr; // 所有权：cm_chart_free 释放（ownsHandle=true 时）
        ChartSnapshot snapshot;
        bool valid = false;
        bool ownsHandle = true; // 打开工程时工程图归 ProjectDoc，MainWindow 仅引用
    };
    /// 某一盘的三模式图纸（一盘一图：每盘独立生成/编辑）。
    struct PlateCharts
    {
        ModeChart charts[3];
    };
    /// 释放全部盘内由本窗口持有的句柄。
    void clearPlates();
    /// 由 Rust 句柄构建快照（供多盘渲染与材料表使用）。
    static ChartSnapshot snapshotFromHandle(void* handle);

    std::array<PlateCharts, kMaxPlateCount> m_plates;
    /// 总盘模式的跨盘汇总快照（材料表显示用）。
    ChartSnapshot m_aggregated;
    uint32_t m_activeCraftMode = 2; // 0 十字绣 / 1 珠绣 / 2 拼豆（默认拼豆）
    int m_activePlate = 0;          // 当前激活盘（0..24）

    ads::CDockManager* m_dockManager = nullptr;
    ads::CDockWidget* m_centerDock = nullptr;
    QGraphicsView* m_view = nullptr;
    CraftScene* m_scene = nullptr;
    ProjectDoc* m_project = nullptr;
    ToolBase* m_activeTool = nullptr;

    DockProperty* m_dockProperty = nullptr;
    DockLegend* m_dockLegend = nullptr;
    DockMaterial* m_dockMaterial = nullptr;
    DockColors* m_dockColors = nullptr;
    DockParams* m_dockParams = nullptr;

    QActionGroup* m_toolGroup = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;

    /// 当前绣色（调色板索引，-1 = 未选）
    int m_activeColorIndex = -1;

    /// 首次显示后做一次视图适配
    bool m_pendingFirstFit = true;

    /// 当前主题 / 颜色配置（通用设置）
    ThemeConfig m_theme;

    /// 十字绣专用设置（用量估算 / 布料 / 生成 / 显示）
    CrossStitchSettings m_crossStitch;

    /// 自定义标题栏状态
    QWidget* m_titleBar = nullptr;
    QLabel* m_logoLabel = nullptr;
    QMenuBar* m_titleMenuBar = nullptr;
    QToolButton* m_languageButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QToolButton* m_maximizeButton = nullptr;
    bool m_customMaximized = false;
    QRect m_normalGeometry;

    SelectTool* m_selectTool = nullptr;
    StitchTool* m_stitchTool = nullptr;
    FillTool* m_fillTool = nullptr;
    RectFillTool* m_rectFillTool = nullptr;
    ShapeFillTool* m_circleFillTool = nullptr;
    ShapeFillTool* m_ellipseFillTool = nullptr;
    PolygonFillTool* m_polygonFillTool = nullptr;
    SelectRegionTool* m_selectRegionTool = nullptr;
    MirrorTool* m_mirrorTool = nullptr;
    HighlightRegionTool* m_highlightTool = nullptr;
    PickerTool* m_pickerTool = nullptr;
    EraserTool* m_eraserTool = nullptr;
    TextTool* m_textTool = nullptr;

    // 文字工具预览状态（文字转豆子：预览 → 实时调整 → 确认生成）；
    // 覆盖层由场景管理生命周期（CraftScene::ensureTextPreview/clearTextPreview）
    bool m_textPreviewActive = false;
    QPoint m_textAnchor;            ///< 预览锚点格（图纸格坐标）
    QVector<QPoint> m_textOffsets;  ///< 确认时涂色的绝对格坐标

    void updateTextPreview();
    void clearTextPreview();

    /// 选区操作（删除 / 镜像），无选区时禁用。
    QAction* m_deleteSelectionAction = nullptr;
    /// 当前选区（格子坐标，闭区间）。
    QRect m_selectionRect;

    /// 视图显示模式动作组（符号 / 色块 / 十字针迹）
    QActionGroup* m_viewModeGroup = nullptr;
    /// 看图模式（画布最大化）动作：快捷键挂在画布视图子树上
    QAction* m_viewMaxAction = nullptr;
    /// 看图模式开关状态（记录恢复用）
    bool m_viewMaxActive = false;
    /// 进入看图模式前各面板的可见状态（0 属性 1 变换 2 材料表 3 色卡）
    std::array<bool, 4> m_viewMaxDocks = {true, true, true, true};
    /// 顶部工具栏（看图模式下隐藏）
    QToolBar* m_toolbar = nullptr;
    /// 底部状态栏常驻提示（就绪 / 当前命令说明）。
    QLabel* m_hintLabel = nullptr;

    /// 一笔涂色期间合并撤销记录
    bool m_inStroke = false;

    // 菜单（retranslateUi 需要）
    QMenu* m_fileMenu = nullptr;
    QMenu* m_panelMenu = nullptr;
    QMenu* m_languageMenu = nullptr;
    QMenu* m_helpMenu = nullptr;
};
