#include "dock_params.h"

#include "ui/i18n/tr_manager.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QFontComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

DockParams::DockParams(ads::CDockManager* manager, QWidget* parent)
    : ads::CDockWidget(manager, tr("Transform"), parent)
{
    setObjectName(QStringLiteral("DockParams"));
    setWindowTitle(tr("Transform"));

    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // 镜像参数组
    m_mirrorBox = new QGroupBox(tr("Mirror"), widget);
    auto* vbox = new QVBoxLayout(m_mirrorBox);
    vbox->setSpacing(6);

    auto* mirrorH = new QPushButton(tr("Mirror left-right (whole plate)"), m_mirrorBox);
    mirrorH->setObjectName(QStringLiteral("param.mirror_h"));
    mirrorH->setToolTip(tr("Mirror the whole active plate left-right"));
    connect(mirrorH, &QPushButton::clicked, this,
            [this]() { emit mirrorWholeRequested(true); });
    vbox->addWidget(mirrorH);

    auto* mirrorV = new QPushButton(tr("Mirror up-down (whole plate)"), m_mirrorBox);
    mirrorV->setObjectName(QStringLiteral("param.mirror_v"));
    mirrorV->setToolTip(tr("Mirror the whole active plate up-down"));
    connect(mirrorV, &QPushButton::clicked, this,
            [this]() { emit mirrorWholeRequested(false); });
    vbox->addWidget(mirrorV);

    auto* customMirror = new QPushButton(tr("Custom mirror (2 points)"), m_mirrorBox);
    customMirror->setObjectName(QStringLiteral("param.mirror_custom"));
    customMirror->setCheckable(true);
    customMirror->setToolTip(
        tr("Click two points on the canvas; the line through them is the mirror axis"));
    vbox->addWidget(customMirror);

    auto* xyAxis = new QPushButton(tr("Custom axis (XY)"), m_mirrorBox);
    xyAxis->setObjectName(QStringLiteral("param.mirror_xy"));
    xyAxis->setCheckable(true);
    xyAxis->setToolTip(
        tr("Hover to highlight a thin full-canvas grid line, click to mirror along it"));
    vbox->addWidget(xyAxis);

    m_modeButtons = {customMirror, xyAxis};
    // 仅用户点击才发信号；程序静默置位（互斥 / 取消）不发，避免切换工具时连锁
    connect(customMirror, &QPushButton::toggled, this, [this](bool on) {
        if (!m_silent) {
            emit customMirrorToggled(on);
        }
    });
    connect(xyAxis, &QPushButton::toggled, this, [this](bool on) {
        if (!m_silent) {
            emit xyAxisToggled(on);
        }
    });

    layout->addWidget(m_mirrorBox);

    // 移动参数组：X 移动 / Y 移动（格子数，可负）+ 应用
    m_moveBox = new QGroupBox(tr("Move"), widget);
    auto* mv = new QVBoxLayout(m_moveBox);
    mv->setSpacing(6);

    auto* xSpin = new QSpinBox(m_moveBox);
    xSpin->setObjectName(QStringLiteral("param.move_x"));
    xSpin->setRange(-9999, 9999);
    xSpin->setValue(0);
    xSpin->setSuffix(tr(" cols"));
    xSpin->setToolTip(tr("Horizontal offset in cells (negative = left)"));
    auto* ySpin = new QSpinBox(m_moveBox);
    ySpin->setObjectName(QStringLiteral("param.move_y"));
    ySpin->setRange(-9999, 9999);
    ySpin->setValue(0);
    ySpin->setSuffix(tr(" rows"));
    ySpin->setToolTip(tr("Vertical offset in cells (negative = up)"));

    auto* moveRow1 = new QHBoxLayout();
    m_moveXLabel = new QLabel(tr("X"), m_moveBox);
    moveRow1->addWidget(m_moveXLabel);
    moveRow1->addWidget(xSpin);
    auto* moveRow2 = new QHBoxLayout();
    m_moveYLabel = new QLabel(tr("Y"), m_moveBox);
    moveRow2->addWidget(m_moveYLabel);
    moveRow2->addWidget(ySpin);
    mv->addLayout(moveRow1);
    mv->addLayout(moveRow2);

    auto* moveBtn = new QPushButton(tr("Apply move"), m_moveBox);
    moveBtn->setObjectName(QStringLiteral("param.move_apply"));
    moveBtn->setToolTip(tr("Move the selected content by the offsets above"));
    connect(moveBtn, &QPushButton::clicked, this, [this, xSpin, ySpin]() {
        emit moveRequested(xSpin->value(), ySpin->value());
    });
    mv->addWidget(moveBtn);

    layout->addWidget(m_moveBox);

    // 文字参数组（文字转豆子）：内容 / 字体 / 字高+旋转+粗体一行 / 放置+确认。
    // 画布点击格出预览，实时根据参数计算，确认后写入豆子格子；
    // 「放置」按钮在文字工具未激活时一键切到文字工具。
    m_textBox = new QGroupBox(tr("Text"), widget);
    auto* tb = new QVBoxLayout(m_textBox);
    tb->setSpacing(6);

    // 内容：多行输入框（换行输入多行文字；光栅化按行拆分排版）
    m_textContent = new QTextEdit(m_textBox);
    m_textContent->setObjectName(QStringLiteral("param.text_content"));
    m_textContent->setPlaceholderText(tr("Text to place\n(Enter starts a new line)"));
    m_textContent->setAcceptRichText(false);
    m_textContent->setFixedHeight(64);
    m_textContent->setToolTip(tr("This text is previewed at the clicked cell; Confirm writes beads"));
    tb->addWidget(m_textContent);

    m_textFont = new QFontComboBox(m_textBox);
    m_textFont->setObjectName(QStringLiteral("param.text_font"));
    // QFontComboBox 默认 AdjustToContents 会按最长字体名撑出 ~293px 的
    // 最小宽度，直接把整个变换面板撑宽；改为按最小内容长度收缩。
    m_textFont->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_textFont->setMinimumContentsLength(8);
    m_textFont->setToolTip(tr("Font used to rasterize the text into bead cells"));
    tb->addWidget(m_textFont);

    // 字高 / 旋转 / 粗体同一行（紧凑）
    auto* paramRow = new QHBoxLayout;
    paramRow->setSpacing(6);
    m_textHeight = new QSpinBox(m_textBox);
    m_textHeight->setObjectName(QStringLiteral("param.text_height"));
    m_textHeight->setRange(2, 64);
    m_textHeight->setValue(12);
    m_textHeight->setSuffix(tr(" cells"));
    m_textHeight->setToolTip(tr("Text height in beads (1 cell = 1 bead), per line"));
    paramRow->addWidget(m_textHeight);
    // 默认 sizeHint 偏宽（约 130px），挤压同行的粗体与下方按钮行；定宽收窄
    m_textHeight->setFixedWidth(88);

    m_textAngle = new QSpinBox(m_textBox);
    m_textAngle->setObjectName(QStringLiteral("param.text_angle"));
    // 全周角度：0–360（可回绕；内核渲染按 cos/sin 支持任意角度）
    m_textAngle->setRange(0, 360);
    m_textAngle->setWrapping(true);
    m_textAngle->setValue(0);
    m_textAngle->setSuffix(tr("°"));
    m_textAngle->setToolTip(tr("Rotation angle, clockwise positive"));
    paramRow->addWidget(m_textAngle);
    m_textAngle->setFixedWidth(88);

    m_textBold = new QCheckBox(tr("Bold"), m_textBox);
    m_textBold->setObjectName(QStringLiteral("param.text_bold"));
    paramRow->addWidget(m_textBold);
    paramRow->addStretch();
    tb->addLayout(paramRow);

    // 预览参数实时上报：任一输入变化 → 主窗口刷新画布预览
    connect(m_textContent, &QTextEdit::textChanged, this, [this]() {
        emit textParamsChanged();
    });
    connect(m_textFont, &QFontComboBox::currentFontChanged, this, [this]() {
        emit textParamsChanged();
    });
    connect(m_textHeight, qOverload<int>(&QSpinBox::valueChanged), this,
            [this]() { emit textParamsChanged(); });
    connect(m_textAngle, qOverload<int>(&QSpinBox::valueChanged), this,
            [this]() { emit textParamsChanged(); });
    connect(m_textBold, &QCheckBox::toggled, this, [this]() {
        emit textParamsChanged();
    });

    // 放置（切到文字工具准备点击画布）+ 确认生成（把预览写入豆子），同一行
    auto* actionRow = new QHBoxLayout;
    actionRow->setSpacing(6);
    auto* textPlace = new QPushButton(tr("Place"), m_textBox);
    textPlace->setObjectName(QStringLiteral("param.text_place"));
    textPlace->setToolTip(tr("Switch to the Text tool, then click a cell to preview"));
    connect(textPlace, &QPushButton::clicked, this, [this]() {
        emit textPlaceRequested();
    });
    actionRow->addWidget(textPlace);

    auto* textConfirm = new QPushButton(tr("Confirm"), m_textBox);
    textConfirm->setObjectName(QStringLiteral("param.text_confirm"));
    textConfirm->setToolTip(tr("Write the preview text into bead cells (Esc cancels the preview)"));
    connect(textConfirm, &QPushButton::clicked, this, [this]() {
        emit textConfirmRequested();
    });
    actionRow->addWidget(textConfirm);
    actionRow->addStretch();
    tb->addLayout(actionRow);

    layout->addWidget(m_textBox);

    layout->addStretch();
    setWidget(widget);

    // 语言切换 → 面板内部完整重译
    connect(&TrManager::instance(), &TrManager::languageChanged,
            this, &DockParams::retranslateUi);
}

