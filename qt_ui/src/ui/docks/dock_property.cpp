#include "dock_property.h"

#include "ui/i18n/tr_manager.h"
#include "ffi_bind/rust_api.h"
#include "project/project_doc.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {
constexpr int kMaxGridSide = 9999;
constexpr int kMinFabricCount = 6;
constexpr int kMaxFabricCount = 32;
constexpr double kPerlerDefaultBeadMm = 5.0;  // 拼豆标准 5mm（Midi）
constexpr int kMaxDmcColors = 203;    // DMC 绣线色卡条目数
constexpr int kMaxPerlerColors = 159; // Artkal S 拼豆色卡条目数

/// 后台一次生成三种模式图纸，返回三份裸句柄（QFuture 结果要求可拷贝）。
struct GenResult
{
    void* charts[3] = {nullptr, nullptr, nullptr};
};
} // namespace

/// 自适应图片框：始终保持整图等比缩放到可视区，居中显示。
class ImagePreviewLabel : public QLabel
{
public:
    using QLabel::QLabel;

    void setPreviewImage(const QImage& image)
    {
        m_pixmap = QPixmap::fromImage(image);
        update();
    }

    void clearPreview()
    {
        m_pixmap = QPixmap();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        if (m_pixmap.isNull()) {
            p.setPen(palette().color(foregroundRole()));
            p.drawText(rect(), Qt::AlignCenter, text());
            return;
        }
        const QSize fit = m_pixmap.size().scaled(rect().size(), Qt::KeepAspectRatio);
        const QRect target((rect().width() - fit.width()) / 2,
                           (rect().height() - fit.height()) / 2,
                           fit.width(), fit.height());
        p.drawPixmap(target, m_pixmap);
    }

private:
    QPixmap m_pixmap;
};

DockProperty::DockProperty(ProjectDoc* project, ads::CDockManager* manager, QWidget* parent)
    : ads::CDockWidget(manager, QString(), parent)
    , m_project(project)
{
    setObjectName(QStringLiteral("DockProperty"));
    buildUi();

    // 语言切换 → 面板内部完整重译（标题由主窗口设置）
    connect(&TrManager::instance(), &TrManager::languageChanged,
            this, &DockProperty::retranslateUi);

    m_watcher = new QFutureWatcher<void*>(this);
    connect(m_watcher, &QFutureWatcher<void*>::finished, this, [this]() {
        // 后台线程返回三份裸句柄（0 十字绣 / 1 珠绣 / 2 拼豆）
        GenResult* result = static_cast<GenResult*>(m_watcher->result());
        bool anyValid = false;
        for (size_t i = 0; i < m_modeCharts.charts.size(); ++i) {
            m_modeCharts.charts[i] = result->charts[i];
            anyValid = anyValid || result->charts[i] != nullptr;
        }
        delete result;
        setBusy(false);
        if (!anyValid) {
            emit statusMessage(tr("Generation failed"), 0);
            return;
        }
        m_hasModeCharts = true;
        m_modeCharts.params = m_modeParams;
        m_modeCharts.options = m_modeOptions;
        updateSizeReadout();
        emit chartsGenerated(m_pendingTarget);
        emit statusMessage(tr("Chart generated (3 craft modes)"), 3000);
    });

    const CmCrossStitchParams defaults = rust_api::defaultCrossStitchParams();
    m_aspectRatio = defaults.grid_height > 0
        ? static_cast<double>(defaults.grid_width) / static_cast<double>(defaults.grid_height)
        : 1.0;
}

// ------------------------------------------------------------
// UI 构建
// ------------------------------------------------------------

void DockProperty::buildUi()
{
    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    layout->addWidget(buildSourceGroup());
    layout->addWidget(buildSizeGroup());
    layout->addWidget(buildColorGroup());
    layout->addWidget(buildProcessGroup());
    layout->addWidget(buildGenerateGroup());
    layout->addStretch();

    setWidget(root);
    applySizeModeUi();
    updateSizeReadout();
}

QGroupBox* DockProperty::buildSourceGroup()
{
    m_sourceBox = new QGroupBox(tr("Source image"));
    auto* layout = new QVBoxLayout(m_sourceBox);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // 图片框：预览缩略图（导入后整图等比自适应显示）
    m_imagePreview = new ImagePreviewLabel(m_sourceBox);
    m_imagePreview->setObjectName(QStringLiteral("imagePreview"));
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->setMinimumHeight(140);
    m_imagePreview->setMaximumHeight(220);
    m_imagePreview->setText(tr("No image imported"));

    m_imageInfoLabel = new QLabel(tr("Drag in or import an image to begin"), m_sourceBox);
    m_imageInfoLabel->setWordWrap(true);
    m_imageInfoLabel->setObjectName(QStringLiteral("imageInfoLabel"));
    m_imageInfoLabel->setAlignment(Qt::AlignCenter);

    m_importButton = new QPushButton(tr("Import image..."), m_sourceBox);
    m_importButton->setObjectName(QStringLiteral("importButton"));
    connect(m_importButton, &QPushButton::clicked, this, &DockProperty::onImportImage);

    layout->addWidget(m_imagePreview);
    layout->addWidget(m_imageInfoLabel);
    layout->addWidget(m_importButton);
    return m_sourceBox;
}

