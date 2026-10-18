#include "mainwindow.h"

#include "project/project_doc.h"
#include "shortcut_manager.h"
#include "ui/dialogs/about_dialog.h"
#include "ui/dialogs/settings_dialog.h"
#include "ui/docks/dock_colors.h"
#include "ui/docks/dock_legend.h"
#include "ui/docks/dock_material.h"
#include "ui/docks/dock_property.h"
#include "ui/i18n/tr_manager.h"
#include "ui/icon_factory.h"
#include "canvas/tools/eraser_tool.h"
#include "canvas/tools/fill_tool.h"
#include "canvas/tools/mirror_tool.h"
#include "canvas/tools/highlight_region_tool.h"
#include "canvas/tools/pan_tool.h"
#include "canvas/tools/picker_tool.h"
#include "canvas/tools/polygon_fill_tool.h"
#include "canvas/tools/rect_fill_tool.h"
#include "canvas/tools/select_region_tool.h"
#include "canvas/tools/select_tool.h"
#include "canvas/tools/shape_fill_tool.h"
#include "canvas/tools/stitch_tool.h"
#include "canvas/tools/text_tool.h"
#include "ui/docks/dock_params.h"
#include "canvas/tools/zoom_tool.h"
#include "canvas/items/cross_stitch_item.h"
#include "canvas/items/text_preview_item.h"
#include "workspace.h"

#include "DockManager.h"
#include "DockAreaWidget.h"

#ifdef Q_OS_WINDOWS
#include <qt_windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#include <QAction>
#include <QActionGroup>
#include <QDebug>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QButtonGroup>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QGraphicsView>
#include <QHash>
#include <QInputDialog>
#include <QLabel>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QPixmap>
#include <QMenu>
#include <QMenuBar>
#include <QPrintDialog>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QWindow>
#include <QMessageBox>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QStyle>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    qInfo("[CM-boot] MainWindow ctor enter");
    setObjectName(QStringLiteral("MainWindow"));
    // 自定义标题栏：无边框窗口，菜单直接融入标题栏（Photon Studio 风格）
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    resize(1400, 900);

    buildUi();
    qInfo("[CM-boot] buildUi done");
    buildMenus();
    buildToolbar();
    qInfo("[CM-boot] toolbar done");
    buildDocks();
    buildPanelMenu();
    buildConnections();
    retranslateUi();
    qInfo("[CM-boot] ui built");

    // 恢复上次保存的主题 / 颜色配置
    loadThemeConfig();
    applyTheme();
    qInfo("[CM-boot] theme applied");

    // 启动默认模块（设置 → 起始状态；默认拼豆）
    m_activeCraftMode = m_theme.startupMode;

    // 十字绣专用设置（用量估算 / 布料 / 生成 / 显示）
    m_crossStitch.load();
    // 快捷键（吸管 / 看图模式）按持久化配置应用（覆盖 ShortcutManager 默认值）
    applyShortcuts();

    // 启动：崩溃恢复检查 → 新建空工程
    checkAutosaveRecovery();
    qInfo("[CM-boot] autosave check done");
    m_project->newChart(QSize(100, 100), rust_api::defaultCrossStitchParams());
    m_project->startAutosave();
    // 按启动默认模块接管/创建对应模式空图（模式与数据必须一致，
    // 否则空盘填色写进的句柄与切换后读取的句柄不是同一个，数据会丢）
    setActiveCraftMode(m_activeCraftMode);

    // 底部操作提示栏：常驻左侧（就绪），临时消息（保存结果 / 格坐标等）走右侧
    statusBar();
    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName(QStringLiteral("statusHint"));
    statusBar()->addPermanentWidget(m_hintLabel);
    showHint(tr("Ready"));
    qInfo("[CM-boot] MainWindow ctor done");
}

void MainWindow::showHint(const QString& text)
{
    if (m_hintLabel) {
        m_hintLabel->setText(text);
    }
}

MainWindow::~MainWindow()
{
    // 各盘三模式图纸句柄所有权在本窗口（工程打开的引用句柄除外）
    clearPlates();
}

/// 看图模式：把画布铺满整个窗口——左右停靠区、顶部工具栏、底部颜色栏、
/// 菜单栏、状态栏全部隐藏；再按（空格）恢复原布局。
/// 快捷键挂在画布视图子树（WidgetWithChildren），
/// 面板输入框内打字按空格不会误触此模式。
void MainWindow::toggleViewMode()
{
    const bool on = !m_viewMaxActive;
    m_viewMaxActive = on;

    // 左右两侧面板逐个收起 / 恢复（记录进入前的可见状态，还原原样恢复；
    // 整区面板全部关闭时 ADS 停靠区自动隐藏，画布随之铺满）
    ads::CDockWidget* const docks[] = {m_dockProperty, m_dockParams,
                                       m_dockLegend, m_dockMaterial};
    for (int i = 0; i < 4; ++i) {
        if (!docks[i]) {
            continue;
        }
        if (on) {
            m_viewMaxDocks[i] = docks[i]->isVisible();
            docks[i]->toggleView(false);
        } else if (m_viewMaxDocks[i]) {
            docks[i]->toggleView(true);
        }
    }

    // 工具栏（顶部工具条 + 底部颜色栏）、菜单栏、状态栏
    if (m_toolbar) {
        m_toolbar->setVisible(!on);
    }
    if (m_dockColors) {
        m_dockColors->setVisible(!on);
    }
    if (m_titleMenuBar) {
        m_titleMenuBar->setVisible(!on);
    }
    statusBar()->setVisible(!on);

    // 焦点回到画布：看图模式下还原快捷键继续可用
    if (m_view) {
        m_view->setFocus();
    }
    showHint(on ? tr("View mode: all panels hidden, press Space to restore")
                : tr("Ready"));
}

/// 拼豆设置 → 快捷键：把配置的按键应用到吸管工具与看图模式动作。
void MainWindow::applyShortcuts()
{
    if (m_toolGroup) {
        for (QAction* a : m_toolGroup->actions()) {
            if (a->objectName() == QLatin1String("tool.picker")) {
                a->setShortcut(QKeySequence(m_crossStitch.keyPicker));
            }
        }
    }
    if (m_viewMaxAction) {
        m_viewMaxAction->setShortcut(QKeySequence(m_crossStitch.keyViewMax));
    }
}

QMenu* MainWindow::createPopupMenu()
{
    // 禁用 QMainWindow 在工具栏 / 停靠区右击弹出的"工具栏列表"菜单：
    // 底部颜色栏不需要该菜单（右击色块另有自己的菜单），顶部工具栏也不需要。
    // 注意：ADS 停靠面板的右键菜单由 Advanced Docking System 自行处理，不受影响。
    return nullptr;
}

void MainWindow::buildUi()
{
    // 工程文档（窗口持有）
    m_project = new ProjectDoc(this);
    m_project->setObjectName(QStringLiteral("ProjectDoc"));
    m_activeTool = nullptr;

    // 自定义标题栏（含菜单栏，须先于 buildMenus）
    buildTitleBar();

    // ADS 停靠管理必须先于中心部件创建：
    // CDockManager 构造函数会接管 QMainWindow 的中心区（setCentralWidget(this)），
    // 之后再设置普通中心部件会被“孤儿化”（挤成 84x14 细条）。
    //
    // 配置标志是静态的，必须在实例化 CDockManager 之前设置：
    // - 去掉标签页标题右边的 X（ActiveTabHasCloseButton / AllTabsHaveCloseButton）
    // - 去掉面板标题栏右侧的三个小按钮（关闭 / 浮动 / 标签页菜单）
    ads::CDockManager::setConfigFlag(ads::CDockManager::ActiveTabHasCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::AllTabsHaveCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, false);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::DockAreaHasAutoHideButton, false);
    ads::CDockManager::setAutoHideConfigFlag(ads::CDockManager::AutoHideHasMinimizeButton, false);

    m_dockManager = new ads::CDockManager(this);

    // 画布
    m_scene = new CraftScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing, false);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    // CAD 式画布：无边框、无滚动条（滚轮缩放 / 中键平移）
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 画布作为 ADS 中央部件：CAD 式主工作区（画图 / 编辑 / 预览都在此）
    m_centerDock = new ads::CDockWidget(m_dockManager, QString(), this);
    m_centerDock->setObjectName(QStringLiteral("CenterCanvas"));
    m_centerDock->setWidget(m_view);
    m_centerDock->setFeatures(ads::CDockWidget::NoDockWidgetFeatures);
    m_dockManager->setCentralWidget(m_centerDock);
}

void MainWindow::buildTitleBar()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("TitleBar"));
    m_titleBar->setFixedHeight(42);
    m_titleBar->installEventFilter(this);

    auto* lay = new QHBoxLayout(m_titleBar);
    lay->setContentsMargins(4, 0, 0, 0);
    lay->setSpacing(2);

    // 应用 LOGO（标题栏最左侧，融入菜单栏）
    m_logoLabel = new QLabel(m_titleBar);
    m_logoLabel->setObjectName(QStringLiteral("TitleLogo"));
    {
        QImage logoImg;
        logoImg.load(QStringLiteral(":/icons/favicon.ico"));
        if (!logoImg.isNull()) {
            const int h = 24;
            const int w = qMax(1, static_cast<int>(h * logoImg.width() / qreal(logoImg.height()) + 0.5));
            QPixmap pix = QPixmap::fromImage(
                logoImg.scaled(QSize(w, h), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            pix.setDevicePixelRatio(m_titleBar->devicePixelRatioF());
            m_logoLabel->setPixmap(pix);
        }
    }
    m_logoLabel->setFixedSize(30, 42);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    lay->addWidget(m_logoLabel);

    // 菜单栏（融入标题栏，占满高度，项内边距使其垂直居中）
    m_titleMenuBar = new QMenuBar(m_titleBar);
    m_titleMenuBar->setObjectName(QStringLiteral("TitleMenuBar"));
    m_titleMenuBar->setFixedHeight(42);
    lay->addWidget(m_titleMenuBar);

    lay->addStretch();

    // 功能图标按钮（Segoe 图标字体，Windows 10/11 内置，带回退链）
    QFont glyphFont;
    glyphFont.setFamilies({QStringLiteral("Segoe Fluent Icons"),
                           QStringLiteral("Segoe MDL2 Assets")});
    glyphFont.setPointSize(10);
    auto makeButton = [this, &glyphFont](ushort glyph, const char* name) {
        auto* btn = new QToolButton(m_titleBar);
        btn->setObjectName(QLatin1String(name));
        btn->setFont(glyphFont);
        btn->setText(QChar(glyph));
        btn->setAutoRaise(true);
        btn->setFixedSize(42, 32);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    };

    m_languageButton = makeButton(0xE774, "TitleLangButton");    // 语言（地球）
    // 主题切换按钮已按需求移除（主题仍随系统 / 设置持久化生效）
    m_settingsButton = makeButton(0xE713, "TitleSettingsButton");// 设置（齿轮）
    auto* minButton = makeButton(0xE921, "TitleMinButton");      // 最小化
    m_maximizeButton = makeButton(0xE922, "TitleMaxButton");     // 最大化
    auto* closeButton = makeButton(0xE8BB, "TitleCloseButton");  // 关闭

    connect(m_languageButton, &QToolButton::clicked, this, [this]() {
        m_languageMenu->popup(m_languageButton->mapToGlobal(
            QPoint(0, m_languageButton->height())));
    });
    connect(m_settingsButton, &QToolButton::clicked, this, &MainWindow::onSettings);
    connect(minButton, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QToolButton::clicked, this, &MainWindow::toggleMaximize);
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);

    // 顺序：语言 / 设置 → 窗口控制（模块下拉已随拆分移除，主题按钮已移除）
    lay->addWidget(m_languageButton);
    lay->addWidget(m_settingsButton);
    lay->addSpacing(8);
    lay->addWidget(minButton);
    lay->addWidget(m_maximizeButton);
    lay->addWidget(closeButton);

    setMenuWidget(m_titleBar);
}

void MainWindow::buildMenus()
{
    m_fileMenu = m_titleMenuBar->addMenu(QString());
    auto* newAction = m_fileMenu->addAction(QString());
    newAction->setObjectName(QStringLiteral("file.new"));
    connect(newAction, &QAction::triggered, this, &MainWindow::onNewProject);

    auto* openAction = m_fileMenu->addAction(QString());
    openAction->setObjectName(QStringLiteral("file.open"));
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenProject);

    m_fileMenu->addSeparator();

    auto* saveAction = m_fileMenu->addAction(QString());
    saveAction->setObjectName(QStringLiteral("file.save"));
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveProject);

    auto* saveAsAction = m_fileMenu->addAction(QString());
    saveAsAction->setObjectName(QStringLiteral("file.save_as"));
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAsProject);

    m_fileMenu->addSeparator();

    auto* quitAction = m_fileMenu->addAction(QString());
    quitAction->setMenuRole(QAction::QuitRole);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // ！编辑 / 视图 / 工作区菜单已按需求移除（菜单栏只留 文件 / 面板 / 帮助）。
    // undo / redo 仍保留为独立 QAction：主工具栏共用同一实例的图标 / 状态同步。
    m_undoAction = new QAction(this);
    m_undoAction->setObjectName(QStringLiteral("edit.undo"));
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);
    m_redoAction = new QAction(this);
    m_redoAction->setObjectName(QStringLiteral("edit.redo"));
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    // 面板菜单：按模块分组列出所有面板，可显示 / 隐藏
    m_panelMenu = m_titleMenuBar->addMenu(QString());
    m_panelMenu->setObjectName(QStringLiteral("PanelMenu"));

    // 语言菜单（不挂菜单栏，由标题栏地球按钮弹出）
    m_languageMenu = new QMenu(this);
    auto* languageGroup = new QActionGroup(m_languageMenu);
    languageGroup->setExclusive(true);
    for (const QString& lang : TrManager::supportedLanguages()) {
        // 显示本地化名称（zh_CN → 中文 / en_US → English），语言代码仅作数据
        const QString display =
            lang == QLatin1String("zh_CN") ? tr("Chinese") : tr("English");
        auto* action = m_languageMenu->addAction(display);
        action->setCheckable(true);
        action->setChecked(lang == TrManager::instance().currentLanguage());
        action->setData(lang);
        languageGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, lang]() { onSwitchLanguage(lang); });
    }

    m_helpMenu = m_titleMenuBar->addMenu(QString());
    auto* aboutAction = m_helpMenu->addAction(QString());
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::buildToolbar()
{
    m_toolbar = addToolBar(QString());
    auto* toolbar = m_toolbar;
    toolbar->setObjectName(QStringLiteral("MainToolBar"));
    toolbar->setMovable(true);
    // 图标放大到 30：图形本身也重绘得更饱满（内容区占网格 82%），
    // 两头一起加，图标在按钮里才是主体而不是"小点缀"。
    const int iconSize = IconFactory::kToolBarIconSize;
    toolbar->setIconSize(QSize(iconSize, iconSize));
    // 按钮做成正方形：上下左右对称 padding + 1px 透明描边。
    // 旧样式 vertical padding=0、horizontal=6px，按钮宽>高，点击/选中时
    // 高亮背景跟着瘦长矩形走 → 外框是"扁"的。对称后按钮 == 高亮框都是正方形。
    toolbar->setStyleSheet(QStringLiteral(
        "QToolBar#MainToolBar { padding: 1px 6px; spacing: 2px; }"
        "QToolBar#MainToolBar QToolButton { padding: 1px 1px;"
        " border: 1px solid transparent; border-radius: 8px; }"
        "QToolBar#MainToolBar QToolButton:checked { background: #d6e9ff;"
        " border-color: #7fb2e5; color: #12395e; }"
        "QToolBar#MainToolBar QToolButton:hover { background: rgba(0, 0, 0, 0.06);"
        " border-color: rgba(127, 178, 229, 0.45); }"
        "QToolBar#MainToolBar QToolButton:pressed { background: rgba(0, 0, 0, 0.12);"
        " border-color: #7fb2e5; }"
        "QToolBar#MainToolBar::separator { background: #b0b0b0; width: 2px; margin: 3px 7px; }"));

    const QColor fg = palette().color(QPalette::WindowText);

    // ---- 历史组：撤销 / 重做（与编辑菜单共用同一 QAction，状态同步） ----
    m_undoAction->setIcon(IconFactory::make(IconFactory::Kind::Undo, fg, iconSize));
    m_redoAction->setIcon(IconFactory::make(IconFactory::Kind::Redo, fg, iconSize));
    m_undoAction->setToolTip(tr("Undo"));
    m_redoAction->setToolTip(tr("Redo"));
    toolbar->addAction(m_undoAction);
    toolbar->addAction(m_redoAction);
    toolbar->addSeparator();

    // 打印按钮：放在选择按钮左边（用户指定位置；打印内容 = 当前激活盘）
    {
        QAction* printAction =
            toolbar->addAction(IconFactory::make(IconFactory::Kind::Print, fg, iconSize),
                               QString());
        printAction->setObjectName(QStringLiteral("file.print"));
        printAction->setToolTip(tr("Print chart"));
        connect(printAction, &QAction::triggered, this, &MainWindow::onPrintChart);
    }

    // 工具组
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    // 视图（观察）组：与工具同排，紧跟选择按钮
    m_viewModeGroup = new QActionGroup(this);
    m_viewModeGroup->setExclusive(true);

    m_selectTool = new SelectTool(this);
    m_stitchTool = new StitchTool(this);
    m_fillTool = new FillTool(this);
    m_rectFillTool = new RectFillTool(this);
    m_circleFillTool = new ShapeFillTool(ShapeFillTool::Circle, this);
    m_ellipseFillTool = new ShapeFillTool(ShapeFillTool::Ellipse, this);
    m_polygonFillTool = new PolygonFillTool(this);
    m_selectRegionTool = new SelectRegionTool(this);
    m_mirrorTool = new MirrorTool(this);
    m_highlightTool = new HighlightRegionTool(this);
    m_pickerTool = new PickerTool(this);
    m_eraserTool = new EraserTool(this);
    m_textTool = new TextTool(this);

    struct ToolEntry {
        ToolBase* tool;
        const char* objectName;
        IconFactory::Kind icon;
        bool separatorBefore;
    };
    // 按绣图软件的标准工作流分组：绘制 → 区域 → 取色 → 清除
    // （平移/缩放已由鼠标中键拖拽 + 滚轮实现，不再占工具栏位置）
    const ToolEntry entries[] = {
        {m_selectTool, "tool.select", IconFactory::Kind::Select, false},
        {m_selectRegionTool, "tool.select_region", IconFactory::Kind::SelectRegion, false},
        {m_stitchTool, "tool.stitch", IconFactory::Kind::Stitch, true},
        {m_fillTool, "tool.fill", IconFactory::Kind::Fill, false},
        {m_rectFillTool, "tool.rect_fill", IconFactory::Kind::RectFill, false},
        {m_circleFillTool, "tool.circle_fill", IconFactory::Kind::CircleFill, false},
        {m_ellipseFillTool, "tool.ellipse_fill", IconFactory::Kind::EllipseFill, false},
        {m_polygonFillTool, "tool.polygon_fill", IconFactory::Kind::PolygonFill, false},
        {m_highlightTool, "tool.highlight", IconFactory::Kind::Highlight, false},
        {m_pickerTool, "tool.picker", IconFactory::Kind::Picker, true},
        // 橡皮擦已放入绣笔右键（拖拽连续擦除），工具栏不再单独呈现
    };

    struct ViewModeEntry {
        const char* objectName;
        IconFactory::Kind icon;
        int mode;
        QString tooltip;
    };
    // 视图模式：色块（格子文本由底部"色号"按钮同步控制）/ 针迹预览 / 拼豆圆环
    const ViewModeEntry modes[] = {
        {"view.color_block", IconFactory::Kind::ColorBlock, 1, tr("Color block view")},
        {"view.stitch_preview", IconFactory::Kind::StitchPreview, 2, tr("Stitch preview")},
        // 拼豆圆环视图：用「Perler rings」与模块名「Perler beads」区分，
        // 避免 MainWindow 上下文同一源串两种译名（模块名=拼豆，视图=拼豆圆环）
        {"view.perler", IconFactory::Kind::Perler, 3, tr("Perler rings")},
    };

    for (const auto& entry : entries) {
        if (entry.separatorBefore) {
            toolbar->addSeparator();
        }
        auto* action = toolbar->addAction(QString());
        action->setObjectName(QLatin1String(entry.objectName));
        action->setCheckable(true);
        action->setData(QLatin1String(entry.objectName));
        action->setIcon(IconFactory::make(entry.icon, fg, iconSize));
        m_toolGroup->addAction(action);
        ShortcutManager::instance().applyTo(action);
        if (QLatin1String(entry.objectName) == QLatin1String("tool.select_region")) {
            // 选区操作：删除（无选区时置灰）；镜像为独立工具（画布点选镜像轴）
            m_deleteSelectionAction = toolbar->addAction(
                IconFactory::make(IconFactory::Kind::SelectionDelete, fg, iconSize), QString());
            m_deleteSelectionAction->setObjectName(QStringLiteral("tool.selection_delete"));
            m_deleteSelectionAction->setToolTip(tr("Delete selection"));
            m_deleteSelectionAction->setEnabled(false);
            connect(m_deleteSelectionAction, &QAction::triggered,
                    this, &MainWindow::onDeleteSelection);
        }
        if (QLatin1String(entry.objectName) == QLatin1String("tool.select")) {
            action->setChecked(true);
            // 适应窗口与选择按钮一组（2 个一组），紧随选择按钮
            auto* fitAction = toolbar->addAction(
                IconFactory::make(IconFactory::Kind::FitView, fg, iconSize), QString());
            fitAction->setObjectName(QStringLiteral("view.fit"));
            fitAction->setToolTip(tr("Left: fit active plate; right: fit all plates"));
            ShortcutManager::instance().applyTo(fitAction);
            connect(fitAction, &QAction::triggered, this, &MainWindow::fitActivePlate);
            // 右键：全部盘适应窗口（左键已切为激活盘适应）
            if (auto* fitBtn = qobject_cast<QToolButton*>(toolbar->widgetForAction(fitAction))) {
                fitBtn->setContextMenuPolicy(Qt::CustomContextMenu);
                connect(fitBtn, &QToolButton::customContextMenuRequested, this,
                        [this](const QPoint&) { fitView(); });
            }
            // 适应窗口后加分隔线：视图组与前面工具分组
            toolbar->addSeparator();
            // 全部观察（显示模式）按钮紧跟其后，同一组排列
            for (const auto& mode : modes) {
                auto* vAction = toolbar->addAction(
                    IconFactory::make(mode.icon, fg, iconSize), QString());
                vAction->setObjectName(QLatin1String(mode.objectName));
                vAction->setCheckable(true);
                vAction->setData(mode.mode);
                vAction->setChecked(mode.mode == 1); // 默认色块视图
                vAction->setToolTip(mode.tooltip);
                m_viewModeGroup->addAction(vAction);
                connect(vAction, &QAction::triggered, this, [this]() {
                    QAction* a = m_viewModeGroup->checkedAction();
                    if (a && m_scene->crossStitchItem()) {
                        m_scene->crossStitchItem()->setDisplayMode(a->data().toInt());
                        const int mode = a->data().toInt();
                        if (mode == 1) {
                            showHint(tr("Color block view: color cells, text follows the code button"));
                        } else if (mode == 2) {
                            showHint(tr("Stitch preview: cross-stitch stitches"));
                        } else {
                            showHint(tr("Perler beads view: bead rings"));
                        }
                    }
                });
            }
            // 新增盘（纯加盘，不生成；生成组另有"新盘"=加盘并生成）
            auto* addPlateAction = toolbar->addAction(
                IconFactory::make(IconFactory::Kind::AddPlate, fg, iconSize), QString());
            addPlateAction->setObjectName(QStringLiteral("tool.add_plate"));
            addPlateAction->setToolTip(tr("Add plate"));
            connect(addPlateAction, &QAction::triggered, this, [this]() {
                addPlateSlot();
            });
        }
    }
    connect(m_toolGroup, &QActionGroup::triggered, this, &MainWindow::onToolSelected);

    // 文字工具：不进工具栏（面板「放置」按钮触发），仍注册进工具组，
    // 让 onToolSelected 的激活 / 预览生命周期 / retranslate 一套逻辑照常生效
    {
        QAction* textAction = new QAction(this);
        textAction->setObjectName(QStringLiteral("tool.text"));
        textAction->setCheckable(true);
        textAction->setData(QString(QStringLiteral("tool.text")));
        m_toolGroup->addAction(textAction);
    }

    // 初始工具
    if (m_toolGroup->checkedAction()) {
        onToolSelected(m_toolGroup->checkedAction());
    }

    // ---- 看图模式（画布最大化）：空格切换（默认，可在拼豆设置里改） ----
    // 快捷键上下文是 WidgetWithChildren，且动作挂在画布视图上：
    // 只有点击画布获得焦点后才生效，面板输入框里打字时按空格不会被误触。
    m_viewMaxAction = new QAction(this);
    m_viewMaxAction->setObjectName(QStringLiteral("view.max"));
    m_viewMaxAction->setShortcut(
        ShortcutManager::instance().sequence(QStringLiteral("view.max")));
    m_viewMaxAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_view->addAction(m_viewMaxAction);
    connect(m_viewMaxAction, &QAction::triggered,
            this, &MainWindow::toggleViewMode);
}

