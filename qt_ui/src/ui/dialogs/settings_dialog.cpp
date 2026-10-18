#include "settings_dialog.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QRadioButton>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ui/cross_stitch_settings.h"

PaletteSet PaletteSet::defaults(bool dark)
{
    PaletteSet p;
    if (dark) {
        // 深色：炭蓝灰分层（画布最深 → 面板标题最亮，Linear 风格）
        p.canvasBackground = QColor(0x15181c); // 画布（视口，最深）
        p.appBackground = QColor(0x1a1d21);    // 软件背景
        p.toolBar = QColor(0x21262c);          // 工具栏
        p.menuBar = QColor(0x252b32);          // 菜单栏 / 标题栏
        p.dockTitle = QColor(0x2c333b);        // 面板标题（最亮，突出层次）
    } else {
        // 浅色：Photon Studio 暖中性灰（菜单栏最亮 → 面板渐深，画布独立中灰）
        p.menuBar = QColor(0xf5f5f5);          // 菜单栏 / 标题栏（最亮）
        p.toolBar = QColor(0xeaeaea);          // 工具栏
        p.appBackground = QColor(0xececec);    // 软件背景（内容区）
        p.dockTitle = QColor(0xe3e3e3);        // 面板标题
        p.canvasBackground = QColor(0xdcdcdc); // 画布（视口）
    }
    p.accent = QColor(0xa11740); // 高亮色默认深红，用户可在设置中修改
    return p;
}

ThemeConfig ThemeConfig::defaults()
{
    ThemeConfig cfg;
    cfg.mode = 0;
    cfg.darkBase = false;
    cfg.palettes[0] = PaletteSet::defaults(false);
    cfg.palettes[1] = PaletteSet::defaults(true);
    cfg.palettes[2] = PaletteSet::defaults(false);
    return cfg;
}

namespace {
const char* kSwatchStyle = "QPushButton { background: %1; border: 1px solid #a0a0a0;"
                           " border-bottom-color: #8a8a8a; border-radius: 6px; }";
const int kColorRoleCount = 6;
const char* kRoleLabelKey[kColorRoleCount] = {
    "appBackground", "dockTitle", "canvasBackground", "menuBar", "toolBar", "accent",
};
} // namespace