QGroupBox* DockProperty::buildSizeGroup()
{
    const CmCrossStitchParams defaults = rust_api::defaultCrossStitchParams();
    m_sizeBox = new QGroupBox(tr("Chart size"));
    auto* form = new QFormLayout(m_sizeBox);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_sizeForm = form;

    // 单一计算方式：按网格数 或 按成品尺寸（单选框，二选一）
    m_sizeByGridRadio = new QRadioButton(tr("Fixed grid"), m_sizeBox);
    m_sizeByGridRadio->setObjectName(QStringLiteral("sizeByGridRadio"));
    m_sizeByGridRadio->setChecked(true);
    m_sizeByTargetRadio = new QRadioButton(tr("By finished size"), m_sizeBox);
    m_sizeByTargetRadio->setObjectName(QStringLiteral("sizeByTargetRadio"));
    m_sizeByTargetRadio->setChecked(false);
    m_sizeByGridRadio->setToolTip(
        tr("Choose ONE way to size the chart:\n"
           "Fixed grid — set the stitch/bead count, read the finished size.\n"
           "By finished size — set the physical size, derive the grid automatically."));
    m_sizeByTargetRadio->setToolTip(m_sizeByGridRadio->toolTip());

    auto* sizeModeRow = new QWidget(m_sizeBox);
    auto* sizeModeLayout = new QHBoxLayout(sizeModeRow);
    sizeModeLayout->setContentsMargins(0, 0, 0, 0);
    sizeModeLayout->setSpacing(8);
    sizeModeLayout->addWidget(m_sizeByGridRadio);
    sizeModeLayout->addWidget(m_sizeByTargetRadio);
    sizeModeLayout->addStretch();
    form->addRow(QString(), sizeModeRow);
    m_sizeModeRowIdx = form->rowCount() - 1;

    connect(m_sizeByGridRadio, &QRadioButton::toggled, this, [this](bool on) {
        if (on) {
            applySizeModeUi();
            updateSizeReadout();
        }
    });
    connect(m_sizeByTargetRadio, &QRadioButton::toggled, this, [this](bool on) {
        if (on) {
            applySizeModeUi();
            onSizeToGrid();
        }
    });

    // 网格宽 × 高 + 锁定宽高比
    m_gridWidthSpin = new QSpinBox(m_sizeBox);
    m_gridWidthSpin->setRange(1, kMaxGridSide);
    m_gridWidthSpin->setValue(static_cast<int>(defaults.grid_width));
    m_gridWidthSpin->setSuffix(QStringLiteral(" "));
    m_gridWidthSpin->setObjectName(QStringLiteral("gridWidthSpin"));
    m_gridWidthSpin->setMaximumWidth(96);

    m_gridHeightSpin = new QSpinBox(m_sizeBox);
    m_gridHeightSpin->setRange(1, kMaxGridSide);
    m_gridHeightSpin->setValue(static_cast<int>(defaults.grid_height));
    m_gridHeightSpin->setObjectName(QStringLiteral("gridHeightSpin"));
    m_gridHeightSpin->setMaximumWidth(96);

    m_lockAspectButton = new QToolButton(m_sizeBox);
    m_lockAspectButton->setText(QStringLiteral("1:1"));
    m_lockAspectButton->setCheckable(true);
    m_lockAspectButton->setChecked(m_lockAspect);
    m_lockAspectButton->setToolTip(
        tr("Lock aspect ratio: height follows width, preserving the source image proportion"));
    m_lockAspectButton->setObjectName(QStringLiteral("lockAspectButton"));
    connect(m_lockAspectButton, &QToolButton::toggled, this, [this](bool checked) {
        m_lockAspect = checked;
        if (checked && m_gridWidthSpin->value() > 0) {
            m_aspectRatio = static_cast<double>(m_gridHeightSpin->value())
                / static_cast<double>(m_gridWidthSpin->value());
        }
    });

    connect(m_gridWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DockProperty::onGridWidthChanged);
    connect(m_gridWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateSizeReadout(); });
    connect(m_gridHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateSizeReadout(); });

    auto* gridRow = new QWidget(m_sizeBox);
    auto* gridLayout = new QHBoxLayout(gridRow);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(4);
    gridLayout->addWidget(m_gridWidthSpin);
    gridLayout->addWidget(new QLabel(QStringLiteral("\u00d7"), gridRow));
    gridLayout->addWidget(m_gridHeightSpin);
    gridLayout->addStretch();
    form->addRow(tr("Grid"), gridRow);
    m_gridRowIdx = form->rowCount() - 1;

    // 面料计数 + 锁定按钮同一行（1:1 移到输入框右侧，节省网格行宽度）
    m_fabricCountSpin = new QSpinBox(m_sizeBox);
    m_fabricCountSpin->setRange(kMinFabricCount, kMaxFabricCount);
    m_fabricCountSpin->setValue(static_cast<int>(defaults.fabric_count));
    m_fabricCountSpin->setSuffix(tr(" ct"));
    m_fabricCountSpin->setToolTip(
        tr("Fabric count (holes per inch). Higher count = smaller stitches."));
    m_fabricCountSpin->setObjectName(QStringLiteral("fabricCountSpin"));
    m_fabricCountSpin->setMaximumWidth(96);
    connect(m_fabricCountSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateSizeReadout(); });

    auto* dimRow = new QWidget(m_sizeBox);
    auto* dimLayout = new QHBoxLayout(dimRow);
    dimLayout->setContentsMargins(0, 0, 0, 0);
    dimLayout->setSpacing(4);
    dimLayout->addWidget(m_fabricCountSpin);
    dimLayout->addWidget(m_lockAspectButton);
    dimLayout->addStretch();
    form->addRow(tr("Fabric"), dimRow);
    m_dimRowIdx = form->rowCount() - 1;

    // 换算读数（按网格数 → 成品尺寸；按成品尺寸 → 网格数）
    m_sizeReadoutLabel = new QLabel(QStringLiteral("-"), m_sizeBox);
    m_sizeReadoutLabel->setObjectName(QStringLiteral("sizeReadoutLabel"));
    form->addRow(tr("Result"), m_sizeReadoutLabel);
    m_sizeReadoutRowIdx = form->rowCount() - 1;

    // 按成品尺寸反算网格（无应用按钮：输入即自动换算）
    m_targetWidthSpin = new QSpinBox(m_sizeBox);
    m_targetWidthSpin->setRange(1, 10000);
    m_targetWidthSpin->setSuffix(tr(" mm"));
    m_targetWidthSpin->setValue(200);
    m_targetWidthSpin->setMaximumWidth(96);
    m_targetHeightSpin = new QSpinBox(m_sizeBox);
    m_targetHeightSpin->setRange(1, 10000);
    m_targetHeightSpin->setSuffix(tr(" mm"));
    m_targetHeightSpin->setValue(150);
    m_targetHeightSpin->setMaximumWidth(96);
    connect(m_targetWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { onSizeToGrid(); });
    connect(m_targetHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { onSizeToGrid(); });

    auto* sizeRow = new QWidget(m_sizeBox);
    auto* sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(4);
    sizeLayout->addWidget(m_targetWidthSpin);
    sizeLayout->addWidget(new QLabel(QStringLiteral("\u00d7"), sizeRow));
    sizeLayout->addWidget(m_targetHeightSpin);
    sizeLayout->addStretch();
    form->addRow(tr("Target"), sizeRow);
    m_targetRowIdx = form->rowCount() - 1;

    return m_sizeBox;
}