/// 工具栏图标随主题重新着色。
void MainWindow::refreshToolIcons()
{
    const QColor fg = palette().color(QPalette::WindowText);
    // 必须带工具栏实际使用的边长：make() 的 logicalSize 决定位图分辨率，
    // 漏传就会用默认值重新生成一套偏小的图标，主题切换后图标突然缩水。
    const int iconSize = IconFactory::kToolBarIconSize;
    static const QHash<QString, IconFactory::Kind> kIconMap = {
        {QStringLiteral("tool.select"), IconFactory::Kind::Select},
        {QStringLiteral("tool.select_region"), IconFactory::Kind::SelectRegion},
        {QStringLiteral("tool.selection_delete"), IconFactory::Kind::SelectionDelete},
        {QStringLiteral("tool.stitch"), IconFactory::Kind::Stitch},
        {QStringLiteral("tool.fill"), IconFactory::Kind::Fill},
        {QStringLiteral("tool.rect_fill"), IconFactory::Kind::RectFill},
        {QStringLiteral("tool.circle_fill"), IconFactory::Kind::CircleFill},
        {QStringLiteral("tool.ellipse_fill"), IconFactory::Kind::EllipseFill},
        {QStringLiteral("tool.polygon_fill"), IconFactory::Kind::PolygonFill},
        {QStringLiteral("tool.picker"), IconFactory::Kind::Picker},
        {QStringLiteral("tool.text"), IconFactory::Kind::Text},
        {QStringLiteral("tool.eraser"), IconFactory::Kind::Eraser},
        {QStringLiteral("view.fit"), IconFactory::Kind::FitView},
        {QStringLiteral("view.color_block"), IconFactory::Kind::ColorBlock},
        {QStringLiteral("view.stitch_preview"), IconFactory::Kind::StitchPreview},
        {QStringLiteral("view.perler"), IconFactory::Kind::Perler},
    };
    for (QAction* action : findChildren<QAction*>()) {
        const auto it = kIconMap.constFind(action->objectName());
        if (it != kIconMap.constEnd()) {
            action->setIcon(IconFactory::make(*it, fg, iconSize));
        }
    }
    // 撤销 / 重做（无 objectName 区分，直接按指针定位）
    m_undoAction->setIcon(IconFactory::make(IconFactory::Kind::Undo, fg, iconSize));
    m_redoAction->setIcon(IconFactory::make(IconFactory::Kind::Redo, fg, iconSize));
}

void MainWindow::buildDocks()
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));

    m_dockProperty = new DockProperty(project, m_dockManager, this);
    m_dockLegend = new DockLegend(project, m_dockManager, this);
    m_dockMaterial = new DockMaterial(m_dockManager, this);
    // 参数面板：与图纸向导同一停靠区（Tab 切换），放置命令参数（如镜像）
    m_dockParams = new DockParams(m_dockManager, this);

    // 布局：左（属性 + 参数面板 Tab）｜中（画布）｜右（图例 + 色卡），底部通栏颜色工具栏
    m_dockProperty->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    m_dockProperty->setMinimumSize(280, 0);
    m_dockParams->setMinimumSize(240, 0);
    m_dockLegend->setMinimumSize(320, 0);
    m_dockMaterial->setMinimumSize(320, 0);

    auto* leftDock = m_dockManager->addDockWidget(ads::LeftDockWidgetArea, m_dockProperty);
    m_dockManager->addDockWidgetTabToArea(m_dockParams, leftDock);
    // 左侧区域默认聚焦图纸向导（属性面板），而非变换面板
    m_dockProperty->setAsCurrentTab();
    auto* rightDock = m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_dockLegend);
    // 色卡与材料表同处右侧同一停靠区：切换标题即切换面板，不左右并排
    m_dockManager->addDockWidgetTabToArea(m_dockMaterial, rightDock);
    // 默认显示色卡面板（而非材料表）
    m_dockMaterial->setAsCurrentTab();

    // 所有面板去掉标题右侧的关闭 X（保留可拖动 / 可浮动）
    for (ads::CDockWidget* dock : {static_cast<ads::CDockWidget*>(m_dockProperty),
                                   static_cast<ads::CDockWidget*>(m_dockParams),
                                   static_cast<ads::CDockWidget*>(m_dockLegend),
                                   static_cast<ads::CDockWidget*>(m_dockMaterial)}) {
        dock->setFeatures(ads::CDockWidget::DockWidgetMovable | ads::CDockWidget::DockWidgetFloatable
                          | ads::CDockWidget::DockWidgetFocusable);
    }

    // 底部颜色面板：作为工具栏（无标题、左侧拖动手柄、小色块 + 右侧搜索框）
    m_dockColors = new DockColors(this);
    addToolBar(Qt::BottomToolBarArea, m_dockColors);
    // 色块标签模式（符号/色号/无）→ 画布格子文本同步（拼豆模式直接显示色号）
    connect(m_dockColors, &DockColors::labelModeChanged, this,
            [this](int mode) {
                if (m_scene && m_scene->crossStitchItem()) {
                    m_scene->crossStitchItem()->setLabelMode(mode);
                }
            });
}

void MainWindow::buildPanelMenu()
{
    if (!m_panelMenu) {
        return;
    }
    m_panelMenu->clear();

    // 面板菜单：独立手作软件，全部面板直列表内（无模块分组）。
    // 顺序按使用频率：图纸向导、变换面板、材料表、色卡、画布。
    // 颜色栏是底部工具栏而非停靠面板，用自定义可勾选项（与 ADS toggle 不同）。
    for (ads::CDockWidget* dock :
         {static_cast<ads::CDockWidget*>(m_dockProperty), static_cast<ads::CDockWidget*>(m_dockParams),
          static_cast<ads::CDockWidget*>(m_dockLegend), static_cast<ads::CDockWidget*>(m_dockMaterial),
          static_cast<ads::CDockWidget*>(m_centerDock)}) {
        if (dock) {
            m_panelMenu->addAction(dock->toggleViewAction());
        }
    }

    // 颜色栏：自定义可勾选项（工具栏单独一个显隐入口）
    if (m_dockColors) {
        m_panelMenu->addSeparator();
        QAction* colorsAction = m_panelMenu->addAction(QString());
        colorsAction->setObjectName(QStringLiteral("panel.colors"));
        colorsAction->setCheckable(true);
        colorsAction->setChecked(!m_dockColors->isHidden());
        connect(colorsAction, &QAction::triggered, this, [this](bool on) {
            m_dockColors->setVisible(on);
        });
    }
}