void DockParams::retranslateUi()
{
    setWindowTitle(tr("Transform"));
    if (m_mirrorBox) {
        m_mirrorBox->setTitle(tr("Mirror"));
    }
    if (m_moveBox) {
        m_moveBox->setTitle(tr("Move"));
        if (m_moveXLabel) {
            m_moveXLabel->setText(tr("X"));
        }
        if (m_moveYLabel) {
            m_moveYLabel->setText(tr("Y"));
        }
    }
    if (m_textBox) {
        m_textBox->setTitle(tr("Text"));
    }

    // 文字组成员直接由成员指针重设
    m_textContent->setPlaceholderText(tr("Text to place\n(Enter starts a new line)"));
    m_textContent->setToolTip(tr("This text is previewed at the clicked cell; Confirm writes beads"));
    m_textFont->setToolTip(tr("Font used to rasterize the text into bead cells"));
    m_textHeight->setSuffix(tr(" cells"));
    m_textHeight->setToolTip(tr("Text height in beads (1 cell = 1 bead), per line"));
    m_textAngle->setSuffix(tr("°"));
    m_textAngle->setToolTip(tr("Rotation angle, clockwise positive"));
    m_textBold->setText(tr("Bold"));

    // 按钮类用 objectName 定位（局部变量，未存成员）
    auto resetButton = [](QWidget* scope, const char* objName, const QString& text,
                          const QString& tooltip) {
        if (!scope) {
            return;
        }
        for (QWidget* w : scope->findChildren<QWidget*>()) {
            if (w->objectName() == QLatin1String(objName)) {
                w->setProperty("text", text);
                w->setToolTip(tooltip);
                break;
            }
        }
    };
    resetButton(m_mirrorBox, "param.mirror_h", tr("Mirror left-right (whole plate)"),
                tr("Mirror the whole active plate left-right"));
    resetButton(m_mirrorBox, "param.mirror_v", tr("Mirror up-down (whole plate)"),
                tr("Mirror the whole active plate up-down"));
    resetButton(m_mirrorBox, "param.mirror_custom",
                tr("Custom mirror (2 points)"),
                tr("Click two points on the canvas; the line through them is the mirror axis"));
    resetButton(m_mirrorBox, "param.mirror_xy", tr("Custom axis (XY)"),
                tr("Hover to highlight a thin full-canvas grid line, click to mirror along it"));
    resetButton(m_moveBox, "param.move_apply", tr("Apply move"),
                tr("Move the selected content by the offsets above"));
    resetButton(m_textBox, "param.text_place", tr("Place"),
                tr("Switch to the Text tool, then click a cell to preview"));
    resetButton(m_textBox, "param.text_confirm", tr("Confirm"),
                tr("Write the preview text into bead cells (Esc cancels the preview)"));
}

DockParams::~DockParams() = default;

DockParams::TextStampParams DockParams::textStampParams() const
{
    TextStampParams params;
    if (m_textContent) {
        // 多行输入：逐行拆分（首尾空白仍去除，行内空格保留）
        params.text = m_textContent->toPlainText().trimmed();
    }
    if (m_textFont) {
        params.font = m_textFont->currentFont();
    }
    if (m_textHeight) {
        params.cellHeight = m_textHeight->value();
    }
    if (m_textAngle) {
        params.angleDeg = m_textAngle->value();
    }
    if (m_textBold) {
        params.bold = m_textBold->isChecked();
    }
    return params;
}

void DockParams::setMirrorModeOff()
{
    applyCheckedState(QString(), false);
}

void DockParams::setMirrorModeOn(const QString& objectName)
{
    applyCheckedState(objectName, true);
}

void DockParams::applyCheckedState(const QString& objectName, bool checked)
{
    m_silent = true;
    for (QAbstractButton* b : m_modeButtons) {
        const bool target = !objectName.isEmpty() && b->objectName() == objectName
                                ? checked
                                : false;
        if (b->isChecked() != target) {
            b->setChecked(target);
        }
    }
    m_silent = false;
}