QGroupBox* DockProperty::buildColorGroup()
{
    const CmCrossStitchParams defaults = rust_api::defaultCrossStitchParams();
    m_colorBox = new QGroupBox(tr("Color"));
    auto* form = new QFormLayout(m_colorBox);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_colorForm = form;

    // 色卡提示：三种模式的材料一览（十字绣=绣线，珠绣=珠子，拼豆=拼豆）
    m_cardHintLabel = new QLabel(tr("Floss / Beads / Perler"), m_colorBox);
    m_cardHintLabel->setObjectName(QStringLiteral("cardHintLabel"));
    m_cardHintLabel->setWordWrap(true);
    form->addRow(tr("Palette"), m_cardHintLabel);

    m_maxColorsSpin = new QSpinBox(m_colorBox);
    m_maxColorsSpin->setRange(1, kMaxDmcColors);
    m_maxColorsSpin->setValue(static_cast<int>(defaults.max_colors));
    m_maxColorsSpin->setObjectName(QStringLiteral("maxColorsSpin"));
    m_maxColorsSpin->setMaximumWidth(84);
    connect(m_maxColorsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateSizeReadout(); });
    form->addRow(tr("Max colors"), m_maxColorsSpin);

    m_sampleModeCombo = new QComboBox(m_colorBox);
    m_sampleModeCombo->addItem(tr("Box average"), static_cast<int>(cm::sample::MEAN));
    m_sampleModeCombo->addItem(tr("Dominant color (mode)"), static_cast<int>(cm::sample::MODE));
    m_sampleModeCombo->addItem(tr("Medoid color"), static_cast<int>(cm::sample::MEDOID));
    m_sampleModeCombo->setCurrentIndex(1); // 默认众数：保留实色
    m_sampleModeCombo->setToolTip(
        tr("How source pixels are merged into one stitch.\n"
           "Dominant color keeps flat color areas crisp; m_colorBox average blends and may invent\n"
           "colors that exist neither in the photo nor in the palette."));
    m_sampleModeCombo->setObjectName(QStringLiteral("sampleModeCombo"));
    m_sampleModeCombo->setMaximumWidth(180);
    form->addRow(tr("Sampling"), m_sampleModeCombo);

    m_ditherCheck = new QCheckBox(tr("Floyd-Steinberg dithering"), m_colorBox);
    m_ditherCheck->setChecked(false);
    m_ditherCheck->setObjectName(QStringLiteral("ditherCheck"));
    form->addRow(QString(), m_ditherCheck);

    return m_colorBox;
}