/// 统一设置对话框：通用设置（主题 / 启动状态）与拼豆设置已合并为单个面板，
/// 取代原先「设置」下拉里的两个入口。
SettingsDialog::SettingsDialog(const ThemeConfig& current, const CrossStitchSettings& cross,
                               QWidget* parent)
    : QDialog(parent)
    , m_config(current)
    , m_cross(cross)
{
    // 宽度适应内容：分类 140px + 表单页
    setMinimumSize(660, 480);
    setWindowTitle(tr("Settings"));

    auto* rootLay = new QVBoxLayout(this);
    auto* contentLay = new QHBoxLayout;

    // 左侧目录：主题 / 启动状态 / 用量 / 布料 / 图案 / 快捷键（顺序即页序）
    m_categories = new QListWidget(this);
    m_categories->setObjectName(QStringLiteral("SettingsCategories"));
    m_categories->setFixedWidth(140);
    m_categories->addItem(tr("Theme"));
    m_categories->addItem(tr("Startup state"));
    m_categories->addItem(tr("Usage"));
    m_categories->addItem(tr("Fabric"));
    m_categories->addItem(tr("Pattern"));
    m_categories->addItem(tr("Shortcuts"));
    contentLay->addWidget(m_categories);

    // 右侧内容页
    m_pages = new QStackedWidget(this);
    contentLay->addWidget(m_pages, 1);
    rootLay->addLayout(contentLay, 1);

    auto* page = new QWidget(this);
    auto* pageLay = new QVBoxLayout(page);

    pageLay->addSpacing(6);

    // ---- 颜色设置：两列（浅色 | 自定义；深色主题已移除），列标题即主题选择，色块均可编辑 ----
    auto* grid = new QGridLayout;
    const QString headerTexts[2] = {
        tr("Light theme"), tr("Custom"),
    };
    for (int col = 0; col < 2; ++col) {
        const int mode = (col == 0) ? 0 : 2; // 深色(1)已废弃，自定义沿用 palettes[2]
        auto* header = new QPushButton(headerTexts[col], page);
        header->setCheckable(true);
        header->setAutoExclusive(true);
        header->setCursor(Qt::PointingHandCursor);
        header->setFixedHeight(30);
        header->setChecked(m_config.mode == mode);
        header->setStyleSheet(
            QStringLiteral("QPushButton { padding: 4px 12px; }"
                           "QPushButton:checked { background: #d9d9d9;"
                           " border-color: #8f8f8f; font-weight: 600; }"));
        grid->addWidget(header, 0, col + 1, Qt::AlignCenter);
        m_headerButtons.append(header);
        connect(header, &QPushButton::clicked, this, [this, mode]() {
            m_config.mode = mode;
            if (mode == 0) {
                m_config.darkBase = false; // 深色基础已废弃
            }
            emitPreview();
        });
    }

    const QString roleLabels[kColorRoleCount] = {
        tr("App background color"),
        tr("Panel title color"),
        tr("Canvas background color"),
        tr("Menu bar color"),
        tr("Toolbar color"),
        tr("Highlight color"),
    };

    for (int i = 0; i < kColorRoleCount; ++i) {
        grid->addWidget(new QLabel(roleLabels[i], page), i + 1, 0);
        for (int col = 0; col < 2; ++col) {
            const int palIdx = (col == 0) ? 0 : 2; // 深色列(1)已移除
            auto* swatch = makeSwatch(page, *rolePtr(m_config.palettes[palIdx], i));
            m_swatches[palIdx][i] = swatch;
            grid->addWidget(swatch, i + 1, col + 1, Qt::AlignCenter);

            const int role = i;
            const int column = palIdx;
            connect(swatch, &QPushButton::clicked, this, [this, role, column]() {
                QColor* target = rolePtr(m_config.palettes[column], role);
                const QColor picked =
                    QColorDialog::getColor(*target, this, tr("Color settings"));
                if (!picked.isValid()) {
                    return;
                }
                *target = picked;
                m_swatches[column][role]->setStyleSheet(
                    QString::fromUtf8(kSwatchStyle).arg(picked.name()));
                emitPreview();
            });
        }
    }
    grid->setHorizontalSpacing(10);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 0);
    grid->setColumnStretch(3, 0);
    grid->setColumnStretch(4, 1); // 剩余空间推到最右，三列整体靠左
    pageLay->addLayout(grid);

    auto* resetButton = new QPushButton(tr("Reset to theme defaults"), page);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        m_config.palettes[0] = PaletteSet::defaults(false);
        m_config.palettes[1] = PaletteSet::defaults(true);
        m_config.palettes[2] = PaletteSet::defaults(m_config.darkBase);
        updateSwatchStyles();
        emitPreview();
    });
    pageLay->addWidget(resetButton);

    pageLay->addStretch();

    // 页面加入右侧堆叠
    m_pages->addWidget(page);

    // ---- 起始状态：软件启动时默认激活的手作品类（默认拼豆） ----
    auto* startupPage = new QWidget(this);
    auto* startupLay = new QVBoxLayout(startupPage);
    // 模块已按拼豆命名，设置里的启动模块名称随之更新
    auto* startupGroup = new QGroupBox(tr("Perler-bead module startup state"), startupPage);
    auto* groupLay = new QVBoxLayout(startupGroup);
    const QString modeLabels[3] = {
        tr("Cross stitch"), tr("Bead embroidery"), tr("Perler beads"),
    };
    auto* modeGroup = new QButtonGroup(startupGroup);
    const int order[3] = {2, 1, 0}; // 拼豆、珠绣、十字绣（拼豆优先）
    for (int m : order) {
        auto* rb = new QRadioButton(modeLabels[m], startupGroup);
        rb->setChecked(m_config.startupMode == m);
        modeGroup->addButton(rb, m);
        groupLay->addWidget(rb);
    }
    QObject::connect(modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_config.startupMode = id;
        emitPreview();
    });
    startupLay->addWidget(startupGroup);
    startupLay->addStretch();
    m_pages->addWidget(startupPage);

    // ---- 拼豆设置页并入（用量 / 布料 / 图案 / 快捷键） ----
    buildCrossPages();

    connect(m_categories, &QListWidget::currentRowChanged,
            m_pages, &QStackedWidget::setCurrentIndex);
    m_categories->setCurrentRow(0);

    // ---- 底部按钮 ----
    auto* buttonRow = new QHBoxLayout;
    auto* okButton = new QPushButton(tr("OK"), this);
    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    buttonRow->addStretch();
    buttonRow->addWidget(okButton);
    buttonRow->addWidget(cancelButton);
    rootLay->addLayout(buttonRow);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