void MainWindow::buildConnections()
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));

    connect(project, &ProjectDoc::chartChanged, this, &MainWindow::refreshFromProject);
    connect(project, &ProjectDoc::statusMessage, this, [this](const QString& text, int timeout) {
        statusBar()->showMessage(text, timeout);
    });
    connect(project, &ProjectDoc::undoRedoChanged, this, &MainWindow::updateUndoRedoActions);

    // ---- 画布工具 → 内核编辑（统一经 ProjectDoc 命令栈）----
    connect(m_stitchTool, &StitchTool::cellPaintRequested, this, &MainWindow::onCellPaintRequested);
    connect(m_stitchTool, &StitchTool::cellEraseRequested, this, &MainWindow::onCellEraseRequested);
    connect(m_stitchTool, &StitchTool::strokeStarted, this, &MainWindow::onStrokeStarted);
    connect(m_stitchTool, &StitchTool::strokeFinished, this, &MainWindow::onStrokeFinished);

    connect(m_textTool, &TextTool::textAnchorRequested, this, &MainWindow::onTextAnchorRequested);
    connect(m_textTool, &TextTool::textCancelRequested, this, &MainWindow::onTextCancelPreview);

    connect(m_fillTool, &FillTool::fillRequested, this, &MainWindow::onFillRequested);
    connect(m_fillTool, &FillTool::fillEraseRequested, this, &MainWindow::onFillEraseRequested);

    connect(m_rectFillTool, &RectFillTool::rectFillRequested,
            this, &MainWindow::onRectFillRequested);
    connect(m_rectFillTool, &RectFillTool::rectEraseRequested,
            this, &MainWindow::onRectEraseRequested);
    connect(m_circleFillTool, &ShapeFillTool::shapeFillRequested,
            this, &MainWindow::onShapeFillRequested);
    connect(m_circleFillTool, &ShapeFillTool::shapeEraseRequested,
            this, &MainWindow::onShapeEraseRequested);
    connect(m_ellipseFillTool, &ShapeFillTool::shapeFillRequested,
            this, &MainWindow::onShapeFillRequested);
    connect(m_ellipseFillTool, &ShapeFillTool::shapeEraseRequested,
            this, &MainWindow::onShapeEraseRequested);
    connect(m_polygonFillTool, &PolygonFillTool::polygonFillRequested,
            this, &MainWindow::onPolygonFillRequested);
    connect(m_polygonFillTool, &PolygonFillTool::polygonEraseRequested,
            this, &MainWindow::onPolygonEraseRequested);
    connect(m_selectRegionTool, &SelectRegionTool::selectionChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_selectRegionTool, &SelectRegionTool::scaleRegionPreview,
            this, &MainWindow::onScaleRegionPreview);
    connect(m_mirrorTool, &MirrorTool::mirrorAxisRequested,
            this, &MainWindow::onMirrorAxisRequested);
    connect(m_highlightTool, &HighlightRegionTool::highlightRequested,
            this, &MainWindow::onHighlightRequested);

    // ---- 参数面板（镜像参数）----
    connect(m_dockParams, &DockParams::mirrorWholeRequested,
            this, &MainWindow::onMirrorWhole);
    connect(m_dockParams, &DockParams::customMirrorToggled, this,
            [this](bool on) { onMirrorModeToggled(MirrorTool::TwoPoints, on); });
    connect(m_dockParams, &DockParams::xyAxisToggled, this,
            [this](bool on) { onMirrorModeToggled(MirrorTool::XyAxis, on); });
    connect(m_dockParams, &DockParams::moveRequested,
            this, &MainWindow::onMoveSelection);
    // ---- 参数面板（文字组：预览刷新 / 确认生成）----
    connect(m_dockParams, &DockParams::textParamsChanged,
            this, &MainWindow::onTextParamsChanged);
    connect(m_dockParams, &DockParams::textConfirmRequested,
            this, &MainWindow::onTextConfirmRequested);
    connect(m_dockParams, &DockParams::textPlaceRequested,
            this, &MainWindow::onTextPlaceRequested);
    connect(m_selectRegionTool, &SelectRegionTool::moveSelectionRequested,
            this, &MainWindow::onMoveSelection);

    connect(m_pickerTool, &PickerTool::colorPicked, this, &MainWindow::onColorPicked);

    connect(m_eraserTool, &EraserTool::cellEraseRequested, this, &MainWindow::onCellEraseRequested);
    connect(m_eraserTool, &EraserTool::strokeStarted, this, &MainWindow::onStrokeStarted);
    connect(m_eraserTool, &EraserTool::strokeFinished, this, &MainWindow::onStrokeFinished);

    // ---- 面板信号 ----
    connect(m_dockColors, &DockColors::colorActivated, this, &MainWindow::onColorActivated);
    connect(m_dockColors, &DockColors::pruneRequested, this, &MainWindow::onPruneUnusedColors);
    connect(m_dockColors, &DockColors::colorMerged, this, &MainWindow::onColorMerged);
    connect(m_dockColors, &DockColors::colorReplaceWithCardRequested,
            this, &MainWindow::onColorReplaceWithCard);
    connect(m_dockColors, &DockColors::symbolChangeRequested, this, &MainWindow::onSymbolChanged);
    connect(m_dockColors, &DockColors::colorDeleteRequested, this, &MainWindow::onColorDeleted);
    connect(m_dockLegend, &DockLegend::colorActivated, this, &MainWindow::onColorActivated);
    connect(m_dockLegend, &DockLegend::colorMerged, this, &MainWindow::onColorMerged);
    connect(m_dockLegend, &DockLegend::colorReplaceWithCardRequested,
            this, &MainWindow::onColorReplaceWithCard);
    connect(m_dockLegend, &DockLegend::colorDeleteRequested, this, &MainWindow::onColorDeleted);
    connect(m_dockLegend, &DockLegend::symbolChangeRequested, this, &MainWindow::onSymbolChanged);
    connect(m_dockLegend, &DockLegend::statusMessage, this,
            [this](const QString& text, int timeout) { statusBar()->showMessage(text, timeout); });
    // 材料表打印缩放调整 → 即时写回并持久化（只更新该字段，避免整结构覆盖）
    connect(m_dockLegend, &DockLegend::printLegendScaleChanged, this, [this](int pct) {
        m_crossStitch.printLegendScalePct = pct;
        m_crossStitch.save();
    });
    connect(m_dockProperty, &DockProperty::statusMessage, this,
            [this](const QString& text, int timeout) { statusBar()->showMessage(text, timeout); });
    // 三种模式图纸生成完成 → 接管并激活当前模式（target=-1 新盘 / >=0 指定盘）
    connect(m_dockProperty, &DockProperty::chartsGenerated, this,
            &MainWindow::onChartsGenerated);
    // 生成守卫（后台生成前）：目标盘锁定 → 拦截；盘上已有图纸 → 先确认覆盖
    m_dockProperty->setGenerateGuard([this](int targetPlate) {
        if (targetPlate < 0) {
            return true; // 新盘
        }
        if (plateLocked(targetPlate)) {
            statusBar()->showMessage(
                tr("Plate is locked: unlock it before generating"), 4000);
            return false;
        }
        if (plateHasContent(targetPlate)) {
            return QMessageBox::question(
                       this, tr("Generate chart"),
                       tr("Plate %1 already has a chart. Overwrite it?")
                           .arg(targetPlate + 1),
                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                   == QMessageBox::Yes;
        }
        return true;
    });
    // 材料表 / 色卡面板的模式单选框 → 切换显示对应模式的图纸（材料与颜色同步更新）
    connect(m_dockLegend, &DockLegend::craftModeChanged, this,
            &MainWindow::setActiveCraftMode);
    connect(m_dockMaterial, &DockMaterial::craftModeChanged, this,
            &MainWindow::setActiveCraftMode);
    // 材料表盘选择下拉 → 切换激活盘（-1 = 总盘浏览，不换编辑盘）
    connect(m_dockLegend, &DockLegend::plateSelected, this,
            &MainWindow::onPlateSelected);
    // 「新增盘」按钮 → 盘数 +1（≤25），空盘不生成（与生成组"新盘"互补）
    connect(m_dockLegend, &DockLegend::plateAddRequested, this, [this]() {
        addPlateSlot();
    });
    // 画布点击盘 → 激活该盘（ToolBase 左键上报）
    connect(m_scene, &CraftScene::plateActivationRequested, this, [this](int plate) {
        if (plate < 0 || plate >= m_crossStitch.plateCount || plate == m_activePlate) {
            return;
        }
        m_dockLegend->setCurrentPlateOption(plate); // 触发 plateSelected → onPlateSelected
    });

    // 色卡双击 → 加入底部颜色面板（图纸调色板）
    connect(m_dockMaterial, &DockMaterial::colorCodeDoubleClicked,
            this, &MainWindow::onAddColorFromCard);

    // CAD 式坐标读数：画布悬停格坐标显示在状态栏
    connect(m_scene, &CraftScene::cellHovered, this, [this](int x, int y) {
        if (x < 0 || y < 0) {
            statusBar()->clearMessage();
        } else {
            statusBar()->showMessage(tr("Cell (%1, %2)").arg(x).arg(y));
        }
    });

    connect(&TrManager::instance(), &TrManager::languageChanged, this, &MainWindow::onLanguageChanged);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("BeanDraw - DIY Craft Design"));

    m_fileMenu->setTitle(tr("&File"));
    for (QAction* a : m_fileMenu->actions()) {
        if (a->objectName() == QLatin1String("file.new")) a->setText(tr("&New"));
        else if (a->objectName() == QLatin1String("file.open")) a->setText(tr("&Open..."));
        else if (a->objectName() == QLatin1String("file.save")) a->setText(tr("&Save"));
        else if (a->objectName() == QLatin1String("file.save_as")) a->setText(tr("Save &As..."));
        else if (a->isSeparator()) { /* skip */ }
        else if (a->menuRole() == QAction::QuitRole) a->setText(tr("&Quit"));
    }

    // 工具栏共享的撤销 / 重做动作（原编辑菜单已移除，文本仅作 tooltip 兜底）
    m_undoAction->setText(tr("&Undo"));
    m_redoAction->setText(tr("&Redo"));

    // 工具栏独立按钮（非工具组动作）：打印
    for (QAction* a : findChildren<QAction*>()) {
        if (a->objectName() == QLatin1String("file.print")) {
            a->setText(tr("Print chart"));
            a->setToolTip(tr("Print chart"));
        }
    }

    // ！视图 / 工作区菜单已移除，retranslateUi 相应段落删除
    // 面板菜单：独立手作软件，全部面板直列（面板标题由 ADS 维护）
    if (m_panelMenu) {
        m_panelMenu->setTitle(tr("&Panels"));
        for (QAction* s : m_panelMenu->actions()) {
            if (s->objectName() == QLatin1String("panel.colors")) {
                s->setText(tr("Colour bar"));
            }
        }
    }

    // 语言菜单：按语言代码重设显示名（数据仍是语言代码，勾选逻辑不变）
    if (m_languageMenu) {
        for (QAction* a : m_languageMenu->actions()) {
            const QString code = a->data().toString();
            if (code == QLatin1String("zh_CN")) {
                a->setText(tr("Chinese"));
            } else if (code == QLatin1String("en_US")) {
                a->setText(tr("English"));
            }
        }
    }

    m_helpMenu->setTitle(tr("&Help"));
    for (QAction* a : m_helpMenu->actions()) {
        if (a->menuRole() == QAction::AboutRole) {
            a->setText(tr("&About"));
        }
    }

    // ！独立手作软件：设置入口合并，标题栏齿轮按钮直接打开统一设置对话框

    m_dockProperty->setWindowTitle(tr("Chart wizard"));
    m_dockLegend->setWindowTitle(tr("Material list"));
    m_dockMaterial->setWindowTitle(tr("Color card"));
    m_centerDock->setWindowTitle(tr("Canvas"));

    for (QAction* a : m_toolGroup->actions()) {
        const QString name = a->objectName();
        if (name == QLatin1String("tool.select")) a->setText(tr("Select"));
        else if (name == QLatin1String("tool.select_region")) a->setText(tr("Select region"));
        else if (name == QLatin1String("tool.stitch")) a->setText(tr("Stitch brush"));
        else if (name == QLatin1String("tool.fill")) a->setText(tr("Fill region"));
        else if (name == QLatin1String("tool.rect_fill")) a->setText(tr("Fill rectangle"));
        else if (name == QLatin1String("tool.circle_fill")) a->setText(tr("Fill circle"));
        else if (name == QLatin1String("tool.ellipse_fill")) a->setText(tr("Fill ellipse"));
        else if (name == QLatin1String("tool.polygon_fill")) a->setText(tr("Fill polygon"));
        else if (name == QLatin1String("tool.text")) a->setText(tr("Text"));
        else if (name == QLatin1String("tool.picker")) a->setText(tr("Pick colour"));
        else if (name == QLatin1String("tool.eraser")) a->setText(tr("Erase"));
    }

    // 工具按钮：图标 + 文字下置，工具栏拥挤时仍可辨认
    for (QAction* a : m_toolGroup->actions()) {
        a->setToolTip(a->text());
    }

    if (!m_viewModeGroup) {
        return;
    }
    for (QAction* a : m_viewModeGroup->actions()) {
        const QString name = a->objectName();
        if (name == QLatin1String("view.color_block")) {
            a->setText(tr("Colour blocks"));
            a->setToolTip(tr("Show colour blocks only"));
        } else if (name == QLatin1String("view.stitch_preview")) {
            a->setText(tr("Stitch preview"));
            a->setToolTip(tr("Preview as cross stitches"));
        } else if (name == QLatin1String("view.perler")) {
            a->setText(tr("Perler rings"));
            a->setToolTip(tr("Preview as perler beads"));
        }
    }

    // 底部状态栏随语言刷新：永久提示复位为"就绪"，右侧临时消息清空
    // （旧临时文本是切换前语言拼的，无法逐条回译）
    showHint(tr("Ready"));
    statusBar()->clearMessage();
}

// ------------------------------------------------------------
// 槽函数
// ------------------------------------------------------------

void MainWindow::onNewProject()
{
    showHint(tr("New project: create a blank chart"));
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    // 新建工程：回到默认单盘，清空全部盘的三模式缓存，回到默认十字绣
    if (m_crossStitch.plateCount != 1) {
        m_crossStitch.plateCount = 1;
        m_crossStitch.save();
    }
    m_dockLegend->setPlateCount(1);
    clearPlates();
    m_activePlate = 0;
    // 新建文件按「起始状态」设置选择默认模块（默认拼豆），模式与图纸保持一致
    m_activeCraftMode = m_theme.startupMode;
    m_dockLegend->setCraftMode(m_activeCraftMode);
    m_dockMaterial->setCraftMode(m_activeCraftMode);
    project->newChart(QSize(100, 100), rust_api::defaultCrossStitchParams());
    setActiveCraftMode(m_activeCraftMode);
    statusBar()->showMessage(tr("New project"), 2000);
}

void MainWindow::onOpenProject()
{
    showHint(tr("Open project: load a .bean file"));
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open project"), QString(), tr("BeanDraw projects (*.bean)"));
    if (path.isEmpty()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (project->open(path)) {
        // 打开工程只含当前模式一份图纸：登记到激活盘（所有权归 ProjectDoc，本窗口仅引用），
        // 其余盘与模式需重新生成
        const uint32_t projMode =
            static_cast<uint32_t>(project->options().craft_mode);
        m_activeCraftMode = projMode > 2 ? 0 : projMode;
        clearPlates();
        m_activePlate = 0;
        const void* h = project->chartHandle();
        if (h) {
            for (int mode = 0; mode < 3; ++mode) {
                ModeChart& mc = m_plates[0].charts[mode];
                mc = ModeChart{};
                mc.valid = (mode == static_cast<int>(m_activeCraftMode));
                mc.handle = const_cast<void*>(h);
                mc.ownsHandle = false; // 工程图由 ProjectDoc 持有
                if (mc.valid) {
                    mc.snapshot = snapshotFromHandle(mc.handle);
                }
            }
        }
        m_dockLegend->setCraftMode(m_activeCraftMode);
        m_dockMaterial->setCraftMode(m_activeCraftMode);
        statusBar()->showMessage(tr("Opened %1").arg(path), 3000);
    }
}

void MainWindow::onSaveProject()
{
    showHint(tr("Save project"));
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (project->currentPath().isEmpty()) {
        onSaveAsProject();
        return;
    }
    project->save();
    statusBar()->showMessage(tr("Project saved"), 2000);
}

void MainWindow::onSaveAsProject()
{
    showHint(tr("Save project as: choose a file"));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save project as"), QString(), tr("BeanDraw projects (*.bean)"));
    if (path.isEmpty()) {
        return;
    }
    findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"))->saveAs(path);
}

void MainWindow::onAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onUndo()
{
    if (lockedPlateGuard()) {
        return;
    }
    showHint(tr("Undo last step"));
    findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"))->undo();
}

void MainWindow::onRedo()
{
    if (lockedPlateGuard()) {
        return;
    }
    showHint(tr("Redo last step"));
    findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"))->redo();
}

void MainWindow::onZoomIn()
{
    showHint(tr("Zoom in"));
    m_view->scale(1.15, 1.15);
}

void MainWindow::onZoomOut()
{
    showHint(tr("Zoom out"));
    m_view->scale(1.0 / 1.15, 1.0 / 1.15);
}

void MainWindow::onFitView()
{
    showHint(tr("Fit all plates to window"));
    fitView();
}

void MainWindow::onToolSelected(QAction* action)
{
    if (!action) {
        return;
    }
    const QString toolId = action->data().toString();
    ToolBase* tool = m_selectTool;
    if (toolId == QLatin1String("tool.stitch")) {
        tool = m_stitchTool;
    } else if (toolId == QLatin1String("tool.fill")) {
        tool = m_fillTool;
    } else if (toolId == QLatin1String("tool.rect_fill")) {
        tool = m_rectFillTool;
    } else if (toolId == QLatin1String("tool.circle_fill")) {
        tool = m_circleFillTool;
    } else if (toolId == QLatin1String("tool.ellipse_fill")) {
        tool = m_ellipseFillTool;
    } else if (toolId == QLatin1String("tool.polygon_fill")) {
        tool = m_polygonFillTool;
    } else if (toolId == QLatin1String("tool.text")) {
        tool = m_textTool;
    } else if (toolId == QLatin1String("tool.highlight")) {
        tool = m_highlightTool;
    } else if (toolId == QLatin1String("tool.select_region")) {
        tool = m_selectRegionTool;
    } else if (toolId == QLatin1String("tool.picker")) {
        tool = m_pickerTool;
    } else if (toolId == QLatin1String("tool.eraser")) {
        tool = m_eraserTool;
    }
    if (m_activeTool && m_activeTool != tool) {
        // 注意：离开选区工具时【不】清除选区——删除等操作沿用框选范围。
        // 离开镜像工具（参数面板驱动）时清除点选状态，并同步取消面板勾选
        if (m_activeTool == m_mirrorTool) {
            m_mirrorTool->clearState();
            if (m_dockParams) {
                m_dockParams->setMirrorModeOff();
            }
        }
        m_activeTool->deactivate();
    }
    m_activeTool = tool;
    m_scene->setCurrentTool(tool);
    tool->activate(m_view, m_scene);
    // 文字预览只属于文字工具：切走即取消（避免预览挂在其他工具下误确认）
    if (toolId != QLatin1String("tool.text")) {
        clearTextPreview();
    }

    // 底部操作提示：当前工具操作说明
    if (toolId == QLatin1String("tool.select")) {
        showHint(tr("Select: middle-drag pans, wheel zooms"));
    } else if (toolId == QLatin1String("tool.select_region")) {
        showHint(tr("Selection: drag to select; handles resize the range only"));
    } else if (toolId == QLatin1String("tool.stitch")) {
        showHint(tr("Stitch brush: click/drag to paint current color, right-drag to erase"));
    } else if (toolId == QLatin1String("tool.fill")) {
        showHint(tr("Fill: click a connected region to fill current color"));
    } else if (toolId == QLatin1String("tool.rect_fill")) {
        showHint(tr("Rectangle fill: drag a rectangle, right-drag to clear"));
    } else if (toolId == QLatin1String("tool.circle_fill")) {
        showHint(tr("Circle fill: drag a bounding box, circle previews live"));
    } else if (toolId == QLatin1String("tool.ellipse_fill")) {
        showHint(tr("Ellipse fill: drag a bounding box, ellipse previews live"));
    } else if (toolId == QLatin1String("tool.polygon_fill")) {
        showHint(tr("Polygon fill: click vertices, double-click or right-click to close"));
    } else if (toolId == QLatin1String("tool.text")) {
        showHint(tr("Text: click a cell to place a preview, adjust in the Transform panel, "
                    "Confirm writes beads; Esc cancels"));
    } else if (toolId == QLatin1String("tool.highlight")) {
        showHint(tr("Highlight: click a cell to highlight its connected region, right-click or Esc clears"));
    } else if (toolId == QLatin1String("tool.picker")) {
        showHint(tr("Picker: click to pick a color"));
    } else if (toolId == QLatin1String("tool.eraser")) {
        showHint(tr("Eraser: click/drag to erase"));
    }
}

// ------------------------------------------------------------
// 画布编辑（工具 → 内核）
// ------------------------------------------------------------

void MainWindow::onStrokeStarted()
{
    m_inStroke = true;
}

void MainWindow::onStrokeFinished()
{
    // 一笔结束：把整笔作为一条命令栈记录（Ctrl+Z 一次撤销整笔）
    if (!m_inStroke) {
        return;
    }
    m_inStroke = false;
    findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"))->pushHistory(tr("Paint stroke"));
}

void MainWindow::onCellPaintRequested(int x, int y)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || m_activeColorIndex < 0) {
        return;
    }
    // 用当前绣色涂抹（符号索引 = 调色板索引，两者一一对应）
    rust_api::chartPaintCell(project->chartHandle(),
                             static_cast<quint32>(x),
                             static_cast<quint32>(y),
                             static_cast<quint32>(m_activeColorIndex));
    syncCanvasAfterEdit(/*fullRebuild=*/false);
    if (!m_inStroke) {
        project->pushHistory(tr("Paint cell"));
    }
}

void MainWindow::onCellEraseRequested(int x, int y)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    rust_api::chartPaintCell(project->chartHandle(),
                             static_cast<quint32>(x),
                             static_cast<quint32>(y),
                             cm::kColorNone);
    syncCanvasAfterEdit(false);
    if (!m_inStroke) {
        project->pushHistory(tr("Erase cell"));
    }
}

/// 文字 → 豆子格：把一行文字光栅化为格子偏移集合。
/// 两步粗排 + 最近邻重采样（1 像素 = 1 格）：
/// 1) 水平渲染取未旋转字形的 ink 高度 →「格 / 像素」缩放（字高以
///    未旋转字形为准，旋转只改排布不改字高）；
/// 2) 以文字中心旋转 angleDeg（顺时针为正）二次渲染 → ink 包围盒
///    最近邻缩到目标格高。返回旋转后包围盒内每格相对偏移。
static QVector<QPoint> rasterizeTextToCells(const QString& text, int cellHeight,
                                            bool bold, const QFont& fontParam,
                                            int angleDeg)
{
    // 多行文字：按 \n 拆行（输入框支持换行；空行保留占位）
    const QStringList lines =
        text.split(QChar(u'\n'), Qt::KeepEmptyParts);
    if (lines.isEmpty()) {
        return {};
    }
    // 首个非空行作为度量基准（空行无 ink）
    QString metricLine;
    for (const QString& ln : lines) {
        if (!ln.trimmed().isEmpty()) {
            metricLine = ln;
            break;
        }
    }
    if (metricLine.isEmpty()) {
        return {};
    }

    // 大字号渲染（目标格高的 8 倍），缩回目标高度时笔画边缘才不会锯齿化
    const int renderH = qMax(cellHeight * 8, 48);
    QFont font = fontParam;
    font.setPixelSize(renderH);
    if (bold) {
        font.setBold(true);
    }
    const QFontMetrics fm(font);
    const int margin = 4;

    // 1) 水平渲染基准行：未旋转字形的 ink 高度定缩放比（字高 = 每行字高）
    QImage unrot(qMax(2, fm.horizontalAdvance(metricLine) + margin * 2),
                 renderH + margin * 2, QImage::Format_RGB32);
    unrot.fill(Qt::black);
    {
        QPainter painter(&unrot);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QPoint(margin, margin + fm.ascent()), metricLine);
    }
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (int y = 0; y < unrot.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(unrot.constScanLine(y));
        for (int x = 0; x < unrot.width(); ++x) {
            if (qGray(line[x]) > 127) {
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
        }
    }
    if (minX > maxX || minY > maxY) {
        return {};
    }
    const qreal scale = static_cast<qreal>(cellHeight) / (maxY - minY + 1);

    // 2) 旋转渲染：整块文字绕块中心转 angleDeg（顺时针为正）。
    //    行距 = 字体行高；行内左对齐，整体按最宽行居中。
    int advMax = 0;
    for (const QString& ln : lines) {
        advMax = qMax(advMax, fm.horizontalAdvance(ln));
    }
    const int lineStep = fm.height();
    const int blockH = lineStep * lines.size();
    // 画布尺寸按旋转后投影估算 + 大余量（ascent/descent/旋转溢出）
    const double rad = qDegreesToRadians(angleDeg);
    const int wImg = static_cast<int>(qAbs(advMax * qCos(rad))
                                      + qAbs(renderH * 1.6 * qSin(rad))) + margin * 8;
    const int hImg = static_cast<int>(qAbs(advMax * qSin(rad))
                                      + qAbs(renderH * 1.6 * qCos(rad))) + margin * 8;
    QImage img(wImg, hImg, QImage::Format_RGB32);
    img.fill(Qt::black);
    {
        QPainter painter(&img);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.translate(wImg / 2.0, hImg / 2.0);
        painter.rotate(angleDeg);
        for (int i = 0; i < lines.size(); ++i) {
            painter.drawText(QPointF(-advMax / 2.0,
                                     -blockH / 2.0 + i * lineStep + fm.ascent()),
                             lines.at(i));
        }
    }

    // 旋转后 ink 包围盒 → 最近邻重采样
    minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qGray(line[x]) > 127) {
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
        }
    }
    if (minX > maxX || minY > maxY) {
        return {};
    }
    const int inkW = maxX - minX + 1;
    const int inkH = maxY - minY + 1;
    const int targetW = qMax(1, qRound(inkW * scale));
    const int targetH = qMax(1, qRound(inkH * scale));
    QVector<QPoint> cells;
    cells.reserve(targetW * targetH / 4);
    for (int cy = 0; cy < targetH; ++cy) {
        const int sy = qBound(0, static_cast<int>((cy + 0.5) / scale), inkH - 1);
        for (int cx = 0; cx < targetW; ++cx) {
            const int sx = qBound(0, static_cast<int>((cx + 0.5) / scale), inkW - 1);
            if (qGray(img.pixel(minX + sx, minY + sy)) > 127) {
                cells.append(QPoint(cx, cy));
            }
        }
    }
    return cells;
}