QGroupBox* DockProperty::buildProcessGroup()
{
    // 符号与去杂点合一：全部作为生成参数一次生效，无独立按钮
    m_processBox = new QGroupBox(tr("Symbol & cleanup"));
    auto* form = new QFormLayout(m_processBox);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_processForm = form;

    m_symbolSetCombo = new QComboBox(m_processBox);
    m_symbolSetCombo->addItem(tr("Letters (A-Z)"), static_cast<int>(cm::symbol::LETTERS));
    m_symbolSetCombo->addItem(tr("Digits (1-9)"), static_cast<int>(cm::symbol::DIGITS));
    m_symbolSetCombo->addItem(tr("Geometric"), static_cast<int>(cm::symbol::GEOMETRIC));
    m_symbolSetCombo->addItem(tr("Mixed"), static_cast<int>(cm::symbol::MIXED));
    m_symbolSetCombo->setToolTip(
        tr("Symbols are assigned by usage: the most used color gets the clearest symbol."));
    m_symbolSetCombo->setObjectName(QStringLiteral("symbolSetCombo"));
    m_symbolSetCombo->setMaximumWidth(180);
    form->addRow(tr("Symbol set"), m_symbolSetCombo);

    m_cleanupCombo = new QComboBox(m_processBox);
    m_cleanupCombo->addItem(tr("Off"), static_cast<int>(cm::cleanup::OFF));
    m_cleanupCombo->addItem(tr("Light (single stitches)"), static_cast<int>(cm::cleanup::LIGHT));
    m_cleanupCombo->addItem(tr("Strong (+ tiny fragments)"), static_cast<int>(cm::cleanup::STRONG));
    m_cleanupCombo->setCurrentIndex(1);
    m_cleanupCombo->setToolTip(
        tr("Merge stray stitches that have no same-colour neighbour into the surrounding colour"));
    m_cleanupCombo->setObjectName(QStringLiteral("cleanupCombo"));
    m_cleanupCombo->setMaximumWidth(180);
    form->addRow(tr("Cleanup"), m_cleanupCombo);

    m_rareColorSpin = new QSpinBox(m_processBox);
    m_rareColorSpin->setRange(0, 500);
    m_rareColorSpin->setValue(3);
    m_rareColorSpin->setSuffix(QStringLiteral(" \u2030"));
    m_rareColorSpin->setMaximumWidth(84);
    m_rareColorSpin->setToolTip(
        tr("Merge colours used by fewer cells than this share (per-mille) into the nearest "
           "major colour"));
    m_rareColorSpin->setObjectName(QStringLiteral("rareColorSpin"));
    form->addRow(tr("Rare colors"), m_rareColorSpin);

    return m_processBox;
}