QColor* SettingsDialog::rolePtr(PaletteSet& palette, int role)
{
    switch (role) {
    case 0: return &palette.appBackground;
    case 1: return &palette.dockTitle;
    case 2: return &palette.canvasBackground;
    case 3: return &palette.menuBar;
    case 4: return &palette.toolBar;
    default: return &palette.accent;
    }
}

QPushButton* SettingsDialog::makeSwatch(QWidget* parent, const QColor& color)
{
    auto* swatch = new QPushButton(parent);
    swatch->setFixedSize(64, 24);
    swatch->setCursor(Qt::PointingHandCursor);
    swatch->setFocusPolicy(Qt::NoFocus);
    swatch->setStyleSheet(QString::fromUtf8(kSwatchStyle).arg(color.name()));
    return swatch;
}

void SettingsDialog::updateSwatchStyles()
{
    const int visibleCols[] = {0, 2}; // 深色列(1)已移除
    for (int col : visibleCols) {
        for (int role = 0; role < kColorRoleCount; ++role) {
            m_swatches[col][role]->setStyleSheet(
                QString::fromUtf8(kSwatchStyle)
                    .arg(rolePtr(m_config.palettes[col], role)->name()));
        }
    }
}

void SettingsDialog::emitPreview()
{
    emit themePreviewed(m_config);
}

// ------------------------------------------------------------
// 拼豆设置页（原拼豆设置对话框内容整体并入，逐页迁移）
// ------------------------------------------------------------