/// 文字工具预览：在锚点格重新光栅化并刷新画布预览覆盖层
/// （锚点不变；参数来自变换面板「文字」组）。无文字或全越界 → 清除预览。
void MainWindow::updateTextPreview()
{
    if (!m_textPreviewActive || !m_scene || !m_scene->crossStitchItem()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        clearTextPreview();
        return;
    }
    const DockParams::TextStampParams op = m_dockParams ? m_dockParams->textStampParams()
                                                        : DockParams::TextStampParams{};
    if (op.text.isEmpty()) {
        clearTextPreview();
        return;
    }
    const QVector<QPoint> offsets =
        rasterizeTextToCells(op.text, op.cellHeight, op.bold, op.font, op.angleDeg);
    if (offsets.isEmpty()) {
        clearTextPreview();
        return;
    }

    // 越界格不进预览 / 不涂豆（与真实生成行为一致）
    const QSize gridSize = rust_api::chartGridSize(project->chartHandle());
    QVector<QPoint> absCells;   // 绝对格坐标（确认时涂豆用）
    QVector<QPoint> relCells;   // 相对锚点差（预览绘制用）
    absCells.reserve(offsets.size());
    relCells.reserve(offsets.size());
    for (const QPoint& off : offsets) {
        const int x = m_textAnchor.x() + off.x();
        const int y = m_textAnchor.y() + off.y();
        if (x < 0 || y < 0 || x >= gridSize.width() || y >= gridSize.height()) {
            continue;
        }
        absCells.append(QPoint(x, y));
        relCells.append(off);
    }
    m_textOffsets = absCells;
    if (relCells.isEmpty()) {
        clearTextPreview();
        return;
    }

    // 锚在起始格左上角（覆盖层原点 = 锚点格左上）
    TextPreviewItem* item = m_scene->ensureTextPreview();
    item->setPos(m_scene->crossStitchItem()->cellRectTopLeft(m_textAnchor.x(),
                                                             m_textAnchor.y()));
    item->setPreview(relCells, m_scene->crossStitchItem()->cellSize(),
                     rust_api::chartColorRgb(project->chartHandle(),
                                             m_activeColorIndex));
}

/// 清除文字预览（确认 / Esc / 点图纸外 / 切换工具 / 场景重建时）。
void MainWindow::clearTextPreview()
{
    m_textPreviewActive = false;
    m_textOffsets.clear();
    if (m_scene) {
        m_scene->clearTextPreview();
    }
}

/// 文字工具点击：在点击格放置（或移动）预览。参数在变换面板「文字」组配置；
/// 预览实时刷新，确认后才涂豆（一条撤销记录）。
void MainWindow::onTextAnchorRequested(int cx, int cy)
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    if (m_activeColorIndex < 0) {
        statusBar()->showMessage(tr("Pick or add a colour first"), 3000);
        return;
    }
    m_textAnchor = QPoint(cx, cy);
    m_textPreviewActive = true;
    updateTextPreview();
    statusBar()->showMessage(
        tr("Preview placed: adjust options in the Transform panel, press Confirm to generate"),
        5000);
}

/// 变换面板文字参数变化：预览实时重算（锚点保持在最后点击的格子）。
void MainWindow::onTextParamsChanged()
{
    if (m_textPreviewActive) {
        updateTextPreview();
    }
}

/// 「确认生成」：把预览掩码写入豆子格子（越界格已在预览阶段剔除），
/// 整段一条撤销记录；清预览后材料表 / 画布刷新。
void MainWindow::onTextConfirmRequested()
{
    if (lockedPlateGuard()) {
        return;
    }
    if (!m_textPreviewActive || m_textOffsets.isEmpty()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || m_activeColorIndex < 0) {
        clearTextPreview();
        return;
    }
    void* chart = project->chartHandle();
    for (const QPoint& p : m_textOffsets) {
        rust_api::chartPaintCell(chart,
                                 static_cast<quint32>(p.x()),
                                 static_cast<quint32>(p.y()),
                                 static_cast<quint32>(m_activeColorIndex));
    }
    project->pushHistory(tr("Add text"));
    syncCanvasAfterEdit(false);
    const int placed = m_textOffsets.size();
    clearTextPreview();
    statusBar()->showMessage(
        tr("Text placed: %1 cells").arg(placed), 3000);
}

/// Esc / 点图纸外 / 切走工具：取消预览。
void MainWindow::onTextCancelPreview()
{
    clearTextPreview();
    statusBar()->showMessage(tr("Text preview cancelled"), 2000);
}

/// 「放置」按钮：一键切到文字工具（入口已从工具栏移除），
/// 之后在画布点击格即出预览。
void MainWindow::onTextPlaceRequested()
{
    for (QAction* a : m_toolGroup->actions()) {
        if (a->objectName() == QLatin1String("tool.text")) {
            a->setChecked(true);
            onToolSelected(a);
            statusBar()->showMessage(
                tr("Text tool active: click a cell to preview"), 3000);
            return;
        }
    }
}

void MainWindow::onFillRequested(int x, int y)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || m_activeColorIndex < 0) {
        return;
    }
    const int changed = rust_api::chartFloodFill(project->chartHandle(),
                                                 static_cast<quint32>(x),
                                                 static_cast<quint32>(y),
                                                 static_cast<quint32>(m_activeColorIndex));
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Fill region"));
    statusBar()->showMessage(tr("Filled %1 stitches").arg(changed), 3000);
}

void MainWindow::onFillEraseRequested(int x, int y)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const int changed = rust_api::chartFloodFill(project->chartHandle(),
                                                 static_cast<quint32>(x),
                                                 static_cast<quint32>(y),
                                                 cm::kColorNone);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Clear region"));
}

void MainWindow::onRectFillRequested(int x0, int y0, int x1, int y1)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || m_activeColorIndex < 0) {
        return;
    }
    const int changed = rust_api::chartRectFill(project->chartHandle(),
                                                static_cast<quint32>(x0),
                                                static_cast<quint32>(y0),
                                                static_cast<quint32>(x1),
                                                static_cast<quint32>(y1),
                                                static_cast<quint32>(m_activeColorIndex));
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Fill rectangle"));
    statusBar()->showMessage(tr("Filled %1 stitches").arg(changed), 3000);
}

void MainWindow::onRectEraseRequested(int x0, int y0, int x1, int y1)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const int changed = rust_api::chartRectFill(project->chartHandle(),
                                                static_cast<quint32>(x0),
                                                static_cast<quint32>(y0),
                                                static_cast<quint32>(x1),
                                                static_cast<quint32>(y1),
                                                cm::kColorNone);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Clear rectangle"));
}

namespace {
/// 圆形内部格子（格子中心到圆心距离 ≤ 半径；半径取外接矩形短边一半）。
QVector<QPoint> circleCells(int x0, int y0, int x1, int y1)
{
    QVector<QPoint> cells;
    const qreal cx = (x0 + x1) / 2.0;
    const qreal cy = (y0 + y1) / 2.0;
    const qreal r = qMin((x1 - x0) / 2.0, (y1 - y0) / 2.0);
    const qreal r2 = r * r;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const qreal dx = x + 0.5 - cx;
            const qreal dy = y + 0.5 - cy;
            if (dx * dx + dy * dy <= r2) {
                cells.append(QPoint(x, y));
            }
        }
    }
    return cells;
}

/// 椭圆内部格子（标准方程判定格子中心）。
QVector<QPoint> ellipseCells(int x0, int y0, int x1, int y1)
{
    QVector<QPoint> cells;
    const qreal cx = (x0 + x1) / 2.0;
    const qreal cy = (y0 + y1) / 2.0;
    const qreal rx = qMax(0.5, (x1 - x0) / 2.0);
    const qreal ry = qMax(0.5, (y1 - y0) / 2.0);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const qreal nx = (x + 0.5 - cx) / rx;
            const qreal ny = (y + 0.5 - cy) / ry;
            if (nx * nx + ny * ny <= 1.0) {
                cells.append(QPoint(x, y));
            }
        }
    }
    return cells;
}

/// 射线法：点是否在多边形内部。
bool pointInPolygon(const QPointF& pt, const QVector<QPoint>& poly)
{
    bool inside = false;
    for (int i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const QPointF a(poly.at(i));
        const QPointF b(poly.at(j));
        if ((a.y() > pt.y()) != (b.y() > pt.y())
            && pt.x() < (b.x() - a.x()) * (pt.y() - a.y()) / (b.y() - a.y()) + a.x()) {
            inside = !inside;
        }
    }
    return inside;
}

/// 多边形内部格子（格子中心判定，按 bbox 遍历）。
QVector<QPoint> polygonCells(const QVector<QPoint>& vertices)
{
    QVector<QPoint> cells;
    if (vertices.size() < 3) {
        return cells;
    }
    QRect bbox;
    for (const QPoint& v : vertices) {
        bbox = bbox.united(QRect(v, v));
    }
    for (int y = bbox.top(); y <= bbox.bottom(); ++y) {
        for (int x = bbox.left(); x <= bbox.right(); ++x) {
            if (pointInPolygon(QPointF(x + 0.5, y + 0.5), vertices)) {
                cells.append(QPoint(x, y));
            }
        }
    }
    return cells;
}

/// 批量填格（坐标列表 → 一次 FFI）。
int fillCellList(void* chart, const QVector<QPoint>& cells, quint32 color)
{
    if (!chart || cells.isEmpty()) {
        return -1;
    }
    QVector<quint32> xs;
    QVector<quint32> ys;
    xs.reserve(cells.size());
    ys.reserve(cells.size());
    for (const QPoint& c : cells) {
        xs.append(static_cast<quint32>(c.x()));
        ys.append(static_cast<quint32>(c.y()));
    }
    return rust_api::chartFillCells(chart, xs, ys, color);
}
} // namespace

void MainWindow::onShapeFillRequested(int shape, int x0, int y0, int x1, int y1)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || m_activeColorIndex < 0) {
        return;
    }
    const QVector<QPoint> cells = (shape == static_cast<int>(ShapeFillTool::Circle))
                                      ? circleCells(x0, y0, x1, y1)
                                      : ellipseCells(x0, y0, x1, y1);
    const int changed = fillCellList(project->chartHandle(), cells,
                                     static_cast<quint32>(m_activeColorIndex));
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Fill shape"));
    statusBar()->showMessage(tr("Filled %1 stitches").arg(changed), 3000);
}

void MainWindow::onShapeEraseRequested(int shape, int x0, int y0, int x1, int y1)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const QVector<QPoint> cells = (shape == static_cast<int>(ShapeFillTool::Circle))
                                      ? circleCells(x0, y0, x1, y1)
                                      : ellipseCells(x0, y0, x1, y1);
    const int changed = fillCellList(project->chartHandle(), cells, cm::kColorNone);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Clear shape"));
}

void MainWindow::onPolygonFillRequested(const QVector<QPoint>& vertices)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || m_activeColorIndex < 0) {
        return;
    }
    const QVector<QPoint> cells = polygonCells(vertices);
    const int changed = fillCellList(project->chartHandle(), cells,
                                     static_cast<quint32>(m_activeColorIndex));
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Fill polygon"));
    statusBar()->showMessage(tr("Filled %1 stitches").arg(changed), 3000);
}

void MainWindow::onPolygonEraseRequested(const QVector<QPoint>& vertices)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const QVector<QPoint> cells = polygonCells(vertices);
    const int changed = fillCellList(project->chartHandle(), cells, cm::kColorNone);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Clear polygon"));
}

void MainWindow::onSelectionChanged(int x0, int y0, int x1, int y1, bool active)
{
    m_selectionRect = active ? QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1) : QRect();
    // 镜像基于选区：选区变化时同步镜像蒙层（切到镜像模式时显示）
    if (m_activeTool == m_mirrorTool) {
        m_mirrorTool->setSelection(m_selectionRect);
    }
    const bool en = active && m_activeTool == m_selectRegionTool;
    if (m_deleteSelectionAction) {
        m_deleteSelectionAction->setEnabled(en);
    }
}

void MainWindow::onScaleRegionPreview(int nx0, int ny0, int nx1, int ny1)
{
    // 控制柄拖拽只调整选区范围，不改动画布内容（最近邻重采样预览已移除）
    m_selectionRect = QRect(nx0, ny0, nx1 - nx0 + 1, ny1 - ny0 + 1);
}

void MainWindow::onDeleteSelection()
{
    if (lockedPlateGuard()) {
        return;
    }
    if (m_selectionRect.isEmpty() || m_activeTool != m_selectRegionTool) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    QVector<QPoint> cells;
    for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
        for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
            cells.append(QPoint(x, y));
        }
    }
    const int changed = fillCellList(project->chartHandle(), cells, cm::kColorNone);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Delete selection"));
}

void MainWindow::onMoveSelection(int dx, int dy)
{
    if (lockedPlateGuard()) {
        return;
    }
    if (dx == 0 && dy == 0) {
        return;
    }
    if (m_selectionRect.isEmpty()) {
        statusBar()->showMessage(tr("Select a region first (selection tool)"), 3000);
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    showHint(tr("Move selection: %1 cols, %2 rows").arg(dx).arg(dy));
    const int changed = rust_api::chartMoveRegion(
        project->chartHandle(),
        static_cast<quint32>(m_selectionRect.left()),
        static_cast<quint32>(m_selectionRect.top()),
        static_cast<quint32>(m_selectionRect.right()),
        static_cast<quint32>(m_selectionRect.bottom()),
        dx, dy);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Move selection"));
    // 选区跟随内容平移，方便连续移动 / 再次变换
    m_selectionRect.translate(dx, dy);
    if (m_activeTool == m_selectRegionTool) {
        m_selectRegionTool->setSelection(m_selectionRect);
    } else if (m_activeTool == m_mirrorTool) {
        m_mirrorTool->setSelection(m_selectionRect);
    }
}

void MainWindow::onMirrorAxisRequested(double ax0, double ay0, double ax1, double ay1)
{
    if (lockedPlateGuard()) {
        return;
    }
    if (m_activeTool != m_mirrorTool) {
        return;
    }
    // 自定义镜像（两点）/ 自定义轴（XY）都基于选区
    if (m_selectionRect.isEmpty()) {
        statusBar()->showMessage(tr("Select a region first (selection tool)"), 3000);
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const int changed = rust_api::chartMirrorRegionAxis(
        project->chartHandle(),
        static_cast<quint32>(m_selectionRect.left()),
        static_cast<quint32>(m_selectionRect.top()),
        static_cast<quint32>(m_selectionRect.right()),
        static_cast<quint32>(m_selectionRect.bottom()),
        ax0, ay0, ax1, ay1);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Mirror selection"));
}

void MainWindow::onHighlightRequested(int x, int y)
{
    if (lockedPlateGuard()) {
        return;
    }
    if (m_activeTool != m_highlightTool) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    QVector<quint32> cells;
    const int n = rust_api::chartFloodRegion(project->chartHandle(),
                                             static_cast<quint32>(x),
                                             static_cast<quint32>(y), cells);
    if (n < 0) {
        return;
    }
    QSet<int> region;
    region.reserve(n);
    for (quint32 c : cells) {
        region.insert(static_cast<int>(c));
    }
    if (CrossStitchItem* item = m_scene->crossStitchItem()) {
        item->setHighlight(item->activePlate(), region);
    }
}

void MainWindow::onMirrorWhole(bool horizontal)
{
    if (lockedPlateGuard()) {
        return;
    }
    showHint(horizontal ? tr("Mirror whole plate left-right")
                        : tr("Mirror whole plate up-down"));
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const ChartSnapshot snap = project->chartSnapshot();
    if (snap.gridSize.width() <= 0 || snap.gridSize.height() <= 0) {
        return;
    }
    const int changed = rust_api::chartMirrorRegion(
        project->chartHandle(),
        0u,
        0u,
        static_cast<quint32>(snap.gridSize.width() - 1),
        static_cast<quint32>(snap.gridSize.height() - 1),
        horizontal);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(horizontal ? tr("Mirror whole plate left-right")
                                    : tr("Mirror whole plate up-down"));
}

void MainWindow::onMirrorModeToggled(int mode, bool on)
{
    if (on) {
        m_mirrorTool->setMode(static_cast<MirrorTool::Mode>(mode));
        // 无论 m_activeTool 是否已被记成 mirror，都重新激活镜像工具：
        // activate 内部（ToolBase::activate）先自行清理旧事件过滤器与图元，
        // 保证本次进入时事件过滤器与预览图元一定就绪（修复"再进镜像轴不识别"）。
        if (m_activeTool && m_activeTool != m_mirrorTool) {
            m_activeTool->deactivate();
        }
        m_activeTool = m_mirrorTool;
        m_scene->setCurrentTool(m_mirrorTool);
        m_mirrorTool->activate(m_view, m_scene);
        // 注意：必须先 activate（蒙层 item 在 activate 时创建），再传入选区显示蒙层
        m_mirrorTool->setSelection(m_selectionRect);
        // 互斥：静默点亮本模式按钮、取消另一模式（不发信号，无连锁）
        if (m_dockParams) {
            m_dockParams->setMirrorModeOn(mode == MirrorTool::TwoPoints
                                              ? QStringLiteral("param.mirror_custom")
                                              : QStringLiteral("param.mirror_xy"));
        }
        if (m_selectionRect.isEmpty()) {
            showHint(tr("Mirror needs a selection: drag a box with the selection tool first"));
        } else {
            showHint(mode == MirrorTool::TwoPoints
                         ? tr("Custom mirror: click 2 points as the mirror axis")
                         : tr("Custom axis XY: hover highlights a thin full-canvas grid line, click to mirror"));
        }
    } else {
        if (m_activeTool == m_mirrorTool) {
            m_mirrorTool->deactivate();
            m_activeTool = m_selectTool;
            m_scene->setCurrentTool(m_selectTool);
            m_selectTool->activate(m_view, m_scene);
            for (QAction* a : m_toolGroup->actions()) {
                if (a->data().toString() == QLatin1String("tool.select")) {
                    a->setChecked(true);
                    break;
                }
            }
        }
        showHint(tr("Ready"));
    }
}

void MainWindow::onColorPicked(int x, int y)
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const ChartSnapshot snapshot = project->chartSnapshot();
    const int index = (y * snapshot.gridSize.width()) + x;
    if (index < 0 || index >= snapshot.cells.size()) {
        return;
    }
    const quint32 colorIndex = snapshot.cells.at(index).color_index;
    if (colorIndex == cm::kColorNone) {
        statusBar()->showMessage(tr("Empty cell"), 2000);
        return;
    }
    onColorActivated(static_cast<int>(colorIndex));
}

void MainWindow::onReplaceColorRequested(quint32 from, quint32 to)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const int changed = rust_api::chartReplaceColor(project->chartHandle(), from, to);
    if (changed <= 0) {
        return;
    }
    syncCanvasAfterEdit(false);
    project->pushHistory(tr("Replace colour"));
    statusBar()->showMessage(tr("Replaced %1 stitches").arg(changed), 3000);
}