QGroupBox* DockProperty::buildGenerateGroup()
{
    m_generateBox = new QGroupBox(tr("Generate"));
    auto* layout = new QVBoxLayout(m_generateBox);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // 两个生成按钮：目标盘 = 当前盘 / 新盘（一盘一图）
    auto* btnRow = new QWidget(m_generateBox);
    auto* btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(4);

    m_generateButton = new QPushButton(tr("Current plate"), btnRow);
    m_generateButton->setObjectName(QStringLiteral("generateButton"));
    m_generateButton->setToolTip(tr("Generate the chart into the active plate"));
    m_generateButton->setEnabled(false);
    connect(m_generateButton, &QPushButton::clicked,
            this, &DockProperty::onGenerateToCurrentPlate);

    m_newPlateButton = new QPushButton(tr("New plate"), btnRow);
    m_newPlateButton->setObjectName(QStringLiteral("generateNewPlateButton"));
    m_newPlateButton->setToolTip(tr("Add a plate and generate the chart into it"));
    m_newPlateButton->setEnabled(false);
    connect(m_newPlateButton, &QPushButton::clicked,
            this, &DockProperty::onGenerateToNewPlate);

    btnLayout->addWidget(m_generateButton);
    btnLayout->addWidget(m_newPlateButton);
    layout->addWidget(btnRow);

    m_progressBar = new QProgressBar(m_generateBox);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setObjectName(QStringLiteral("generateProgress"));

    layout->addWidget(m_progressBar);
    return m_generateBox;
}

// ------------------------------------------------------------
// 行为
// ------------------------------------------------------------

void DockProperty::syncFromProject(const CmCrossStitchParams& params,
                                   const CmStitchOptions& options,
                                   const CmFabricSize& size,
                                   bool hasChart)
{
    QSignalBlocker blockW(m_gridWidthSpin);
    QSignalBlocker blockH(m_gridHeightSpin);
    QSignalBlocker blockF(m_fabricCountSpin);
    QSignalBlocker blockC(m_maxColorsSpin);
    QSignalBlocker blockS(m_symbolSetCombo);
    QSignalBlocker blockM(m_sampleModeCombo);
    QSignalBlocker blockCl(m_cleanupCombo);
    QSignalBlocker blockR(m_rareColorSpin);
    QSignalBlocker blockD(m_ditherCheck);

    m_lockAspect = false;
    m_gridWidthSpin->setValue(static_cast<int>(params.grid_width));
    m_gridHeightSpin->setValue(static_cast<int>(params.grid_height));
    m_lockAspect = true;

    m_fabricCountSpin->setValue(static_cast<int>(params.fabric_count));
    m_maxColorsSpin->setValue(static_cast<int>(params.max_colors));
    m_ditherCheck->setChecked(params.dither != 0);

    const int symbolIdx = m_symbolSetCombo->findData(static_cast<int>(options.symbol_set));
    if (symbolIdx >= 0) {
        m_symbolSetCombo->setCurrentIndex(symbolIdx);
    }
    const int sampleIdx = m_sampleModeCombo->findData(static_cast<int>(options.sample_mode));
    if (sampleIdx >= 0) {
        m_sampleModeCombo->setCurrentIndex(sampleIdx);
    }
    const int cleanupIdx = m_cleanupCombo->findData(static_cast<int>(options.cleanup));
    if (cleanupIdx >= 0) {
        m_cleanupCombo->setCurrentIndex(cleanupIdx);
    }
    m_rareColorSpin->setValue(static_cast<int>(options.rare_color_ppt));

    applySizeModeUi();

    if (hasChart) {
        m_sizeReadoutLabel->setText(QStringLiteral("%1 \u00d7 %2 mm")
                                        .arg(size.width_mm)
                                        .arg(size.height_mm));
    } else {
        updateSizeReadout();
    }
}

double DockProperty::cellMm() const
{
    const int fc = m_fabricCountSpin ? m_fabricCountSpin->value() : kMinFabricCount;
    return 25.4 / qBound(kMinFabricCount, fc, kMaxFabricCount);
}

void DockProperty::applySizeModeUi()
{
    if (!m_sizeForm) {
        return;
    }
    const bool byGrid = !m_sizeByTargetRadio || !m_sizeByTargetRadio->isChecked();
    m_sizeForm->setRowVisible(m_gridRowIdx, byGrid);
    m_sizeForm->setRowVisible(m_dimRowIdx, byGrid);
    m_sizeForm->setRowVisible(m_sizeReadoutRowIdx, byGrid);
    m_sizeForm->setRowVisible(m_targetRowIdx, !byGrid);
}

CmCrossStitchParams DockProperty::collectParams() const
{
    CmCrossStitchParams params = rust_api::defaultCrossStitchParams();
    params.grid_width = static_cast<uint32_t>(m_gridWidthSpin->value());
    params.grid_height = static_cast<uint32_t>(m_gridHeightSpin->value());
    params.max_colors = static_cast<uint32_t>(m_maxColorsSpin->value());
    params.fabric_count = static_cast<uint32_t>(m_fabricCountSpin->value());
    params.dither = m_ditherCheck->isChecked() ? 1u : 0u;
    return params;
}