void SettingsDialog::buildCrossPages()
{
    // ==================== 用量 ====================
    {
        auto* page = new QWidget(m_pages);
        auto* lay = new QVBoxLayout(page);

        auto* threadBox = new QGroupBox(tr("Thread usage per stitch"), page);
        auto* form = new QFormLayout(threadBox);

        m_stitchLen = new QDoubleSpinBox(threadBox);
        m_stitchLen->setRange(1.0, 50.0);
        m_stitchLen->setDecimals(1);
        m_stitchLen->setSingleStep(0.5);
        m_stitchLen->setSuffix(tr(" mm"));
        m_stitchLen->setToolTip(tr("Length of working thread consumed by one full cross stitch,\n"
                                   "including the two diagonals and the travel across the back.\n"
                                   "Industry average on 14ct Aida is about 9 mm at 2 strands."));
        m_stitchLen->setValue(m_cross.stitchLengthMm);
        form->addRow(tr("Thread per stitch"), m_stitchLen);

        m_strands = new QSpinBox(threadBox);
        m_strands->setRange(1, 6);
        m_strands->setToolTip(tr("How many strands you stitch with. A 6-strand skein splits into\n"
                                 "6/strands bundles, so fewer strands means more stitches per skein."));
        m_strands->setValue(m_cross.strands);
        form->addRow(tr("Strands used"), m_strands);

        m_skeinMeters = new QDoubleSpinBox(threadBox);
        m_skeinMeters->setRange(0.5, 50.0);
        m_skeinMeters->setDecimals(1);
        m_skeinMeters->setSingleStep(0.5);
        m_skeinMeters->setSuffix(tr(" m"));
        m_skeinMeters->setToolTip(tr("Length of one skein. DMC 6-strand floss is 8 metres per skein."));
        m_skeinMeters->setValue(m_cross.skeinMeters);
        form->addRow(tr("Length per skein"), m_skeinMeters);

        m_flossMargin = new QSpinBox(threadBox);
        m_flossMargin->setRange(0, 200);
        m_flossMargin->setSuffix(QStringLiteral(" %"));
        m_flossMargin->setToolTip(tr("Extra floss to keep in reserve for starting, finishing,\n"
                                     "turning the work over and rethreading. 15-25%% is typical."));
        m_flossMargin->setValue(m_cross.flossMarginPct);
        form->addRow(tr("Floss margin"), m_flossMargin);

        m_flossPreview = new QLabel(threadBox);
        m_flossPreview->setWordWrap(true);
        m_flossPreview->setStyleSheet(QStringLiteral("color: #5d5d5d;"));
        form->addRow(m_flossPreview);

        lay->addWidget(threadBox);

        // 常见配置一键套用（2×3 网格，避免横向排 6 个按钮撑宽对话框）
        auto* presetBox = new QGroupBox(tr("Common presets"), page);
        auto* presetGrid = new QGridLayout(presetBox);
        struct Preset {
            const char* label;
            int ct;
            int strands;
        };
        const Preset presets[] = {
            {"6ct / 6 strands", 6, 6},   {"11ct / 3 strands", 11, 3},
            {"14ct / 2 strands", 14, 2}, {"16ct / 2 strands", 16, 2},
            {"18ct / 1 strand", 18, 1},  {"22ct / 1 strand", 22, 1},
        };
        for (int i = 0; i < 6; ++i) {
            const Preset& p = presets[i];
            auto* btn = new QPushButton(QString::fromLatin1(p.label), presetBox);
            const int ct = p.ct;
            const int st = p.strands;
            connect(btn, &QPushButton::clicked, this, [this, ct, st]() {
                m_fabricCount->setValue(ct);
                m_strands->setValue(st);
                syncCrossWidgets();
                emitCrossChanged();
            });
            presetGrid->addWidget(btn, i / 3, i % 3);
        }
        lay->addWidget(presetBox);
        // 珠绣 / 拼豆用量并入用量设置页（珠子计数 / 余量 / 直径）
        auto* beadBox = new QGroupBox(tr("Bead embroidery"), page);
        auto* beadForm = new QFormLayout(beadBox);

        m_countBeads = new QCheckBox(tr("Count beads for bead embroidery"), beadBox);
        m_countBeads->setChecked(m_cross.countBeads);
        beadForm->addRow(m_countBeads);

        m_beadMargin = new QSpinBox(beadBox);
        m_beadMargin->setRange(0, 200);
        m_beadMargin->setSuffix(QStringLiteral(" %"));
        m_beadMargin->setToolTip(tr("Beads get dropped and need re-stitching, so the reserve\n"
                                    "is usually higher than for floss. 10-20%% is typical."));
        m_beadMargin->setValue(m_cross.beadMarginPct);
        beadForm->addRow(tr("Bead margin"), m_beadMargin);

        m_beadSize = new QDoubleSpinBox(beadBox);
        m_beadSize->setRange(0.5, 20.0);
        m_beadSize->setDecimals(1);
        m_beadSize->setSingleStep(0.5);
        m_beadSize->setSuffix(tr(" mm"));
        m_beadSize->setToolTip(tr("Bead diameter. Common Miyuki sizes are 2 / 3 / 4 / 5 mm.\n"
                                  "Uniform bead size matters more than the exact value."));
        m_beadSize->setValue(m_cross.beadSizeMm);
        beadForm->addRow(tr("Bead size"), m_beadSize);

        lay->addWidget(beadBox);
        lay->addStretch();
        m_pages->addWidget(page);
    }

    // ==================== 布料 ====================
    {
        auto* page = new QWidget(m_pages);
        auto* lay = new QVBoxLayout(page);
        // 布料分两套体系：拼豆用珠板（盘数 / 盘边距），十字绣用布料（计数 / 留边）
        auto* perlerBox = new QGroupBox(tr("Perler bead plates"), page);
        auto* perlerForm = new QFormLayout(perlerBox);

        m_plateCount = new QSpinBox(perlerBox);
        m_plateCount->setRange(1, kMaxPlateCount);
        m_plateCount->setToolTip(tr("Split a large chart into several independent plates\n"
                                    "(like multi-plate slicing). 1 = single plate."));
        m_plateCount->setValue(m_cross.plateCount);
        perlerForm->addRow(tr("Plate count"), m_plateCount);

        m_plateMargin = new QSpinBox(perlerBox);
        m_plateMargin->setRange(0, 50);
        m_plateMargin->setSuffix(tr(" cells"));
        m_plateMargin->setToolTip(tr("Extra blank grid around the pattern on every side of a plate,\n"
                                     "e.g. 10 cells so the plate edges hold the beads/stitches."));
        m_plateMargin->setValue(m_cross.plateMarginCells);
        perlerForm->addRow(tr("Plate margin"), m_plateMargin);
        lay->addWidget(perlerBox);

        auto* box = new QGroupBox(tr("Cross-stitch fabric"), page);
        auto* form = new QFormLayout(box);

        m_fabricCount = new QSpinBox(box);
        m_fabricCount->setRange(6, 32);
        m_fabricCount->setSuffix(tr(" ct"));
        m_fabricCount->setToolTip(tr("Stitches per inch. Cell size = 25.4 / ct mm.\n"
                                     "Evenweave and linen are stitched over 2 threads,\n"
                                     "so 28-count linen behaves like 14ct."));
        m_fabricCount->setValue(m_cross.fabricCount);
        form->addRow(tr("Fabric count"), m_fabricCount);

        m_borderMargin = new QSpinBox(box);
        m_borderMargin->setRange(0, 500);
        m_borderMargin->setSingleStep(5);
        m_borderMargin->setSuffix(tr(" mm"));
        m_borderMargin->setToolTip(tr("Extra fabric on every side for framing or finishing.\n"
                                      "75 mm (3 inches) per side is a common minimum."));
        m_borderMargin->setValue(m_cross.borderMarginMm);
        form->addRow(tr("Border margin per side"), m_borderMargin);

        m_extraStitches = new QSpinBox(box);
        m_extraStitches->setRange(0, 100);
        m_extraStitches->setSingleStep(2);
        m_extraStitches->setToolTip(tr("Extra stitches per colour to cover starting\n"
                                       "and finishing that thread."));
        m_extraStitches->setValue(m_cross.extraStitchesPerColor);
        form->addRow(tr("Extra stitches per colour"), m_extraStitches);

        m_coverage = new QSpinBox(box);
        m_coverage->setRange(1, 100);
        m_coverage->setSuffix(QStringLiteral(" %"));
        m_coverage->setToolTip(tr("Share of the grid that actually gets stitched.\n"
                                  "Sparse motifs may only fill 60%% of the grid."));
        m_coverage->setValue(m_cross.coveragePct);
        form->addRow(tr("Coverage"), m_coverage);

        lay->addWidget(box);
        lay->addStretch();
        m_pages->addWidget(page);
    }

    // ==================== 图案 ====================
    {
        auto* page = new QWidget(m_pages);
        auto* lay = new QVBoxLayout(page);

        auto* genBox = new QGroupBox(tr("Pattern generation"), page);
        auto* genForm = new QFormLayout(genBox);

        m_maxColors = new QSpinBox(genBox);
        m_maxColors->setRange(1, 64);
        m_maxColors->setValue(m_cross.maxColors);
        genForm->addRow(tr("Max colors"), m_maxColors);

        m_symbolSet = new QComboBox(genBox);
        m_symbolSet->addItem(tr("Letters"));
        m_symbolSet->addItem(tr("Digits"));
        m_symbolSet->addItem(tr("Geometric"));
        m_symbolSet->addItem(tr("Mixed"));
        m_symbolSet->setCurrentIndex(qBound(0, m_cross.symbolSet, 3));
        genForm->addRow(tr("Symbol set"), m_symbolSet);

        m_cleanup = new QComboBox(genBox);
        m_cleanup->addItem(tr("Off"));
        m_cleanup->addItem(tr("Light (single stitches)"));
        m_cleanup->addItem(tr("Strong (single + small patches)"));
        m_cleanup->setCurrentIndex(qBound(0, m_cross.cleanup, 2));
        genForm->addRow(tr("Noise cleanup"), m_cleanup);

        m_rarePpt = new QSpinBox(genBox);
        m_rarePpt->setRange(0, 100);
        m_rarePpt->setSuffix(QStringLiteral(" ‰"));
        m_rarePpt->setToolTip(tr("Colours used less than this share of the design are merged\n"
                                 "into their nearest main colour. 0 disables merging."));
        m_rarePpt->setValue(m_cross.rareColorPpt);
        genForm->addRow(tr("Rare color merge"), m_rarePpt);

        m_dither = new QCheckBox(tr("Floyd-Steinberg dithering"), genBox);
        m_dither->setChecked(m_cross.dither);
        genForm->addRow(m_dither);

        lay->addWidget(genBox);

        auto* viewBox = new QGroupBox(tr("Canvas display"), page);
        auto* viewForm = new QFormLayout(viewBox);

        m_showSymbols = new QCheckBox(tr("Show symbols"), viewBox);
        m_showSymbols->setChecked(m_cross.showSymbols);
        viewForm->addRow(m_showSymbols);

        m_showGrid = new QCheckBox(tr("Show grid lines"), viewBox);
        m_showGrid->setChecked(m_cross.showGrid);
        viewForm->addRow(m_showGrid);

        m_majorGrid = new QSpinBox(viewBox);
        m_majorGrid->setRange(1, 100);
        m_majorGrid->setSuffix(tr(" cells"));
        m_majorGrid->setToolTip(tr("A heavier line every N cells makes counting easier."));
        m_majorGrid->setValue(m_cross.majorGridEvery);
        viewForm->addRow(tr("Bold line every"), m_majorGrid);


        lay->addWidget(viewBox);

        // ---- 材料表面板列显示 ----
        auto* colBox = new QGroupBox(tr("Material list columns"), page);
        auto* colForm = new QFormLayout(colBox);

        m_showColorNameCol = new QCheckBox(tr("Colour name (DMC official name)"), colBox);
        m_showColorNameCol->setChecked(m_cross.showColorNameColumn);
        m_showColorNameCol->setToolTip(tr("Most charts only need the DMC code, so this is off by default."));
        colForm->addRow(m_showColorNameCol);

        m_showStitchesCol = new QCheckBox(tr("Stitches"), colBox);
        m_showStitchesCol->setChecked(m_cross.showStitchesColumn);
        colForm->addRow(m_showStitchesCol);

        m_showBeadsCol = new QCheckBox(tr("Beads"), colBox);
        m_showBeadsCol->setChecked(m_cross.showBeadsColumn);
        colForm->addRow(m_showBeadsCol);

        m_showThreadCol = new QCheckBox(tr("Thread"), colBox);
        m_showThreadCol->setChecked(m_cross.showThreadColumn);
        colForm->addRow(m_showThreadCol);

        lay->addWidget(colBox);
        lay->addStretch();
        m_pages->addWidget(page);
    }

    // ==================== 快捷键 ====================
    {
        auto* page = new QWidget(m_pages);
        auto* lay = new QVBoxLayout(page);

        auto* keyBox = new QGroupBox(tr("Shortcuts"), page);
        auto* form = new QFormLayout(keyBox);

        m_keyPicker = new QKeySequenceEdit(QKeySequence(m_cross.keyPicker), keyBox);
        m_keyPicker->setToolTip(tr("Shortcut for the pick-colour (eyedropper) tool.\n"
                                   "Click into the box and press the key you want,\n"
                                   "click the clear button to fall back to the default."));
        form->addRow(tr("Eyedropper (pick colour)"), m_keyPicker);

        m_keyViewMax = new QKeySequenceEdit(QKeySequence(m_cross.keyViewMax), keyBox);
        m_keyViewMax->setToolTip(tr("Shortcut for the view mode (maximized canvas: all panels,\n"
                                    "toolbars and menu are hidden; press again to restore).\n"
                                    "The key only fires while the canvas has focus."));
        form->addRow(tr("Maximize canvas (view mode)"), m_keyViewMax);

        lay->addWidget(keyBox);
        lay->addStretch();
        m_pages->addWidget(page);
    }

    // ---- 全部信号汇入 emitCrossChanged ----
    // 各控件信号都带参数，用无参 lambda 吞掉再转发。
    // 模板参数 Sig 是具体的成员函数指针类型，交给 connect 做重载决议。
    const auto hookValue = [this](auto* obj, auto sig) {
        connect(obj, sig, this, [this](auto) { emitCrossChanged(); });
    };
    hookValue(m_strands, &QSpinBox::valueChanged);
    hookValue(m_flossMargin, &QSpinBox::valueChanged);
    hookValue(m_fabricCount, &QSpinBox::valueChanged);
    hookValue(m_beadMargin, &QSpinBox::valueChanged);
    hookValue(m_borderMargin, &QSpinBox::valueChanged);
    hookValue(m_extraStitches, &QSpinBox::valueChanged);
    hookValue(m_coverage, &QSpinBox::valueChanged);
    hookValue(m_maxColors, &QSpinBox::valueChanged);
    hookValue(m_rarePpt, &QSpinBox::valueChanged);
    hookValue(m_majorGrid, &QSpinBox::valueChanged);
    hookValue(m_plateCount, &QSpinBox::valueChanged);
    hookValue(m_plateMargin, &QSpinBox::valueChanged);
    hookValue(m_stitchLen, &QDoubleSpinBox::valueChanged);
    hookValue(m_skeinMeters, &QDoubleSpinBox::valueChanged);
    hookValue(m_beadSize, &QDoubleSpinBox::valueChanged);

    const auto hookToggle = [this](QAbstractButton* btn) {
        connect(btn, &QAbstractButton::toggled, this, [this](bool) { emitCrossChanged(); });
    };
    hookToggle(m_countBeads);
    hookToggle(m_dither);
    hookToggle(m_showSymbols);
    hookToggle(m_showGrid);
    hookToggle(m_showColorNameCol);
    hookToggle(m_showStitchesCol);
    hookToggle(m_showBeadsCol);
    hookToggle(m_showThreadCol);

    hookValue(m_symbolSet, &QComboBox::currentIndexChanged);
    hookValue(m_cleanup, &QComboBox::currentIndexChanged);
    hookValue(m_keyPicker, &QKeySequenceEdit::keySequenceChanged);
    hookValue(m_keyViewMax, &QKeySequenceEdit::keySequenceChanged);

    emitCrossChanged();
}