void MainWindow::onColorMerged(int from, int to)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || from < 0 || to < 0 || from == to) {
        return;
    }
    // 合并语义：from 的针全部改用 to，随后 from 从调色板移除（色块随之消失）
    const int changed = rust_api::chartMergeColor(project->chartHandle(),
                                                  static_cast<quint32>(from),
                                                  static_cast<quint32>(to));
    if (changed < 0) {
        statusBar()->showMessage(tr("Merge failed"), 3000);
        return;
    }
    m_activeColorIndex = -1;
    m_dockColors->setActiveIndex(-1);
    project->pushHistory(tr("Merge color"));
    refreshFromProject();
    {
        const ChartSnapshot snap = project->chartSnapshot();
        QStringList codes;
        for (int i = 0; i < snap.colorCodes.size() && i < 3; ++i) {
            codes << snap.colorCodes.at(i);
        }
        qWarning() << "[CM] merged" << from << "->" << to << "changed" << changed
                   << "legend" << snap.legend.size()
                   << "codes" << codes.join(',');
    }
    statusBar()->showMessage(tr("Merged %1 stitches").arg(changed), 3000);
}

void MainWindow::onColorReplaceWithCard(int from, const QString& code)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || from < 0 || code.isEmpty()) {
        return;
    }
    // 色卡库选的新色：先加入调色板（已存在则复用），再把 from 的针合并过去 = 替换
    const int newIndex = rust_api::chartAddColor(project->chartHandle(), code);
    if (newIndex < 0) {
        statusBar()->showMessage(tr("Color not found: %1").arg(code), 2000);
        return;
    }
    if (newIndex == from) {
        return; // 同色号，无需替换
    }
    const int changed = rust_api::chartMergeColor(project->chartHandle(),
                                                  static_cast<quint32>(from),
                                                  static_cast<quint32>(newIndex));
    if (changed < 0) {
        statusBar()->showMessage(tr("Replace failed"), 3000);
        return;
    }
    m_activeColorIndex = -1;
    m_dockColors->setActiveIndex(-1);
    project->pushHistory(tr("Replace colour"));
    refreshFromProject();
    // 替换后选中新色作为当前画笔色，材料表 / 颜色栏 / 画布三者同步：
    // from 已从调色板移除（merge 语义），新色索引在其之后时会前移一位。
    const int finalIdx = newIndex > from ? newIndex - 1 : newIndex;
    int sel = finalIdx;
    if (m_dockLegend->currentPlate() == -1) {
        sel = -1;
        for (int i = 0; i < m_aggregated.colorCodes.size(); ++i) {
            if (m_aggregated.colorCodes.at(i) == code) {
                sel = i;
                break;
            }
        }
    }
    if (sel >= 0) {
        onColorActivated(sel);
    }
    statusBar()->showMessage(tr("Replaced with %1 (%2 stitches)").arg(code).arg(changed), 3000);
}

void MainWindow::onColorDeleted(int colorIndex)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || colorIndex < 0) {
        return;
    }
    // 删除前确认：该色所有针会被清空
    const QString code = colorIndex < project->chartSnapshot().colorCodes.size()
                             ? project->chartSnapshot().colorCodes.at(colorIndex)
                             : QString::number(colorIndex);
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Delete colour"),
        tr("Delete colour %1? All stitches in this colour become empty.").arg(code),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    const int cleared = rust_api::chartDeleteColor(project->chartHandle(),
                                                   static_cast<quint32>(colorIndex));
    if (cleared < 0) {
        statusBar()->showMessage(tr("Delete failed"), 3000);
        return;
    }
    m_activeColorIndex = -1;
    m_dockColors->setActiveIndex(-1);
    project->pushHistory(tr("Delete colour"));
    refreshFromProject();
    statusBar()->showMessage(tr("Deleted colour %1 (%2 stitches cleared)").arg(code).arg(cleared),
                             3000);
}

void MainWindow::onSymbolChanged(int colorIndex, uint symbol)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid() || colorIndex < 0) {
        return;
    }
    const int swapped = rust_api::chartSetColorSymbol(project->chartHandle(),
                                                      static_cast<quint32>(colorIndex),
                                                      static_cast<char32_t>(symbol));
    if (swapped < -1) {
        statusBar()->showMessage(tr("Symbol change failed"), 3000);
        return;
    }
    project->pushHistory(tr("Change symbol"));
    refreshFromProject();
    if (swapped >= 0) {
        statusBar()->showMessage(tr("Symbol swapped with colour %1").arg(swapped + 1), 3000);
    }
}

void MainWindow::onCenterViewRequested(qreal sceneX, qreal sceneY)
{
    if (!m_view) {
        return;
    }
    m_view->centerOn(sceneX, sceneY);
}

/// 编辑后刷新画布：网格尺寸未变时只更新图元数据，避免重建导致闪烁与滚动位置丢失。
void MainWindow::syncCanvasAfterEdit(bool fullRebuild)
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    const ChartSnapshot snapshot = project->chartSnapshot();
    if (fullRebuild || !m_scene->updatePlate(m_activePlate, snapshot)) {
        // 全量重建：收集全部盘当前模式快照（优先用缓存，避免逐盘重新调 Rust 拷贝）
        QVector<ChartSnapshot> plates;
        plates.reserve(m_crossStitch.plateCount);
        for (int p = 0; p < m_crossStitch.plateCount; ++p) {
            const ModeChart& mc = m_plates[p].charts[m_activeCraftMode];
            plates.append(mc.valid && mc.handle
                              ? (mc.snapshot.valid ? mc.snapshot : snapshotFromHandle(mc.handle))
                              : ChartSnapshot{});
        }
        m_scene->setPlates(plates);
        // 重建后恢复多盘布局（盘数 / 边距 / 当前盘 / 激活盘）
        m_dockLegend->setPlateCount(m_crossStitch.plateCount);
        if (m_scene->crossStitchItem()) {
            m_scene->crossStitchItem()->setPlateLayout(
                m_crossStitch.plateCount, m_crossStitch.plateMarginCells);
            m_scene->crossStitchItem()->setCurrentPlate(m_dockLegend->currentPlate());
            m_scene->crossStitchItem()->setActivePlate(m_activePlate);
            m_scene->crossStitchItem()->setPlateNames(m_crossStitch.plateNames);
        }
    }
    // 缓存激活盘最新快照（删除盘 / 切盘时避免重新调 Rust 拷贝全部盘）
    m_plates[m_activePlate].charts[m_activeCraftMode].snapshot = snapshot;
    // 场景全量重建会烧掉文字预览覆盖层：还有预览活动时按原锚点重画
    if (m_textPreviewActive) {
        updateTextPreview();
    }
    // 总盘模式：材料表显示所有盘汇总；否则显示激活盘
    if (m_dockLegend->currentPlate() == -1) {
        m_aggregated = aggregatedSnapshot();
        m_dockLegend->refresh(m_aggregated);
    } else {
        m_dockLegend->refresh(snapshot);
    }
    m_dockLegend->setCraftMode(m_activeCraftMode);
    updateUndoRedoActions();
}

void MainWindow::onColorActivated(int index)
{
    int realIndex = index;
    // 总盘模式下材料表是跨盘汇总索引；画笔作用于激活盘，需映射回激活盘的调色板索引
    if (m_dockLegend->currentPlate() == -1) {
        realIndex = -1;
        if (index >= 0 && index < m_aggregated.colorCodes.size()) {
            const QString code = m_aggregated.colorCodes.at(index);
            const ModeChart& mc = m_plates[m_activePlate].charts[m_activeCraftMode];
            if (mc.snapshot.valid) {
                for (int i = 0; i < mc.snapshot.colorCodes.size(); ++i) {
                    if (mc.snapshot.colorCodes.at(i) == code) {
                        realIndex = i;
                        break;
                    }
                }
            }
        }
    }
    m_activeColorIndex = realIndex;
    if (realIndex >= 0) {
        statusBar()->showMessage(tr("Active color: %1").arg(realIndex + 1), 2000);
    }
    // 底部颜色栏 ↔ 材料表双向同步：选中色互相高亮（颜色栏选中带描边外框）。
    // 总盘时材料表是跨盘汇总索引，直接按来源 index；其余情况用激活盘 realIndex。
    const int legendIndex = (m_dockLegend->currentPlate() == -1) ? index : realIndex;
    m_dockLegend->selectColorIndex(legendIndex);
    m_dockColors->setActiveIndex(realIndex);
    // 选色后自动切到绣笔，减少一次点击；
    // 但文字预览进行中例外：切走文字工具会取消预览，此刻应让预览
    // 实时跟随新选中的豆子颜色（预览颜色 = 当前激活颜色）。
    if (m_textPreviewActive) {
        updateTextPreview();
        return;
    }
    for (QAction* a : m_toolGroup->actions()) {
        if (a->objectName() == QLatin1String("tool.stitch") && !a->isChecked()) {
            a->trigger();
            break;
        }
    }
}

void MainWindow::onAddColorFromCard(const QString& code)
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const int index = rust_api::chartAddColor(project->chartHandle(), code);
    qWarning() << "[CM] addColor" << code << "->" << index
               << "projValid" << project->isValid();
    if (index < 0) {
        statusBar()->showMessage(tr("Color not found: %1").arg(code), 2000);
        return;
    }
    refreshFromProject();
    // 新加的颜色要同时成为当前画笔颜色：材料表选中 / 底部颜色栏高亮 / 画布画笔
    // 三者必须一致。旧实现只让材料表选中新色，颜色栏焦点还停在旧色，
    // 画布实际涂的还是旧色。onColorActivated 内部统一完成：
    // 总盘汇总索引 → 激活盘调色板索引映射 + m_activeColorIndex + 颜色栏同步。
    int sel = index;
    if (m_dockLegend->currentPlate() == -1) {
        sel = -1;
        for (int i = 0; i < m_aggregated.colorCodes.size(); ++i) {
            if (m_aggregated.colorCodes.at(i) == code) {
                sel = i;
                break;
            }
        }
    }
    if (sel >= 0) {
        onColorActivated(sel);
    }
    statusBar()->showMessage(tr("Added color %1").arg(code), 2000);
}

void MainWindow::onPruneUnusedColors()
{
    if (lockedPlateGuard()) {
        return;
    }
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (!project->isValid()) {
        return;
    }
    const int removed = rust_api::chartPruneUnused(project->chartHandle());
    if (removed <= 0) {
        statusBar()->showMessage(tr("No unused colors to clean"), 2500);
        return;
    }
    m_activeColorIndex = -1;
    m_dockColors->setActiveIndex(-1);
    project->pushHistory(tr("Clean unused colors"));
    refreshFromProject();
    statusBar()->showMessage(tr("Removed %1 unused colors").arg(removed), 3000);
}

void MainWindow::onLanguageChanged(const QString& language)
{
    Q_UNUSED(language)
    retranslateUi();
    // 子界面重翻译：Dock 内静态文本由各自 refresh 重建，
    // 菜单 / 工具栏 / 窗口标题已在上方更新。
}

void MainWindow::onSwitchLanguage(const QString& language)
{
    TrManager::instance().switchLanguage(language);
}

/// 三种模式图纸生成完成：从属性面板接管三份图纸到目标盘，并激活该盘当前模式。
/// target >= 0 = 生成到该盘；target = -1 = 新增一盘并生成到新盘。
void MainWindow::onChartsGenerated(int targetPlate)
{
    if (!m_dockProperty->hasModeCharts()) {
        return;
    }
    DockProperty::ModeChartsResult result = m_dockProperty->takeModeCharts();
    if (targetPlate < 0) {
        // 新盘：盘数 +1（上限 25）后落到最后一块盘
        if (m_crossStitch.plateCount >= kMaxPlateCount) {
            for (void* raw : result.charts) {
                if (raw) {
                    cm_chart_free(raw);
                }
            }
            statusBar()->showMessage(tr("Maximum %1 plates").arg(kMaxPlateCount), 2000);
            return;
        }
        m_crossStitch.plateCount += 1;
        m_crossStitch.save();
        targetPlate = m_crossStitch.plateCount - 1;
        m_dockLegend->setPlateCount(m_crossStitch.plateCount);
        if (m_scene->crossStitchItem()) {
            m_scene->crossStitchItem()->setPlateLayout(
                m_crossStitch.plateCount, m_crossStitch.plateMarginCells);
        }
    }
    if (targetPlate < 0 || targetPlate >= static_cast<int>(m_plates.size())) {
        targetPlate = 0;
    }
    // 覆盖该盘旧的三份结果
    for (size_t mode = 0; mode < 3; ++mode) {
        ModeChart& mc = m_plates[targetPlate].charts[mode];
        if (mc.handle && mc.ownsHandle) {
            cm_chart_free(mc.handle);
        }
        mc = ModeChart{};
        mc.params = result.params[mode];
        mc.options = result.options[mode];
        mc.handle = result.charts[mode];
        mc.valid = result.charts[mode] != nullptr;
        // 快照在激活时实时生成（refreshFromProject），无需预存
        if (mc.valid) {
            mc.snapshot = snapshotFromHandle(mc.handle);
        }
    }
    // 激活目标盘并显示当前模式图纸
    m_activePlate = targetPlate;
    m_dockProperty->setCurrentPlateHint(m_activePlate);
    m_dockLegend->setCurrentPlateOption(m_activePlate);
    setActiveCraftMode(m_activeCraftMode);
    statusBar()->showMessage(
        tr("Chart generated: cross stitch / bead embroidery / perler beads"), 3000);
}

/// 释放全部盘内由本窗口持有的句柄（工程打开的引用句柄不释放）。
void MainWindow::clearPlates()
{
    for (auto& plate : m_plates) {
        for (auto& mc : plate.charts) {
            if (mc.handle && mc.ownsHandle) {
                cm_chart_free(mc.handle);
            }
            mc = ModeChart{};
        }
    }
}

/// 由 Rust 句柄构建快照（供多盘渲染与材料表使用）。
ChartSnapshot MainWindow::snapshotFromHandle(void* handle)
{
    ChartSnapshot snapshot;
    if (!handle) {
        return snapshot;
    }
    snapshot.valid = true;
    snapshot.gridSize = rust_api::chartGridSize(handle);
    snapshot.cells = rust_api::chartCells(handle);
    snapshot.stitchTotal = rust_api::chartStitchTotal(handle);
    rust_api::chartParams(handle, &snapshot.params, &snapshot.options);
    snapshot.fabricSize = rust_api::chartFabricSize(handle);

    const QVector<CmLegendEntry> legend = rust_api::chartLegend(handle);
    snapshot.legend = legend;
    snapshot.palette.reserve(legend.size());
    snapshot.colorCodes.reserve(legend.size());
    snapshot.colorNames.reserve(legend.size());
    for (int i = 0; i < legend.size(); ++i) {
        snapshot.palette.push_back(rust_api::chartColorRgb(handle, i));
        snapshot.colorCodes.push_back(rust_api::chartColorCode(handle, i));
        snapshot.colorNames.push_back(rust_api::chartColorName(handle, i));
    }
    snapshot.symbols = rust_api::chartSymbols(handle);
    snapshot.symbolAlphabet.reserve(64);
    for (int i = 0; i < 64; ++i) {
        const QChar c = rust_api::symbolInSet(snapshot.options.symbol_set, i);
        if (c.isNull() || snapshot.symbolAlphabet.contains(c)) {
            break;
        }
        snapshot.symbolAlphabet.push_back(c);
    }
    return snapshot;
}

/// 切换手作品类：同步材料表与色卡面板，并把激活盘对应模式的图纸置为当前可编辑图纸。
void MainWindow::setActiveCraftMode(uint32_t mode)
{
    if (mode > 2) {
        mode = 0;
    }
    m_activeCraftMode = mode;
    m_dockLegend->setCraftMode(mode);
    m_dockMaterial->setCraftMode(mode);

    ModeChart& mc = m_plates[m_activePlate].charts[mode];
    qWarning() << "[CM] craftMode" << mode << "plate" << m_activePlate
               << "valid" << mc.valid << "handle" << (mc.handle != nullptr)
               << "snapLegend" << mc.snapshot.legend.size()
               << "projValid" << (m_project ? m_project->isValid() : false);
    if (!mc.valid || !mc.handle) {
        // 空盘：创建空图句柄（默认 100×100），允许自由添加颜色并直接编辑。
        // bead_mode 必须与手作品类一致（与图像生成路径 dock_property 同规则）：
        // 珠绣/拼豆为 1，内核 legend() 才会累计 bead_count —— 否则空盘涂豆后
        // 材料表豆数永远为 0（只在"从图片生成"的图纸上有数据）。
        CmCrossStitchParams p = rust_api::defaultCrossStitchParams();
        CmStitchOptions o = rust_api::defaultStitchOptions();
        o.craft_mode = mode;
        p.bead_mode = mode == cm::craft::CROSS_STITCH ? 0u : 1u;
        void* h = cm_cross_stitch_empty(p.grid_width, p.grid_height, &p, &o);
        if (h) {
            mc = ModeChart{};
            mc.params = p;
            mc.options = o;
            mc.handle = h;
            mc.valid = true;
            mc.ownsHandle = true;
            mc.snapshot = snapshotFromHandle(h);
            void* raw = mc.handle;
            rust_api::ChartHandle borrowed(raw, [](void*) {});
            m_project->adoptChart(std::move(borrowed), QString());
            m_project->resetHistory();
        } else {
            m_project->clearChart();
        }
        refreshFromProject();
        return;
    }

    // 借给 ProjectDoc 一个非持有句柄：所有权仍在 m_plates 里
    void* raw = mc.handle;
    rust_api::ChartHandle borrowed(raw, [](void*) {});
    m_project->adoptChart(std::move(borrowed), QString());
    m_project->resetHistory();
    refreshFromProject();
}

void MainWindow::onPlateSelected(int plate)
{
    // 选中具体盘：设为激活盘并刷新（材料表显示该盘材料、编辑作用于该盘）
    if (plate >= 0 && plate < m_crossStitch.plateCount) {
        showHint(tr("Active plate: %1").arg(plate + 1));
        m_activePlate = plate;
        m_dockProperty->setCurrentPlateHint(m_activePlate);
    }
    if (m_scene->crossStitchItem()) {
        m_scene->crossStitchItem()->setCurrentPlate(plate); // -1 总盘全亮 / N 该盘高亮
        m_scene->crossStitchItem()->setActivePlate(m_activePlate);
    }
    setActiveCraftMode(m_activeCraftMode); // 借目标盘当前模式句柄并刷新
    fitView();
}

void MainWindow::addPlateSlot()
{
    showHint(tr("Add plate: up to %1 plates, 5 per row").arg(kMaxPlateCount));
    if (m_crossStitch.plateCount >= kMaxPlateCount) {
        statusBar()->showMessage(tr("Maximum %1 plates").arg(kMaxPlateCount), 2000);
        return;
    }
    const int newCount = m_crossStitch.plateCount + 1;
    m_crossStitch.plateCount = newCount;
    m_crossStitch.save();
    m_dockLegend->setPlateCount(newCount);
    if (m_scene->crossStitchItem()) {
        m_scene->crossStitchItem()->setPlateLayout(
            m_crossStitch.plateCount, m_crossStitch.plateMarginCells);
    }
    // 选中新盘（空盘，可继续用"生成到当前盘"填充）
    m_dockLegend->setCurrentPlateOption(newCount - 1);
    statusBar()->showMessage(tr("Added plate %1").arg(newCount), 2000);
}