CmStitchOptions DockProperty::collectOptions() const
{
    CmStitchOptions options = rust_api::defaultStitchOptions();
    options.symbol_set = m_symbolSetCombo->currentData().toUInt();
    options.sample_mode = m_sampleModeCombo->currentData().toUInt();
    options.cleanup = m_cleanupCombo->currentData().toUInt();
    options.rare_color_ppt = static_cast<uint32_t>(m_rareColorSpin->value());
    return options;
}

void DockProperty::onImportImage()
{
    // 默认打开桌面：图片素材通常先存到桌面，符合导入场景的直觉
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import image"), dir,
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    QImage image(path);
    if (image.isNull()) {
        emit statusMessage(tr("Cannot open image: %1").arg(QFileInfo(path).fileName()), 5000);
        return;
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);
    m_sourceImage = image;
    m_generateButton->setEnabled(true);
    m_newPlateButton->setEnabled(true);

    const QFileInfo info(path);
    m_imageInfoLabel->setText(QStringLiteral("%1  \u00b7  %2 \u00d7 %3")
                                  .arg(info.fileName())
                                  .arg(image.width())
                                  .arg(image.height()));

    // 预览：整图等比自适应显示全
    // 注：ImagePreviewLabel 未加 Q_OBJECT（无需信号槽），故用 static_cast 而非
    // qobject_cast（后者要求目标类型有 Q_OBJECT 宏，否则编译期 static_assert 失败）。
    if (auto* preview = static_cast<ImagePreviewLabel*>(m_imagePreview)) {
        preview->setPreviewImage(image);
    }

    // 默认网格：保持源图宽高比，默认 100 格宽（宽度优先）
    const double ratio = static_cast<double>(image.height()) / static_cast<double>(image.width());
    m_aspectRatio = ratio;
    {
        QSignalBlocker blockH(m_gridHeightSpin);
        m_gridWidthSpin->setValue(100);
        m_gridHeightSpin->setValue(qMax(1, qRound(100 * ratio)));
    }
    updateSizeReadout();
    emit statusMessage(tr("Image imported: %1").arg(info.fileName()), 3000);
}

void DockProperty::onGridWidthChanged(int value)
{
    if (!m_lockAspect) {
        return;
    }
    const int h = qMax(1, qRound(value * m_aspectRatio));
    QSignalBlocker block(m_gridHeightSpin);
    m_gridHeightSpin->setValue(h);
}

void DockProperty::onSizeToGrid()
{
    // 仅「按成品尺寸」口径下自动换算（网格口径下目标框隐藏，不参与）
    if (!m_sizeByTargetRadio || !m_sizeByTargetRadio->isChecked()) {
        return;
    }
    const double c = cellMm();
    if (c <= 0.0) {
        return;
    }
    const int gw = qMax(1, qRound(m_targetWidthSpin->value() / c));
    const int gh = qMax(1, qRound(m_targetHeightSpin->value() / c));
    {
        QSignalBlocker blockW(m_gridWidthSpin);
        QSignalBlocker blockH(m_gridHeightSpin);
        m_gridWidthSpin->setValue(gw);
        m_gridHeightSpin->setValue(gh);
    }
    m_aspectRatio = static_cast<double>(gh) / static_cast<double>(gw);
    updateSizeReadout();
}

void DockProperty::updateSizeReadout()
{
    if (!m_sizeReadoutLabel) {
        return;
    }
    const double c = cellMm();
    const bool byGrid = !m_sizeByTargetRadio || !m_sizeByTargetRadio->isChecked();
    if (byGrid) {
        const int mmW = qRound(m_gridWidthSpin->value() * c);
        const int mmH = qRound(m_gridHeightSpin->value() * c);
        m_sizeReadoutLabel->setText(QStringLiteral("%1 \u00d7 %2 mm").arg(mmW).arg(mmH));
    } else {
        const int gw = qMax(1, qRound(m_targetWidthSpin->value() / c));
        const int gh = qMax(1, qRound(m_targetHeightSpin->value() / c));
        m_sizeReadoutLabel->setText(QStringLiteral("%1 \u00d7 %2 cells").arg(gw).arg(gh));
    }
}

void DockProperty::setBusy(bool busy)
{
    const bool canGenerate = !busy && !m_sourceImage.isNull();
    if (m_generateButton) {
        m_generateButton->setEnabled(canGenerate);
    }
    if (m_newPlateButton) {
        m_newPlateButton->setEnabled(canGenerate);
    }
    if (m_progressBar) {
        m_progressBar->setEnabled(busy);
        if (!busy) {
            m_progressBar->setValue(0);
        }
    }
    setCursor(busy ? Qt::BusyCursor : Qt::ArrowCursor);
}