void SettingsDialog::emitCrossChanged()
{
    m_cross.stitchLengthMm = m_stitchLen->value();
    m_cross.strands = m_strands->value();
    m_cross.skeinMeters = m_skeinMeters->value();
    m_cross.flossMarginPct = m_flossMargin->value();

    m_cross.beadMarginPct = m_beadMargin->value();
    m_cross.beadSizeMm = m_beadSize->value();
    m_cross.countBeads = m_countBeads->isChecked();

    m_cross.fabricCount = m_fabricCount->value();
    m_cross.borderMarginMm = m_borderMargin->value();
    m_cross.extraStitchesPerColor = m_extraStitches->value();
    m_cross.coveragePct = m_coverage->value();

    m_cross.maxColors = m_maxColors->value();
    m_cross.symbolSet = m_symbolSet->currentIndex();
    m_cross.cleanup = m_cleanup->currentIndex();
    m_cross.rareColorPpt = m_rarePpt->value();
    m_cross.dither = m_dither->isChecked();

    m_cross.showSymbols = m_showSymbols->isChecked();
    m_cross.showGrid = m_showGrid->isChecked();
    m_cross.majorGridEvery = m_majorGrid->value();
    m_cross.plateCount = m_plateCount->value();
    m_cross.plateMarginCells = m_plateMargin->value();

    m_cross.showColorNameColumn = m_showColorNameCol->isChecked();
    m_cross.showStitchesColumn = m_showStitchesCol->isChecked();
    m_cross.showBeadsColumn = m_showBeadsCol->isChecked();
    m_cross.showThreadColumn = m_showThreadCol->isChecked();

    m_cross.keyPicker = m_keyPicker->keySequence().toString(QKeySequence::NativeText);
    m_cross.keyViewMax = m_keyViewMax->keySequence().toString(QKeySequence::NativeText);

    m_cross.sanitize();

    // 用量换算实时预览：1000 针需要几束线 / 几粒珠
    const int skeins = m_cross.skeinsFor(1000);
    m_flossPreview->setText(
        tr("At %1ct with %2 strands: 1 skein covers about %3 stitches. "
           "A colour used 1000 times needs %4 skein(s) and %5 bead(s).")
            .arg(m_cross.fabricCount)
            .arg(m_cross.strands)
            .arg(m_cross.skeinsFor(1000) > 0
                     ? QString::number(qRound(
                           m_cross.skeinMeters * 1000.0 * (6.0 / m_cross.strands)
                           / m_cross.effectiveStitchLengthMm()))
                     : QStringLiteral("-"))
            .arg(skeins)
            .arg(m_cross.beadsFor(1000)));

    emit crossPreviewed(m_cross);
}