void MainWindow::removePlate(int plate)
{
    showHint(tr("Remove plate %1").arg(plate + 1));
    if (m_crossStitch.plateCount <= 1) {
        statusBar()->showMessage(tr("Keep at least one plate"), 2000);
        return;
    }
    if (plate < 0 || plate >= m_crossStitch.plateCount) {
        return;
    }
    // 该盘任一模式有图案 → 弹确认，避免误删
    bool hasPattern = false;
    for (const auto& mc : m_plates[plate].charts) {
        if (mc.valid && mc.handle) {
            hasPattern = true;
            break;
        }
    }
    if (hasPattern) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Remove plate"),
            tr("Plate %1 contains a pattern. Remove it anyway?").arg(plate + 1),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    const bool deletingActive = (plate == m_activePlate);
    // 释放该盘三模式句柄
    for (auto& mc : m_plates[plate].charts) {
        if (mc.handle && mc.ownsHandle) {
            cm_chart_free(mc.handle);
        }
        mc = ModeChart{};
    }
    // 后续盘前移（move 语义避免拷贝大快照；定长数组末尾清空）
    for (int p = plate; p + 1 < kMaxPlateCount; ++p) {
        m_plates[p] = std::move(m_plates[p + 1]);
    }
    m_plates[kMaxPlateCount - 1] = PlateCharts{};
    // 盘数与盘名同步
    m_crossStitch.plateCount -= 1;
    if (plate < m_crossStitch.plateNames.size()) {
        m_crossStitch.plateNames.removeAt(plate);
    }
    m_crossStitch.save();
    // 激活盘修正
    if (m_activePlate > plate) {
        m_activePlate -= 1;
    } else if (m_activePlate >= m_crossStitch.plateCount) {
        m_activePlate = m_crossStitch.plateCount - 1;
    }
    // 画布增量删除（只移除该盘并重排，不重建其他盘 → 实时无卡顿）
    if (m_scene->crossStitchItem()) {
        m_scene->crossStitchItem()->removePlate(plate);
        m_scene->crossStitchItem()->setActivePlate(m_activePlate);
        m_scene->crossStitchItem()->setCurrentPlate(m_dockLegend->currentPlate());
    }
    m_dockLegend->setPlateCount(m_crossStitch.plateCount);
    m_dockProperty->setCurrentPlateHint(m_activePlate);
    if (deletingActive) {
        // 删的是激活盘：接管新激活盘图纸并刷新面板（只此路径全量刷新）
        m_dockLegend->setCurrentPlateOption(m_activePlate, false);
        setActiveCraftMode(m_activeCraftMode);
    } else {
        // 非激活盘：仅静默更新下拉显示，画布已即时删除
        m_dockLegend->setCurrentPlateOption(m_activePlate, false);
    }
    statusBar()->showMessage(tr("Plate removed"), 2000);
}

void MainWindow::resizePlate(int plate, int gridW, int gridH, int anchorLeft, int anchorTop)
{
    if (lockedPlateGuard()) {
        return;
    }
    showHint(tr("Resize plate grid: only crop/extend cells, pattern unchanged"));
    if (plate < 0 || plate >= m_crossStitch.plateCount) {
        return;
    }
    const int gw = qBound(1, gridW, 9999);
    const int gh = qBound(1, gridH, 9999);
    // 该盘三模式统一调整画布格子：裁剪/扩展，图案内容不变（不缩放、不重新生成）
    bool any = false;
    for (size_t mode = 0; mode < 3; ++mode) {
        ModeChart& mc = m_plates[plate].charts[mode];
        if (!mc.valid || !mc.handle) {
            continue;
        }
        if (cm_chart_resize(mc.handle, static_cast<uint32_t>(gw), static_cast<uint32_t>(gh),
                            static_cast<uint32_t>(anchorLeft), static_cast<uint32_t>(anchorTop))
            == cm::codes::OK) {
            mc.snapshot = snapshotFromHandle(mc.handle);
            any = true;
        }
    }
    if (!any) {
        statusBar()->showMessage(tr("Import an image first"), 2000);
        return;
    }
    // 画布增量更新该盘（只重绘这一块，不重建其他盘）
    if (m_scene) {
        const ModeChart& mc = m_plates[plate].charts[m_activeCraftMode];
        m_scene->updatePlate(plate, mc.snapshot);
    }
    // 改的正是激活盘时同步材料表 / 色卡 / 属性面板
    if (plate == m_activePlate) {
        const ModeChart& mc = m_plates[plate].charts[m_activeCraftMode];
        if (m_dockLegend->currentPlate() == -1) {
            m_aggregated = aggregatedSnapshot();
            m_dockLegend->refresh(m_aggregated);
        } else {
            m_dockLegend->refresh(mc.snapshot);
        }
        m_dockMaterial->refreshPalette(mc.snapshot);
        m_dockColors->refresh(mc.snapshot);
        if (m_dockProperty) {
            m_dockProperty->syncFromProject(mc.params, mc.options,
                                            mc.snapshot.fabricSize, mc.snapshot.valid);
        }
    }
    statusBar()->showMessage(
        tr("Plate %1 resized to %2\u00d7%3").arg(plate + 1).arg(gw).arg(gh), 2000);
}

void MainWindow::refreshFromProject()
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    const ChartSnapshot snapshot = project->chartSnapshot();
    qWarning() << "[CM] refresh plate" << m_activePlate << "mode" << m_activeCraftMode
               << "legend" << snapshot.legend.size()
               << "codes0"
               << (snapshot.colorCodes.size() > 0 ? snapshot.colorCodes.at(0)
                                                  : QStringLiteral("<none>"))
               << "idx0" << (snapshot.legend.size() > 0
                                 ? static_cast<qint64>(snapshot.legend.at(0).color_index)
                                 : -1);
    // 写回激活盘缓存：合并/删色/加色/改符号等操作走本函数，
    // 不写缓存会导致总盘汇总读到旧材料
    if (m_activePlate >= 0 && m_activePlate < m_crossStitch.plateCount) {
        m_plates[m_activePlate].charts[m_activeCraftMode].snapshot = snapshot;
    }
    // 多盘渲染：收集全部盘当前模式的快照（一盘一图）
    QVector<ChartSnapshot> plates;
    plates.reserve(m_crossStitch.plateCount);
    for (int p = 0; p < m_crossStitch.plateCount; ++p) {
        const ModeChart& mc = m_plates[p].charts[m_activeCraftMode];
        plates.append(mc.valid && mc.handle
                          ? (mc.snapshot.valid ? mc.snapshot : snapshotFromHandle(mc.handle))
                          : ChartSnapshot{});
    }
    m_scene->setPlates(plates);
    // 多盘布局：盘数 / 盘边距来自十字绣设置，当前盘与激活盘来自材料表下拉
    m_dockLegend->setPlateCount(m_crossStitch.plateCount);
    if (m_scene->crossStitchItem()) {
        m_scene->crossStitchItem()->setPlateLayout(
            m_crossStitch.plateCount, m_crossStitch.plateMarginCells);
        m_scene->crossStitchItem()->setCurrentPlate(m_dockLegend->currentPlate());
        m_scene->crossStitchItem()->setActivePlate(m_activePlate);
        // 盘名（双击改名后回写设置）
        m_scene->crossStitchItem()->setPlateNames(m_crossStitch.plateNames);
        // 盘锁定状态（标题栏锁按钮显示 + 编辑拦截）
        m_scene->crossStitchItem()->setPlateLocks(m_crossStitch.plateLocked);
        connect(m_scene->crossStitchItem(), &CrossStitchItem::plateLockToggled,
                this, &MainWindow::onPlateLockToggled, Qt::UniqueConnection);
        // 盘名（双击改名后回写设置）；lambda+UniqueConnection 组合 Qt6 不支持
        // （静默失败），改为先断旧连接再连新 lambda（面板复用/重建均不重复）
        disconnect(m_scene->crossStitchItem(), &CrossStitchItem::plateRenamed,
                   this, nullptr);
        connect(m_scene->crossStitchItem(), &CrossStitchItem::plateRenamed,
                this, [this](int plate, const QString& name) {
                    while (m_crossStitch.plateNames.size() < kMaxPlateCount) {
                        m_crossStitch.plateNames.append(QString());
                    }
                    if (plate >= 0 && plate < m_crossStitch.plateNames.size()) {
                        m_crossStitch.plateNames[plate] = name;
                        m_crossStitch.save();
                    }
                });
        connect(m_scene->crossStitchItem(), &CrossStitchItem::plateRemoveRequested,
                this, &MainWindow::removePlate, Qt::UniqueConnection);
        connect(m_scene->crossStitchItem(), &CrossStitchItem::plateFitRequested,
                this, &MainWindow::fitPlate, Qt::UniqueConnection);
        connect(m_scene->crossStitchItem(), &CrossStitchItem::plateGridResized,
                this, &MainWindow::resizePlate, Qt::UniqueConnection);
        // 与底部颜色栏"色号"按钮同步：拼豆/色块视图格子文本跟随标签模式
        m_scene->crossStitchItem()->setLabelMode(DockColors::labelMode());
    }
    // 场景重建会销毁文字预览覆盖层：预览活动时按原锚点重画
    if (m_textPreviewActive) {
        updateTextPreview();
    }
    // 总盘模式：材料表显示所有盘汇总；否则显示激活盘
    if (m_dockLegend->currentPlate() == -1) {
        m_aggregated = aggregatedSnapshot();
        m_dockLegend->refresh(m_aggregated);
    } else {
        m_dockLegend->refresh(snapshot);
    }
    // 面板单选框与当前模式保持一致（模式状态由 MainWindow 维护）
    m_dockLegend->setCraftMode(m_activeCraftMode);
    m_dockMaterial->setCraftMode(m_activeCraftMode);
    m_dockMaterial->refreshPalette(snapshot);
    m_dockColors->setCraftMode(m_activeCraftMode);
    m_dockColors->refresh(snapshot);
    if (m_dockProperty) {
        m_dockProperty->syncFromProject(project->params(), project->options(),
                                        snapshot.fabricSize, snapshot.valid);
    }
    updateUndoRedoActions();
}

void MainWindow::updateUndoRedoActions()
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    m_undoAction->setEnabled(project->canUndo());
    m_redoAction->setEnabled(project->canRedo());
}

void MainWindow::fitView()
{
    if (!m_scene) {
        return;
    }
    const QRectF bounds = m_scene->itemsBoundingRect();
    if (bounds.isNull()) {
        return;
    }
    m_view->fitInView(bounds.adjusted(-10, -10, 10, 10), Qt::KeepAspectRatio);
}

void MainWindow::fitActivePlate()
{
    showHint(tr("Fit active plate to window"));
    fitPlate(m_activePlate);
}

void MainWindow::fitPlate(int plate)
{
    showHint(tr("Fit plate to window"));
    if (!m_view || !m_scene || !m_scene->crossStitchItem()) {
        return;
    }
    if (plate >= 0 && plate < m_crossStitch.plateCount) {
        const QRectF pr = m_scene->crossStitchItem()->plateRectWithTitleScene(plate);
        if (!pr.isNull()) {
            m_view->fitInView(pr.adjusted(-6, -6, 6, 6), Qt::KeepAspectRatio);
            return;
        }
    }
    fitView();
}

ChartSnapshot MainWindow::aggregatedSnapshot() const
{
    // 总盘 = 所有盘当前模式的材料按色号合并：同色针数 / 珠数 / 用线相加，
    // 调色板按色号去重，符号取最先出现的盘。
    ChartSnapshot out;
    QMap<QString, int> codeMap;
    quint32 total = 0;
    for (int p = 0; p < m_crossStitch.plateCount; ++p) {
        const ModeChart& mc = m_plates[p].charts[m_activeCraftMode];
        const ChartSnapshot& s = mc.snapshot;
        if (!s.valid || s.legend.isEmpty()) {
            continue;
        }
        // 尺寸 / 符号集等取自第一个有效盘（材料汇总以统计为主；
        // cells 一并带上，材料表以 cells 非空判断“有图纸”）
        if (out.gridSize.isEmpty()) {
            out.gridSize = s.gridSize;
            out.cells = s.cells;
            out.symbolAlphabet = s.symbolAlphabet;
            out.params = s.params;
            out.options = s.options;
            out.fabricSize = s.fabricSize;
        }
        for (const CmLegendEntry& e : s.legend) {
            if (e.color_index >= s.colorCodes.size()) {
                continue;
            }
            const QString code = s.colorCodes.at(e.color_index);
            const auto it = codeMap.constFind(code);
            if (it != codeMap.constEnd()) {
                CmLegendEntry& agg = out.legend[*it];
                agg.stitch_count += e.stitch_count;
                agg.bead_count += e.bead_count;
                agg.length_cm_x100 += e.length_cm_x100;
            } else {
                const int idx = out.palette.size();
                codeMap.insert(code, idx);
                out.palette.append(e.color_index < s.palette.size()
                                       ? s.palette.at(e.color_index)
                                       : QColor());
                out.colorCodes.append(code);
                out.colorNames.append(e.color_index < s.colorNames.size()
                                          ? s.colorNames.at(e.color_index)
                                          : QString());
                out.symbols.append(e.symbol_index < s.symbols.size()
                                       ? s.symbols.at(e.symbol_index)
                                       : QChar('?'));
                CmLegendEntry ne = e;
                ne.color_index = static_cast<quint32>(idx);
                ne.symbol_index = static_cast<quint32>(out.symbols.size() - 1);
                out.legend.append(ne);
            }
        }
        total += s.stitchTotal;
    }
    out.stitchTotal = total;
    out.valid = !out.legend.isEmpty();
    return out;
}