void DockProperty::onGenerateToCurrentPlate()
{
    m_pendingTarget = m_currentPlateHint;
    onGenerate();
}

void DockProperty::onGenerateToNewPlate()
{
    m_pendingTarget = -1; // 主窗口收到后盘数 +1 并生成到新盘
    onGenerate();
}

void DockProperty::onGenerate()
{
    if (m_sourceImage.isNull() || !m_project) {
        emit statusMessage(tr("Import an image first"), 3000);
        return;
    }
    // 生成守卫（主窗口注入）：目标盘锁定 → 拦截；盘上已有图纸 → 先问是否覆盖
    if (m_generateGuard && !m_generateGuard(m_pendingTarget)) {
        return;
    }
    setBusy(true);

    // 生成三种手作品类图纸：十字绣（DMC）/ 珠绣（DMC 珠色卡）/ 拼豆（Artkal S 色卡）
    const CmCrossStitchParams baseParams = collectParams();
    const CmStitchOptions baseOptions = collectOptions();
    const QImage image = m_sourceImage;

    std::array<CmCrossStitchParams, 3> params;
    std::array<CmStitchOptions, 3> options;
    for (size_t mode = 0; mode < params.size(); ++mode) {
        params[mode] = baseParams;
        options[mode] = baseOptions;
        // 每格一珠/一豆；最大颜色数按各自色卡收敛
        params[mode].bead_mode = mode == cm::craft::CROSS_STITCH ? 0u : 1u;
        options[mode].craft_mode = static_cast<uint32_t>(mode);
        const int cardMax = mode == cm::craft::PERLER ? kMaxPerlerColors : kMaxDmcColors;
        params[mode].max_colors =
            static_cast<uint32_t>(qMin(static_cast<int>(baseParams.max_colors), cardMax));
    }
    m_modeParams = params;
    m_modeOptions = options;

    // 后台线程一次生成三份裸句柄（QFuture 结果要求可拷贝，故统一为 void*）
    auto future = QtConcurrent::run(
        [image, params, options]() -> void* {
            auto* result = new GenResult();
            for (size_t mode = 0; mode < params.size(); ++mode) {
                int err = cm::codes::OK;
                rust_api::ChartHandle chart =
                    rust_api::generateChart(image, params[mode], options[mode], nullptr, &err);
                result->charts[mode] = chart.release();
            }
            return static_cast<void*>(result);
        });
    m_watcher->setFuture(future);
}

DockProperty::~DockProperty()
{
    // 未被主窗口取走的生成结果在此释放（正常流程 chartsGenerated 后立即取走）
    for (void* raw : m_modeCharts.charts) {
        if (raw) {
            cm_chart_free(raw);
        }
    }
}

DockProperty::ModeChartsResult DockProperty::takeModeCharts()
{
    ModeChartsResult result;
    result.charts = m_modeCharts.charts;
    result.params = m_modeParams;
    result.options = m_modeOptions;
    for (auto& raw : m_modeCharts.charts) {
        raw = nullptr;
    }
    m_hasModeCharts = false;
    return result;
}