void SettingsDialog::syncCrossWidgets()
{
    m_stitchLen->setValue(m_cross.stitchLengthMm);
    m_strands->setValue(m_cross.strands);
    m_skeinMeters->setValue(m_cross.skeinMeters);
    m_flossMargin->setValue(m_cross.flossMarginPct);

    m_beadMargin->setValue(m_cross.beadMarginPct);
    m_beadSize->setValue(m_cross.beadSizeMm);
    m_countBeads->setChecked(m_cross.countBeads);

    m_fabricCount->setValue(m_cross.fabricCount);
    m_borderMargin->setValue(m_cross.borderMarginMm);
    m_extraStitches->setValue(m_cross.extraStitchesPerColor);
    m_coverage->setValue(m_cross.coveragePct);

    m_maxColors->setValue(m_cross.maxColors);
    m_symbolSet->setCurrentIndex(m_cross.symbolSet);
    m_cleanup->setCurrentIndex(m_cross.cleanup);
    m_rarePpt->setValue(m_cross.rareColorPpt);
    m_dither->setChecked(m_cross.dither);

    m_showSymbols->setChecked(m_cross.showSymbols);
    m_showGrid->setChecked(m_cross.showGrid);
    m_majorGrid->setValue(m_cross.majorGridEvery);
    m_plateCount->setValue(m_cross.plateCount);
    m_plateMargin->setValue(m_cross.plateMarginCells);

    m_showColorNameCol->setChecked(m_cross.showColorNameColumn);
    m_showStitchesCol->setChecked(m_cross.showStitchesColumn);
    m_showBeadsCol->setChecked(m_cross.showBeadsColumn);
    m_showThreadCol->setChecked(m_cross.showThreadColumn);

    m_keyPicker->setKeySequence(QKeySequence(m_cross.keyPicker));
    m_keyViewMax->setKeySequence(QKeySequence(m_cross.keyViewMax));
}