void MainWindow::checkAutosaveRecovery()
{
    const QString autosave = ProjectDoc::findAutosave();
    if (autosave.isEmpty()) {
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Recover unsaved work?"),
        tr("An autosaved project was found from a previous session. Recover it?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
        if (project->openAutosave(autosave)) {
            // 恢复成功：登记到激活盘（所有权归 ProjectDoc），与打开工程路径一致
            const uint32_t projMode =
                static_cast<uint32_t>(project->options().craft_mode);
            m_activeCraftMode = projMode > 2 ? 0 : projMode;
            clearPlates();
            m_activePlate = 0;
            const void* h = project->chartHandle();
            if (h) {
                for (int mode = 0; mode < 3; ++mode) {
                    ModeChart& mc = m_plates[0].charts[mode];
                    mc = ModeChart{};
                    mc.valid = (mode == static_cast<int>(m_activeCraftMode));
                    mc.handle = const_cast<void*>(h);
                    mc.ownsHandle = false; // 工程图由 ProjectDoc 持有
                    if (mc.valid) {
                        mc.snapshot = snapshotFromHandle(mc.handle);
                    }
                }
            }
            m_dockLegend->setCraftMode(m_activeCraftMode);
            m_dockMaterial->setCraftMode(m_activeCraftMode);
            statusBar()->showMessage(tr("Recovered from autosave"), 3000);
            return;
        }
    }
    ProjectDoc::clearAutosave();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    auto* project = findChild<ProjectDoc*>(QStringLiteral("ProjectDoc"));
    if (project->isModified()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Unsaved changes"),
            tr("The current project has unsaved changes. Save before quitting?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (answer == QMessageBox::Save) {
            onSaveProject();
            if (project->isModified()) {
                event->ignore();
                return;
            }
        } else if (answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
#ifdef Q_OS_WINDOWS
    if (m_pendingFirstFit) {
        // Windows 11 圆角窗口
        const HWND hwnd = reinterpret_cast<HWND>(winId());
        const DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    }
#endif
    // 停靠布局稳定后再适配一次视图：构造期视口尺寸未定，立即 fitView 会偏小
    if (m_pendingFirstFit) {
        m_pendingFirstFit = false;
        QTimer::singleShot(100, this, &MainWindow::fitView);
    }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // 标题栏空白区域：左键拖动移动窗口，双击切换最大化
    if (obj == m_titleBar) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* e = static_cast<QMouseEvent*>(event);
            if (e->button() == Qt::LeftButton && windowHandle()) {
                windowHandle()->startSystemMove();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto* e = static_cast<QMouseEvent*>(event);
            if (e->button() == Qt::LeftButton) {
                toggleMaximize();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WINDOWS
    if (eventType == "windows_generic_MSG" && message) {
        auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST && !m_customMaximized) {
            // 边缘热区 → 原生调整窗口大小（CAD/Studio 式八向拉伸）
            POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            ScreenToClient(msg->hwnd, &pt);
            const qreal dpr = devicePixelRatioF();
            const qreal x = pt.x / dpr;
            const qreal y = pt.y / dpr;
            const qreal w = width();
            const qreal h = height();
            constexpr qreal kEdge = 6.0;
            LRESULT hit = HTCLIENT;
            if (y <= kEdge && x <= kEdge) hit = HTTOPLEFT;
            else if (y <= kEdge && x >= w - kEdge) hit = HTTOPRIGHT;
            else if (y >= h - kEdge && x <= kEdge) hit = HTBOTTOMLEFT;
            else if (y >= h - kEdge && x >= w - kEdge) hit = HTBOTTOMRIGHT;
            else if (x <= kEdge) hit = HTLEFT;
            else if (x >= w - kEdge) hit = HTRIGHT;
            else if (y <= kEdge) hit = HTTOP;
            else if (y >= h - kEdge) hit = HTBOTTOM;
            if (hit != HTCLIENT) {
                *result = hit;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::toggleMaximize()
{
    if (m_customMaximized) {
        m_customMaximized = false;
        setGeometry(m_normalGeometry);
        m_maximizeButton->setText(QChar(ushort(0xE922)));
    } else {
        m_normalGeometry = geometry();
        m_customMaximized = true;
        if (QScreen* screen = this->screen()) {
            setGeometry(screen->availableGeometry());
        }
        m_maximizeButton->setText(QChar(ushort(0xE923)));
    }
}

void MainWindow::onToggleTheme()
{
    // 深色主题已移除：快捷键在浅色 / 自定义间切换
    m_theme.mode = (m_theme.mode == 0) ? 2 : 0;
    applyTheme();
    saveThemeConfig();
}

void MainWindow::applyTheme()
{
    // 深色主题已移除：统一使用浅色基础样式
    const QString path = QStringLiteral(":/qss/beandraw.qss");
    QFile file(path);
    QString qss;
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qss = QString::fromUtf8(file.readAll());
    }
    // 当前选中主题的配色覆盖规则（QSS 级联，后规则优先）；
    // 浅色 / 深色 / 自定义三套配色均可在设置中修改
    const PaletteSet& palette = m_theme.active();
    const QColor accent = palette.accent;
    // 高亮色在运行时注入 QSS 占位符 %ACCENT%（单选/复选选中、悬停等强调色）
    qss.replace(QStringLiteral("%ACCENT%"), accent.name());
    qss += QStringLiteral("\n/* 调色板覆盖 */\n");
    qss += QStringLiteral("QMainWindow, QDialog { background-color: %1; }\n")
               .arg(palette.appBackground.name());
    qss += QStringLiteral("QWidget#TitleBar { background-color: %1; }\n")
               .arg(palette.menuBar.name());
    qss += QStringLiteral("QMenuBar { background-color: %1; }\n")
               .arg(palette.menuBar.name());
    qss += QStringLiteral("QToolBar { background-color: %1; }\n")
               .arg(palette.toolBar.name());
    qss += QStringLiteral("QDockWidget::title { background-color: %1; }\n")
               .arg(palette.dockTitle.name());
    qss += QStringLiteral("ads--CDockAreaTitleBar { background-color: %1; }\n")
               .arg(palette.dockTitle.name());
    qss += QStringLiteral("ads--CDockWidgetTab:checked { background-color: %1; }\n")
               .arg(palette.dockTitle.name());
    qss += QStringLiteral("QGraphicsView { background-color: %1; border: none; }\n")
               .arg(palette.canvasBackground.name());
    qApp->setStyleSheet(qss);

    // 主题高亮色同步到应用调色板 Highlight：
    // 对话框按钮选中、颜色栏色块外框等从 QPalette::Highlight 取色，统一用主题色。
    QPalette appPalette = qApp->palette();
    appPalette.setColor(QPalette::Highlight, accent);
    qApp->setPalette(appPalette);

    // 材料表选中高亮 / 外框统一用主题高亮色
    if (m_dockLegend) {
        m_dockLegend->setAccentColor(accent);
    }

    // 矢量图标按新的窗口前景色重绘，保证深浅主题下都清晰
    refreshToolIcons();
}

void MainWindow::loadThemeConfig()
{
    QSettings settings(QStringLiteral("BeanDraw"), QStringLiteral("BeanDraw"));
    m_theme.startupMode = settings.value(QStringLiteral("theme/startupMode"), 2).toInt();
    if (m_theme.startupMode > 2) {
        m_theme.startupMode = 2;
    }
    if (settings.contains(QStringLiteral("theme/mode"))) {
        m_theme.mode = settings.value(QStringLiteral("theme/mode"), 0).toInt();
        m_theme.darkBase = settings.value(QStringLiteral("theme/darkBase"), false).toBool();
    } else {
        // 旧版本配置迁移（单套颜色 → 归入自定义或浅色）
        const bool legacyCustom = settings.value(QStringLiteral("theme/customColors"), false).toBool();
        const bool legacyDark = settings.value(QStringLiteral("theme/dark"), false).toBool();
        m_theme.mode = legacyCustom ? 2 : 0; // 深色已废弃，旧配置落回浅色
        m_theme.darkBase = legacyDark;
    }
    // 历史 mode==1（深色）复位为浅色
    if (m_theme.mode == 1) {
        m_theme.mode = 0;
    }
    struct Role { const char* key; QColor PaletteSet::* member; };
    const Role roles[] = {
        {"appBackground", &PaletteSet::appBackground},
        {"dockTitle", &PaletteSet::dockTitle},
        {"canvasBackground", &PaletteSet::canvasBackground},
        {"menuBar", &PaletteSet::menuBar},
        {"toolBar", &PaletteSet::toolBar},
        {"accent", &PaletteSet::accent},
    };
    const char* groups[] = {"theme/light/", "theme/dark/", "theme/custom/"};
    for (int col = 0; col < 3; ++col) {
        const PaletteSet defSet = PaletteSet::defaults(col == 1);
        for (const Role& role : roles) {
            m_theme.palettes[col].*role.member =
                settings.value(QStringLiteral("%1%2").arg(groups[col], role.key),
                               defSet.*role.member).value<QColor>();
        }
    }
    // 旧默认色升级：浅色菜单栏由 #d9d9d9 提亮为 #f5f5f5
    if (m_theme.palettes[0].menuBar == QColor(0xd9d9d9)) {
        m_theme.palettes[0].menuBar = QColor(0xf5f5f5);
    }
}

void MainWindow::saveThemeConfig()
{
    QSettings settings(QStringLiteral("BeanDraw"), QStringLiteral("BeanDraw"));
    settings.setValue(QStringLiteral("theme/mode"), m_theme.mode);
    settings.setValue(QStringLiteral("theme/darkBase"), m_theme.darkBase);
    settings.setValue(QStringLiteral("theme/startupMode"), m_theme.startupMode);
    struct Role { const char* key; QColor PaletteSet::* member; };
    const Role roles[] = {
        {"appBackground", &PaletteSet::appBackground},
        {"dockTitle", &PaletteSet::dockTitle},
        {"canvasBackground", &PaletteSet::canvasBackground},
        {"menuBar", &PaletteSet::menuBar},
        {"toolBar", &PaletteSet::toolBar},
        {"accent", &PaletteSet::accent},
    };
    const char* groups[] = {"theme/light/", "theme/dark/", "theme/custom/"};
    for (int col = 0; col < 3; ++col) {
        for (const Role& role : roles) {
            settings.setValue(QStringLiteral("%1%2").arg(groups[col], role.key),
                              m_theme.palettes[col].*role.member);
        }
    }
}

void MainWindow::onSettings()
{
    const ThemeConfig themeBackup = m_theme;
    const CrossStitchSettings crossBackup = m_crossStitch;
    SettingsDialog dialog(m_theme, m_crossStitch, this);
    connect(&dialog, &SettingsDialog::themePreviewed, this, [this](const ThemeConfig& cfg) {
        m_theme = cfg;
        applyTheme();
    });
    connect(&dialog, &SettingsDialog::crossPreviewed, this,
            [this](const CrossStitchSettings& cfg) {
                m_crossStitch = cfg;
                // 快捷键实时生效（改了一边看一边就能试）
                applyShortcuts();
                m_dockLegend->setUsageSettings(cfg);
                refreshFromProject();
            });
    const int code = dialog.exec();
    disconnect(&dialog, &SettingsDialog::themePreviewed, this, nullptr);
    disconnect(&dialog, &SettingsDialog::crossPreviewed, this, nullptr);
    if (code == QDialog::Accepted) {
        m_theme = dialog.themeResult();
        saveThemeConfig();
        m_crossStitch = dialog.crossResult();
        m_crossStitch.save();
        applyShortcuts();
        m_dockLegend->setUsageSettings(m_crossStitch);
        refreshFromProject();
        statusBar()->showMessage(tr("Settings applied"), 2000);
    } else {
        // 取消：恢复进入对话框前的配置（主题与拼豆设置一并还原）
        m_theme = themeBackup;
        applyTheme();
        m_crossStitch = crossBackup;
        applyShortcuts();
        m_dockLegend->setUsageSettings(m_crossStitch);
        refreshFromProject();
    }
}

bool MainWindow::plateLocked(int plate) const
{
    return plate >= 0 && plate < m_crossStitch.plateLocked.size()
        && m_crossStitch.plateLocked.at(plate);
}

bool MainWindow::plateHasContent(int plate) const
{
    if (plate < 0 || plate >= static_cast<int>(m_plates.size())) {
        return false;
    }
    for (const ModeChart& mc : m_plates[plate].charts) {
        if (mc.snapshot.valid) {
            for (const CmGridCell& cell : mc.snapshot.cells) {
                if (cell.color_index != cm::kColorNone) {
                    return true;
                }
            }
        }
    }
    return false;
}

/// 激活盘锁定 → 状态栏提示并返回 true；编辑类槽函数（涂/擦/填充/镜像/色表
/// 变更/撤销重做等）开头统一调用。
bool MainWindow::lockedPlateGuard()
{
    if (!plateLocked(m_activePlate)) {
        return false;
    }
    statusBar()->showMessage(tr("Plate is locked: unlock it before editing"), 4000);
    return true;
}

/// 标题栏锁按钮 → 切换锁定：写设置（随盘存活）、同步标题栏图标。
void MainWindow::onPlateLockToggled(int plate)
{
    if (plate < 0 || plate >= kMaxPlateCount) {
        return;
    }
    while (m_crossStitch.plateLocked.size() < kMaxPlateCount) {
        m_crossStitch.plateLocked.append(false);
    }
    const bool now = !m_crossStitch.plateLocked.at(plate);
    m_crossStitch.plateLocked[plate] = now;
    m_crossStitch.save();
    if (auto* item = m_scene->crossStitchItem()) {
        item->setPlateLocked(plate, now);
    }
    statusBar()->showMessage(now ? tr("Plate locked: editing and generation disabled")
                                 : tr("Plate unlocked: editing enabled"),
                            4000);
}

/// 打印分页设置：纸张尺寸（默认 A4）+ 方向 + 每格打印尺寸（毫米）。
/// 分页完全自动：按图纸格子拆页，每页带格号标尺，所有页可拼回完整图。
void MainWindow::askPrintLayout()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Print pagination"));
    auto* form = new QFormLayout(&dlg);

    auto* paperCombo = new QComboBox(&dlg);
    // 与 CrossStitchSettings::printPaperKind 的枚举顺序一致
    const QList<QPair<int, QString>> papers = {
        { 0, QStringLiteral("A5") }, { 1, QStringLiteral("A4") },
        { 2, QStringLiteral("A3") }, { 3, QStringLiteral("Letter") },
        { 4, QStringLiteral("B5") },
    };
    for (const auto& p : papers) {
        paperCombo->addItem(p.second, p.first);
    }
    const int kindIdx = paperCombo->findData(m_crossStitch.printPaperKind);
    paperCombo->setCurrentIndex(kindIdx < 0 ? 1 : kindIdx);
    form->addRow(tr("Paper size:"), paperCombo);

    auto* orientCombo = new QComboBox(&dlg);
    orientCombo->addItem(tr("Portrait"), 0);
    orientCombo->addItem(tr("Landscape"), 1);
    orientCombo->setCurrentIndex(m_crossStitch.printLandscape ? 1 : 0);
    form->addRow(tr("Orientation:"), orientCombo);

    auto* cellSpin = new QDoubleSpinBox(&dlg);
    cellSpin->setRange(2.0, 30.0);
    cellSpin->setSingleStep(0.5);
    cellSpin->setDecimals(1);
    cellSpin->setSuffix(QStringLiteral(" mm"));
    cellSpin->setValue(m_crossStitch.printCellMm);
    cellSpin->setToolTip(
        tr("Bigger cells give clearer symbols and more pages"));
    form->addRow(tr("Cell size (mm):"), cellSpin);

    auto* hint = new QLabel(
        tr("Pages are split by chart cells (automatic); every page has cell-number "
           "rulers so all pages assemble into the full chart."), &dlg);
    hint->setWordWrap(true);
    form->addRow(hint);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);
    if (dlg.exec() == QDialog::Accepted) {
        m_crossStitch.printPaperKind = paperCombo->currentData().toInt();
        m_crossStitch.printLandscape = orientCombo->currentData().toInt() == 1;
        m_crossStitch.printCellMm = cellSpin->value();
        m_crossStitch.save();
    }
}

/// 图纸打印入口：设置纸张/每格尺寸 → **拼版预览**（每页纸张按原图位置
/// 排成矩阵，页号与原图分块一致）→ 预览里点「打印」才弹系统打印对话框
/// 图纸打印入口：**拼版预览先行** —— 预览顶部参数条（纸张/方向/每格尺寸，
/// 改动实时重算），总览（纸张按原图位置排成矩阵）/ 单张（上一张/下一张）
/// 双视图，每页底部带该页颜色汇总；预览里点「打印」才弹系统打印对话框落纸。
void MainWindow::onPrintChart()
{
    CrossStitchItem* item = m_scene ? m_scene->crossStitchItem() : nullptr;
    if (!item) {
        statusBar()->showMessage(tr("Nothing to print: no chart on the canvas"), 3000);
        return;
    }
    qInfo("[CM-print] enter paper=%d landscape=%d cellMm=%.1f",
          m_crossStitch.printPaperKind, m_crossStitch.printLandscape ? 1 : 0,
          m_crossStitch.printCellMm);

    // 参考打印机：仅取其页几何（纸张/方向/dpi）算分页；真机在预览后选
    QPrinter printer(QPrinter::HighResolution);
    printer.setFullPage(false);

    // —— 拼版预览 ——
    QDialog preview(this);
    preview.setWindowTitle(tr("Print preview"));
    preview.resize(980, 760);
    auto* previewLay = new QVBoxLayout(&preview);

    // 顶部参数条：纸张 / 方向 / 每格尺寸（实时重算）+ 视图切换 + 翻页
    auto* topLay = new QHBoxLayout;
    topLay->addWidget(new QLabel(tr("Paper size:"), &preview));
    auto* paperCombo = new QComboBox(&preview);
    // 与 CrossStitchSettings::printPaperKind 的枚举顺序一致
    const QList<QPair<int, QString>> papers = {
        { 0, QStringLiteral("A5") }, { 1, QStringLiteral("A4") },
        { 2, QStringLiteral("A3") }, { 3, QStringLiteral("Letter") },
        { 4, QStringLiteral("B5") },
    };
    for (const auto& p : papers) {
        paperCombo->addItem(p.second, p.first);
    }
    paperCombo->setCurrentIndex(paperCombo->findData(m_crossStitch.printPaperKind));
    topLay->addWidget(paperCombo);

    auto* orientCombo = new QComboBox(&preview);
    orientCombo->addItem(tr("Portrait"), 0);
    orientCombo->addItem(tr("Landscape"), 1);
    orientCombo->setCurrentIndex(m_crossStitch.printLandscape ? 1 : 0);
    topLay->addWidget(orientCombo);

    topLay->addSpacing(8);
    topLay->addWidget(new QLabel(tr("Cell size:"), &preview));
    auto* cellSpin = new QDoubleSpinBox(&preview);
    cellSpin->setRange(2.0, 30.0);
    cellSpin->setSingleStep(1.0);
    cellSpin->setDecimals(1);
    cellSpin->setSuffix(tr(" mm"));
    cellSpin->setValue(m_crossStitch.printCellMm);
    cellSpin->setFixedWidth(64); // 输入框缩窄，按钮条更紧凑
    topLay->addWidget(cellSpin);

    topLay->addWidget(new QLabel(tr("Margin:"), &preview));
    auto* marginSpin = new QDoubleSpinBox(&preview);
    marginSpin->setRange(0.0, 30.0);
    marginSpin->setSingleStep(1.0);
    marginSpin->setDecimals(1);
    marginSpin->setSuffix(tr(" mm"));
    marginSpin->setFixedWidth(64);
    marginSpin->setValue(m_crossStitch.printMarginMm);
    topLay->addWidget(marginSpin);

    topLay->addSpacing(10);
    // 视图切换：总览 / 单张（互斥按钮）
    auto* viewGroup = new QButtonGroup(&preview);
    viewGroup->setExclusive(true);
    auto* overviewBtn = new QPushButton(tr("Overview"), &preview);
    overviewBtn->setCheckable(true);
    overviewBtn->setChecked(true);
    auto* singleBtn = new QPushButton(tr("Single page"), &preview);
    singleBtn->setCheckable(true);
    viewGroup->addButton(overviewBtn, 0);
    viewGroup->addButton(singleBtn, 1);
    topLay->addWidget(overviewBtn);
    topLay->addWidget(singleBtn);

    // 缩放（单张视图方便放大观察格线 / 缩小看整页）
    auto* zoomOutBtn = new QPushButton(QStringLiteral("−"), &preview);
    zoomOutBtn->setToolTip(tr("Zoom out"));
    zoomOutBtn->setFixedWidth(28);
    auto* zoomInBtn = new QPushButton(QStringLiteral("＋"), &preview);
    zoomInBtn->setToolTip(tr("Zoom in"));
    zoomInBtn->setFixedWidth(28);
    auto* zoomLabel = new QLabel(QStringLiteral("100%"), &preview);
    topLay->addWidget(zoomOutBtn);
    topLay->addWidget(zoomInBtn);
    topLay->addWidget(zoomLabel);
    auto* prevBtn = new QPushButton(&preview);
    prevBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    prevBtn->setToolTip(tr("Previous page"));
    auto* nextBtn = new QPushButton(&preview);
    nextBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    nextBtn->setToolTip(tr("Next page"));
    topLay->addWidget(prevBtn);
    topLay->addWidget(nextBtn);
    auto* pageLabel = new QLabel(&preview);
    topLay->addWidget(pageLabel);
    topLay->addStretch(1);
    previewLay->addLayout(topLay);

    // 视图区：0 = 总览网格，1 = 单张大图
    auto* stack = new QStackedWidget(&preview);
    auto* overviewScroll = new QScrollArea(&preview);
    overviewScroll->setWidgetResizable(true);
    auto* pageCanvas = new QWidget(&preview);
    auto* grid = new QGridLayout(pageCanvas);
    grid->setSpacing(12);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    overviewScroll->setWidget(pageCanvas);
    stack->addWidget(overviewScroll);
    auto* singleScroll = new QScrollArea(&preview);
    singleScroll->setWidgetResizable(true);
    singleScroll->setAlignment(Qt::AlignCenter);
    auto* singleCard = new QLabel(&preview);
    singleCard->setAlignment(Qt::AlignCenter);
    singleCard->setFrameShape(QFrame::StyledPanel);
    singleScroll->setWidget(singleCard);
    stack->addWidget(singleScroll);
    previewLay->addWidget(stack, 1);

    auto* buttons = new QDialogButtonBox(Qt::Horizontal, &preview);
    auto* printBtn = new QPushButton(tr("Print"), &preview);
    printBtn->setDefault(true);
    auto* closeBtn = new QPushButton(tr("Cancel"), &preview);
    buttons->addButton(printBtn, QDialogButtonBox::AcceptRole);
    buttons->addButton(closeBtn, QDialogButtonBox::RejectRole);
    previewLay->addWidget(buttons);
    connect(printBtn, &QPushButton::clicked, &preview, &QDialog::accept);
    connect(closeBtn, &QPushButton::clicked, &preview, &QDialog::reject);

    // —— 实时重建：参数条改动 → 写回设置 → 重算分页 → 重渲染 ——
    PrintLayout lay;
    int curPage = 0; // 单张视图当前页（行列序扁平索引）
    int viewMode = 0; // 0 = 总览，1 = 单张
    double zoom = 1.0; // 单张视图缩放倍率（0.4–4.0）
    auto applyPaper = [&]() {
        static const QPageSize::PageSizeId kPaperIds[] = {
            QPageSize::A5, QPageSize::A4, QPageSize::A3, QPageSize::Letter, QPageSize::B5,
        };
        printer.setPageSize(QPageSize(
            kPaperIds[qBound(0, paperCombo->currentData().toInt(), 4)]));
        printer.setPageOrientation(orientCombo->currentIndex() == 1
                                       ? QPageLayout::Landscape : QPageLayout::Portrait);
        m_crossStitch.printPaperKind = paperCombo->currentData().toInt();
        m_crossStitch.printLandscape = orientCombo->currentIndex() == 1;
        m_crossStitch.printCellMm = cellSpin->value();
        m_crossStitch.printMarginMm = marginSpin->value();
    };
    auto okLay = [&]() {
        return lay.pageCols > 0 && lay.pageRows > 0;
    };
    auto updateNav = [&]() {
        const bool single = viewMode == 1;
        const int total = okLay() ? lay.pageCols * lay.pageRows : 0;
        curPage = total > 0 ? qBound(0, curPage, total - 1) : 0;
        prevBtn->setEnabled(single && total > 0 && curPage > 0);
        nextBtn->setEnabled(single && total > 0 && curPage < total - 1);
        zoomInBtn->setEnabled(single && okLay() && zoom < 4.0);
        zoomOutBtn->setEnabled(single && okLay() && zoom > 0.4);
        if (total > 0) {
            pageLabel->setText(tr("Page %1 of %2").arg(curPage + 1).arg(total));
        }
    };
    auto renderSingle = [&]() {
        if (okLay()) {
            const int r = curPage / lay.pageCols;
            const int c = curPage % lay.pageCols;
            // 缩放倍率作用在渲染宽度上（zoom 100% = 适配窗口宽）
            const qreal singleW = qMax(360.0, qreal(preview.width() - 72)) * zoom;
            const qreal scale = singleW / qMax(1.0, lay.pageW);
            // 灰底衬托纸张白边（四周 14px 桌面色），看得清纸张边界
            constexpr int kPad = 14;
            QPixmap pm(int(qCeil(lay.pageW * scale)) + kPad * 2,
                       int(qCeil(lay.pageH * scale)) + kPad * 2);
            pm.fill(QColor(168, 168, 168));
            QPainter pmPainter(&pm);
            pmPainter.translate(kPad, kPad);
            pmPainter.scale(scale, scale);
            // 白色纸张面（paintPrintPage 不画底色，先铺白）
            pmPainter.fillRect(QRectF(0, 0, lay.pageW, lay.pageH), Qt::white);
            paintPrintPage(&pmPainter, lay, item, r, c);
            pmPainter.end();
            singleCard->setPixmap(pm);
            singleCard->adjustSize();
        } else {
            singleCard->setPixmap(QPixmap());
        }
        updateNav();
    };
    auto rebuildAll = [&]() {
        applyPaper();
        qInfo("[CM-print] rebuild paper=%d landscape=%d cellMm=%.1f",
              m_crossStitch.printPaperKind, m_crossStitch.printLandscape ? 1 : 0,
              m_crossStitch.printCellMm);
        lay = printLayoutFor(&printer, item);
        qInfo("[CM-print] layout pages=%dx%d grid %dx%d",
              lay.pageCols, lay.pageRows, lay.gridW, lay.gridH);
        if (!okLay()) {
            pageLabel->setText(tr("Nothing to print: empty chart"));
        }
        curPage = 0;
        // 总览网格重建（先清空旧缩略图）
        while (auto* child = grid->takeAt(0)) {
            if (child->widget()) {
                delete child->widget();
            }
            delete child;
        }
        if (okLay()) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const qreal thumbW = 240.0; // 缩略图宽（像素），按页宽等比缩
            const qreal thumbScale = thumbW / qMax(1.0, lay.pageW);
            for (int row = 0; row < lay.pageRows; ++row) {
                for (int col = 0; col < lay.pageCols; ++col) {
                    QPixmap pm(int(qCeil(lay.pageW * thumbScale)) + 2,
                               int(qCeil(lay.pageH * thumbScale)) + 2);
                    pm.fill(Qt::white);
                    QPainter pmPainter(&pm);
                    pmPainter.translate(1, 1);
                    pmPainter.scale(thumbScale, thumbScale);
                    paintPrintPage(&pmPainter, lay, item, row, col);
                    pmPainter.end();
                    auto* card = new QLabel(&preview);
                    card->setPixmap(pm);
                    card->setAlignment(Qt::AlignCenter);
                    card->setFrameShape(QFrame::StyledPanel);
                    card->setToolTip(tr("Page %1 of %2")
                                          .arg(row * lay.pageCols + col + 1)
                                          .arg(lay.pageCols * lay.pageRows));
                    grid->addWidget(card, row, col);
                }
            }
            QApplication::restoreOverrideCursor();
        }
        m_crossStitch.save();
        renderSingle();
    };

    // 参数改动防抖 250ms（每格尺码连续加减时避免逐次重渲染）
    auto* debounce = new QTimer(&preview);
    debounce->setSingleShot(true);
    debounce->setInterval(250);
    connect(debounce, &QTimer::timeout, &preview, rebuildAll);
    connect(paperCombo, &QComboBox::currentIndexChanged, &preview,
            [debounce](int) { debounce->start(); });
    connect(orientCombo, &QComboBox::currentIndexChanged, &preview,
            [debounce](int) { debounce->start(); });
    connect(cellSpin, &QDoubleSpinBox::valueChanged, &preview,
            [debounce](double) { debounce->start(); });
    connect(marginSpin, &QDoubleSpinBox::valueChanged, &preview,
            [debounce](double) { debounce->start(); });
    connect(viewGroup, &QButtonGroup::idClicked, &preview, [&](int idx) {
        viewMode = idx;
        stack->setCurrentIndex(idx);
        updateNav();
        if (idx == 1) {
            renderSingle(); // 进单张视图时按当前页刷新
        }
    });
    auto applyZoom = [&](bool zoomIn) {
        zoom = qBound(0.4, zoom * (zoomIn ? 1.25 : 0.8), 4.0);
        zoomLabel->setText(QString::number(qRound(zoom * 100.0))
                           + QStringLiteral("%"));
        if (viewMode == 1) {
            renderSingle();
        }
        updateNav();
    };
    connect(zoomInBtn, &QPushButton::clicked, &preview,
            [&] { applyZoom(true); });
    connect(zoomOutBtn, &QPushButton::clicked, &preview,
            [&] { applyZoom(false); });
    connect(prevBtn, &QPushButton::clicked, &preview, [&]() {
        if (curPage > 0) {
            curPage--;
            renderSingle();
        }
    });
    connect(nextBtn, &QPushButton::clicked, &preview, [&]() {
        const int total = okLay() ? lay.pageCols * lay.pageRows : 0;
        if (curPage < total - 1) {
            curPage++;
            renderSingle();
        }
    });

    rebuildAll();
    qInfo("[CM-print] preview built %dx%d thumbs", lay.pageCols, lay.pageRows);

    if (preview.exec() == QDialog::Accepted) {
        // 预览确认后才选打印机（打印机 dpi/边距可能与参考页几何略异，
        // 正式打印按所选打印机重算分页，同样逐页自动拼版）
        QPrintDialog printDialog(&printer, this);
        printDialog.setWindowTitle(tr("Print chart"));
        const int verdict = printDialog.exec();
        qInfo("[CM-print] print dialog verdict=%d dpi=%d", verdict,
              printer.logicalDpiX());
        if (verdict != QDialog::Accepted) {
            return;
        }
        // 正式打印：与预览完全相同的分页几何逐页落纸
        QApplication::setOverrideCursor(Qt::WaitCursor);
        statusBar()->showMessage(tr("Printing chart…"), 10000);
        onRenderPrintChart(&printer, item);
        QApplication::restoreOverrideCursor();
        statusBar()->showMessage(tr("Chart sent to the printer"), 4000);
        qInfo("[CM-print] render done");
    }
}

/// 按当前打印设置 + 目标打印机算分页几何：每格毫米 → 设备像素，
/// 标尺带宽 1.5 格（底部另加页脚），每页尽量多装整格 → 页矩阵。
MainWindow::PrintLayout MainWindow::printLayoutFor(const QPrinter* printer,
                                                   CrossStitchItem* item) const
{
    PrintLayout lay;
    if (!printer || !item) {
        return lay;
    }
    const QRectF page = printer->pageRect(QPrinter::DevicePixel);
    lay.pageW = page.width();
    lay.pageH = page.height();
    lay.dpi = printer->logicalDpiX();
    lay.gridW = item->patternCols(m_activePlate);
    lay.gridH = item->patternRows(m_activePlate);
    const QRectF pattern = item->patternRectScene(m_activePlate);
    if (page.isEmpty() || pattern.isEmpty() || lay.gridW <= 0 || lay.gridH <= 0) {
        return lay;
    }
    lay.cellDev = m_crossStitch.printCellMm * lay.dpi / 25.4;
    lay.band = lay.cellDev * 1.5;
    // 页边距：四周留白（不含标尺带），可设 0-30mm
    lay.marginDev = m_crossStitch.printMarginMm * lay.dpi / 25.4;
    // 底部 = 标尺带（1.5 格）+ 页脚 13mm（颜色瓦片与页面信息同行）
    lay.bottom = lay.cellDev * 1.5 + 13.0 * lay.dpi / 25.4;
    const qreal usableW = qMax(lay.cellDev, lay.pageW
                               - lay.marginDev * 2.0 - lay.band * 2.0);
    // 上标尺带 + 底部（标尺 + 页脚）两条竖带
    const qreal usableH = qMax(lay.cellDev, lay.pageH
                               - lay.marginDev * 2.0 - lay.band - lay.bottom);
    lay.colsPerPage = qMax(1, static_cast<int>(usableW / lay.cellDev));
    lay.rowsPerPage = qMax(1, static_cast<int>(usableH / lay.cellDev));
    lay.pageCols = qCeil(lay.gridW / static_cast<qreal>(lay.colsPerPage));
    lay.pageRows = qCeil(lay.gridH / static_cast<qreal>(lay.rowsPerPage));
    return lay;
}

/// 画单页：图纸切片（按格整除对齐，相邻页可拼合）+ 内容边框 + 四边格号
/// 标尺（上/下列号、左/右行号，全局 1 基，共边数字互校）+ 页脚页码与格子
/// 范围 + 色块/色号/用量瓦片。坐标 = 页内左上 (0,0)；预览缩略图与正式打印共用。
void MainWindow::paintPrintPage(QPainter* painter, const PrintLayout& lay,
                                CrossStitchItem* item, int row, int col) const
{
    if (!painter || !item || lay.pageCols <= 0 || lay.pageRows <= 0) {
        return;
    }
    // 页边距：后续坐标全部在"内页"坐标系（原点 = 边距左上角）
    painter->save();
    painter->translate(lay.marginDev, lay.marginDev);
    const qreal pageWIn = lay.pageW - lay.marginDev * 2.0;
    const qreal pageHIn = lay.pageH - lay.marginDev * 2.0;
    const QRectF pattern = item->patternRectScene(m_activePlate);
    const qreal cellScene = pattern.width() / lay.gridW;
    const int c0 = col * lay.colsPerPage;
    const int r0 = row * lay.rowsPerPage;
    const int w = qMin(lay.colsPerPage, lay.gridW - c0);
    const int h = qMin(lay.rowsPerPage, lay.gridH - r0);

    const QRectF slice(pattern.left() + c0 * cellScene,
                       pattern.top() + r0 * cellScene, w * cellScene, h * cellScene);
    const QRectF content(lay.band, lay.band, w * lay.cellDev, h * lay.cellDev);
    m_scene->render(painter, content, slice);

    // 显式格线覆盖：场景渲染的 cosmetic 1px 细线在 600dpi 纸上只有
    // 0.04mm（看不清），这里按物理线宽重画——浅灰每格线 + 每 10 格深色定位线
    {
        QPen thin(QColor(158, 156, 148));
        thin.setWidthF(qMax(1.0, lay.dpi * 0.10 / 25.4)); // ≈0.10mm
        painter->setPen(thin);
        painter->setBrush(Qt::NoBrush);
        for (int i = 0; i <= w; ++i) {
            const qreal x = content.left() + i * lay.cellDev;
            painter->drawLine(QPointF(x, content.top()),
                              QPointF(x, content.bottom()));
        }
        for (int i = 0; i <= h; ++i) {
            const qreal y = content.top() + i * lay.cellDev;
            painter->drawLine(QPointF(content.left(), y),
                              QPointF(content.right(), y));
        }
        QPen major(QColor(48, 48, 48));
        major.setWidthF(qMax(1.0, lay.dpi * 0.20 / 25.4)); // ≈0.20mm
        painter->setPen(major);
        for (int i = 0; i <= w; ++i) {
            if ((c0 + i) % 10 != 0) {
                continue;
            }
            const qreal x = content.left() + i * lay.cellDev;
            painter->drawLine(QPointF(x, content.top()),
                              QPointF(x, content.bottom()));
        }
        for (int i = 0; i <= h; ++i) {
            if ((r0 + i) % 10 != 0) {
                continue;
            }
            const qreal y = content.top() + i * lay.cellDev;
            painter->drawLine(QPointF(content.left(), y),
                              QPointF(content.right(), y));
        }
    }

    // 内容区边框（切页参考线）
    painter->setPen(QPen(QColor(70, 70, 70), 0.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(content);

    // 四边格号标尺：上/下 = 列号，左/右 = 行号；每 10 格一条长刻度
    // 注意：字体一律用 setPixelSize（页面设备像素，≈0.45 格高）。pt 是
    // 设备相关的，预览缩略图（96dpi + scale 变换）下会缩得看不清。
    QFont rulerFont = painter->font();
    rulerFont.setPixelSize(qMax(6, qRound(lay.cellDev * 0.45)));
    painter->setFont(rulerFont);
    painter->setPen(QColor(50, 50, 50));
    const qreal tick = lay.cellDev * 0.12;
    for (int i = 0; i < w; ++i) {
        const QString n = QString::number(c0 + i + 1);
        painter->drawText(QRectF(content.left() + i * lay.cellDev, 0,
                                 lay.cellDev, lay.band),
                          Qt::AlignHCenter | Qt::AlignBottom, n);
        painter->drawText(QRectF(content.left() + i * lay.cellDev,
                                 content.bottom(), lay.cellDev, lay.band),
                          Qt::AlignHCenter | Qt::AlignTop, n);
        // 刻度：贴着内容框上下缘向内（每格短刻度，每 10 格长刻度）
        const qreal x = content.left() + (i + 1) * lay.cellDev;
        const qreal len = ((c0 + i + 1) % 10 == 0) ? tick * 2.0 : tick;
        painter->drawLine(QPointF(x, content.top()),
                          QPointF(x, content.top() + len));
        painter->drawLine(QPointF(x, content.bottom()),
                          QPointF(x, content.bottom() - len));
    }
    for (int i = 0; i < h; ++i) {
        const QString n = QString::number(r0 + i + 1);
        painter->drawText(QRectF(0, content.top() + i * lay.cellDev,
                                 lay.band, lay.cellDev),
                          Qt::AlignRight | Qt::AlignVCenter, n);
        painter->drawText(QRectF(content.right(), content.top() + i * lay.cellDev,
                                 lay.band, lay.cellDev),
                          Qt::AlignLeft | Qt::AlignVCenter, n);
        const qreal y = content.top() + (i + 1) * lay.cellDev;
        const qreal len = ((r0 + i + 1) % 10 == 0) ? tick * 2.0 : tick;
        painter->drawLine(QPointF(content.left(), y),
                          QPointF(content.left() + len, y));
        painter->drawLine(QPointF(content.right(), y),
                          QPointF(content.right() - len, y));
    }
    // 页脚同一行：颜色瓦片（左侧起排，可换行） + 页码/格范围（右对齐第一行）
    const qreal mmDev = lay.dpi / 25.4;
    const qreal footerMm = 13.0;
    const qreal footerTop = pageHIn - footerMm * mmDev;
    QFont footFont = painter->font();
    // 页码行字高 ≈2.6mm（页面设备像素；pt 在预览变换下会失真）
    footFont.setPixelSize(qMax(10, qRound(2.6 * mmDev)));
    painter->setFont(footFont);
    const QString pageText =
        tr("Page %1 of %2")
                .arg(row * lay.pageCols + col + 1)
                .arg(lay.pageCols * lay.pageRows)
            + QStringLiteral("  |  ")
            + tr("Cells %1-%2 × %3-%4")
                  .arg(c0 + 1).arg(c0 + w).arg(r0 + 1).arg(r0 + h);
    const qreal pageTextW = QFontMetrics(footFont).horizontalAdvance(pageText);
    painter->drawText(QRectF(0, footerTop + 0.4 * mmDev, pageWIn - 1.5 * mmDev,
                             3.2 * mmDev),
                      Qt::AlignRight | Qt::AlignTop, pageText);
    const QVector<CrossStitchItem::ColorUsage> usage =
        item->pageColorUsage(m_activePlate, c0, r0, w, h);
    if (!usage.isEmpty()) {
        // 颜色区：色块 2.6mm + 色号 + ×数量（放不下时降级为只编码号）
        const qreal sw = 2.6 * mmDev;      // 色块边长
        const qreal gap = 0.7 * mmDev;     // 色块-文字间距
        const qreal itemGap = 2.4 * mmDev; // 瓦片间距
        const qreal rowH = 3.2 * mmDev;    // 行高
        const qreal x0t = content.left() + 1.0 * mmDev;
        const qreal xMax = pageWIn - 1.5 * mmDev;
        const qreal yTop = footerTop + 0.6 * mmDev;
        const qreal yMax = lay.pageH - 0.8 * mmDev;

        QFont tileFont = painter->font();
        // 色号文字字高 ≈2.2mm（页面设备像素）
        tileFont.setPixelSize(qMax(9, qRound(2.2 * mmDev)));
        painter->setFont(tileFont);
        const QFontMetrics fm(tileFont);

        // 先试整版（色块+色号+×用量）；放不下则去掉用量重排。
        // 首行右侧让出页码文字位，颜色瓦片与页面信息同一行。
        auto drawTiles = [&](bool withCount) -> bool {
            qreal x = x0t;
            qreal y = yTop;
            int tileRow = 0;
            for (const auto& u : usage) {
                const QString label = withCount
                    ? QStringLiteral("%1 ×%2").arg(u.code, QString::number(u.count))
                    : u.code;
                const qreal tw = fm.horizontalAdvance(label);
                const qreal itemW = sw + gap + tw;
                const qreal rowMax = tileRow == 0 ? xMax - pageTextW - 3.0 * mmDev
                                                  : xMax;
                if (x + itemW > rowMax) {
                    x = x0t;
                    y += rowH;
                    tileRow++;
                }
                if (y + sw > yMax) {
                    return false;
                }
                // 色块（浅色块加深边框，易辨识）
                painter->setPen(QPen(QColor(60, 60, 60), 0.0));
                painter->setBrush(u.color);
                painter->drawRect(QRectF(x, y, sw, sw));
                painter->setPen(QColor(30, 30, 30));
                painter->drawText(QRectF(x + sw + gap, y, tw + 1, sw),
                                  Qt::AlignLeft | Qt::AlignVCenter, label);
                x += itemW + itemGap;
            }
            return true;
        };
        if (!drawTiles(true)) {
            drawTiles(false);
        }
        painter->setBrush(Qt::NoBrush);
    }
    painter->restore(); // 回到整页坐标系（边距 translate 的配对 restoring）
}

/// 正式打印：按分页几何逐页落纸（newPage 翻页），预览与实打共用 paintPrintPage。
void MainWindow::onRenderPrintChart(QPrinter* printer, CrossStitchItem* item)
{
    if (!printer || !item) {
        return;
    }
    const PrintLayout lay = printLayoutFor(printer, item);
    if (lay.pageCols <= 0 || lay.pageRows <= 0) {
        return;
    }
    qInfo("[CM-print] render %dx%d pages dpi=%.0f cellDev=%.1f",
          lay.pageCols, lay.pageRows, lay.dpi, lay.cellDev);
    m_scene->setTextPreviewVisible(false);
    QPainter painter(printer);
    painter.setRenderHint(QPainter::Antialiasing, false);
    int printed = 0;
    for (int row = 0; row < lay.pageRows; ++row) {
        for (int col = 0; col < lay.pageCols; ++col) {
            if (printed++ > 0) {
                printer->newPage();
            }
            paintPrintPage(&painter, lay, item, row, col);
        }
    }
    painter.end();
    m_scene->setTextPreviewVisible(m_textPreviewActive);
}