void DockProperty::retranslateUi()
{
    setWindowTitle(tr("Chart wizard"));

    // —— 源图组 ——
    if (m_sourceBox) {
        m_sourceBox->setTitle(tr("Source image"));
    }
    if (m_imagePreview) {
        m_imagePreview->setText(tr("No image imported"));
    }
    if (m_imageInfoLabel) {
        m_imageInfoLabel->setText(tr("Drag in or import an image to begin"));
    }
    if (m_importButton) {
        m_importButton->setText(tr("Import image..."));
    }

    // —— 尺寸组（组标题 / 单选 / QForm 行标签 / 后缀 / tooltip）——
    auto formLabel = [](QFormLayout* form, int row) -> QLabel* {
        if (!form || row < 0 || row >= form->rowCount()) {
            return nullptr;
        }
        auto* item = form->itemAt(row, QFormLayout::LabelRole);
        return item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
    };
    if (m_sizeBox) {
        m_sizeBox->setTitle(tr("Chart size"));
        if (m_sizeForm) {
            if (auto* l = formLabel(m_sizeForm, m_gridRowIdx)) {
                l->setText(tr("Grid"));
            }
            if (auto* l = formLabel(m_sizeForm, m_dimRowIdx)) {
                l->setText(tr("Fabric"));
            }
            if (auto* l = formLabel(m_sizeForm, m_sizeReadoutRowIdx)) {
                l->setText(tr("Result"));
            }
            if (auto* l = formLabel(m_sizeForm, m_targetRowIdx)) {
                l->setText(tr("Target"));
            }
        }
    }
    if (m_sizeByGridRadio && m_sizeByTargetRadio) {
        m_sizeByGridRadio->setText(tr("Fixed grid"));
        m_sizeByTargetRadio->setText(tr("By finished size"));
        const QString tip = tr(
            "Choose ONE way to size the chart:\n"
            "Fixed grid — set the stitch/bead count, read the finished size.\n"
            "By finished size — set the physical size, derive the grid automatically.");
        m_sizeByGridRadio->setToolTip(tip);
        m_sizeByTargetRadio->setToolTip(tip);
        m_sizeByGridRadio->setChecked(m_sizeByGridRadio->isChecked());
    }
    if (m_lockAspectButton) {
        m_lockAspectButton->setToolTip(
            tr("Lock aspect ratio: height follows width, preserving the source image proportion"));
    }
    if (m_fabricCountSpin) {
        m_fabricCountSpin->setSuffix(tr(" ct"));
        m_fabricCountSpin->setToolTip(
            tr("Fabric count (holes per inch). Higher count = smaller stitches."));
    }
    if (m_targetWidthSpin) {
        m_targetWidthSpin->setSuffix(tr(" mm"));
    }
    if (m_targetHeightSpin) {
        m_targetHeightSpin->setSuffix(tr(" mm"));
    }
    updateSizeReadout(); // 成品尺寸读数文案随语言重建

    // —— 颜色组 ——
    if (m_colorBox) {
        m_colorBox->setTitle(tr("Color"));
        if (m_colorForm) {
            if (auto* l = formLabel(m_colorForm, 0)) {
                l->setText(tr("Palette"));
            }
            if (auto* l = formLabel(m_colorForm, 1)) {
                l->setText(tr("Max colors"));
            }
            if (auto* l = formLabel(m_colorForm, 2)) {
                l->setText(tr("Sampling"));
            }
        }
    }
    if (m_cardHintLabel) {
        m_cardHintLabel->setText(tr("Floss / Beads / Perler"));
    }
    if (m_sampleModeCombo) {
        m_sampleModeCombo->setItemText(0, tr("Box average"));
        m_sampleModeCombo->setItemText(1, tr("Dominant color (mode)"));
        m_sampleModeCombo->setItemText(2, tr("Medoid color"));
        m_sampleModeCombo->setToolTip(
            tr("How source pixels are merged into one stitch.\n"
               "Dominant color keeps flat color areas crisp; box average blends and may invent\n"
               "colors that exist neither in the photo nor in the palette."));
    }
    if (m_ditherCheck) {
        m_ditherCheck->setText(tr("Floyd-Steinberg dithering"));
    }

    // —— 符号与去杂点组 ——
    if (m_processBox) {
        m_processBox->setTitle(tr("Symbol & cleanup"));
        if (m_processForm) {
            if (auto* l = formLabel(m_processForm, 0)) {
                l->setText(tr("Symbol set"));
            }
            if (auto* l = formLabel(m_processForm, 1)) {
                l->setText(tr("Cleanup"));
            }
            if (auto* l = formLabel(m_processForm, 2)) {
                l->setText(tr("Rare colors"));
            }
        }
    }
    if (m_symbolSetCombo) {
        m_symbolSetCombo->setItemText(0, tr("Letters (A-Z)"));
        m_symbolSetCombo->setItemText(1, tr("Digits (1-9)"));
        m_symbolSetCombo->setItemText(2, tr("Geometric"));
        m_symbolSetCombo->setItemText(3, tr("Mixed"));
        m_symbolSetCombo->setToolTip(
            tr("Symbols are assigned by usage: the most used color gets the clearest symbol."));
    }
    if (m_cleanupCombo) {
        m_cleanupCombo->setItemText(0, tr("Off"));
        m_cleanupCombo->setItemText(1, tr("Light (single stitches)"));
        m_cleanupCombo->setItemText(2, tr("Strong (+ tiny fragments)"));
        m_cleanupCombo->setToolTip(
            tr("Merge stray stitches that have no same-colour neighbour into the surrounding colour"));
    }
    if (m_rareColorSpin) {
        m_rareColorSpin->setToolTip(
            tr("Merge colours used by fewer cells than this share (per-mille) into the nearest "
               "major colour"));
    }

    // —— 生成组 ——
    if (m_generateBox) {
        m_generateBox->setTitle(tr("Generate"));
    }
    if (m_generateButton) {
        m_generateButton->setText(tr("Current plate"));
        m_generateButton->setToolTip(tr("Generate the chart into the active plate"));
    }
    if (m_newPlateButton) {
        m_newPlateButton->setText(tr("New plate"));
        m_newPlateButton->setToolTip(tr("Add a plate and generate the chart into it"));
    }
}
