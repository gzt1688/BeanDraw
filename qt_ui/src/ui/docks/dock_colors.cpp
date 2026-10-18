#include "dock_colors.h"

#include "legend_menu_helpers.h"
#include "ui/i18n/tr_manager.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDrag>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr char kColorIndexMime[] = "application/x-color-index";

/// MIME 类型（QStringLiteral 不接受 constexpr char[]，需显式转换）。
inline QString colorMimeType()
{
    return QString::fromLatin1(kColorIndexMime);
}

/// 色块边长（两档）。
///
/// 大档是默认形态，与左右按钮等高；紧凑档只在色块总宽超出视口、必须显示横向
/// 滚动条时启用。紧凑档不是随手取的值 —— 它必须正好等于「底栏内高 - 滚动条高」，
/// 这样滚动条一出现，视口变矮多少、色块就矮多少，两者始终严丝合缝，绝不被压扁。
constexpr int kBlockSizeFull = 36;
constexpr int kHScrollHeight = 8;  // 与 app 级 QSS 的 QScrollBar:horizontal 一致
constexpr int kBarMarginV = 1;     // 底栏上下留白：各 1px，色块不贴边、描边有落点
constexpr int kBlockSizeCompact = kBlockSizeFull - kHScrollHeight;

/// 底栏总高 = 大色块 + 滚动条预留 + 上下留白。
///
/// 关键：底栏高度是**恒定**的，不随滚动条出现与否变化；滚动条始终占着那 8px，
/// 只是没有内容溢出时它透明不可见。所以色块的纵向位置永远不会跳动，
/// 「往下挤 / 被压矮」这类问题从根上不存在。
constexpr int kBarHeight = kBlockSizeFull + kHScrollHeight + kBarMarginV * 2;

/// 底栏左右留白。放在容器布局里，而不是 QSS padding —— 见构造函数里的说明。
constexpr int kBarSidePadding = 6;

/// 单个颜色块：色底 + DMC 色号文字（按亮度选黑白字）。
/// 支持拖拽：拖到另一个块上即触发合并（黑色拖到白色 = 黑色全部改用白色）。
class ColorBlockButton : public QToolButton
{
    Q_OBJECT
public:
    ColorBlockButton(int index, const QColor& color, const QString& code, const QChar& symbol,
                     QWidget* parent = nullptr)
        : QToolButton(parent)
        , m_index(index)
        , m_color(color)
        , m_code(code)
        , m_symbol(symbol)
    {
        setCheckable(true);
        m_blockSize = kBlockSizeFull;
        setFixedSize(m_blockSize, m_blockSize);
        setToolTip(code);
        setCursor(Qt::OpenHandCursor);
        setAcceptDrops(true);
        // 选中态（checked）变化时重算样式：选中色块补描边外框。
        // 不监听则 setChecked 后 QSS 仍是旧值（透明外框），外框不出现。
        connect(this, &QToolButton::toggled, this, [this] { applyStyle(); });
        applyStyle();
    }

    int colorIndex() const { return m_index; }

    /// 色块标签模式：0 = 符号 / 1 = DMC 色号 / 2 = 无文字。
    static void setLabelMode(int mode) { s_labelMode = mode; }
    static int labelMode() { return s_labelMode; }

    /// 切换色块尺寸档位。字号会跟着缩放，避免紧凑档下文字撑破方块。
    void setBlockSize(int px)
    {
        if (px == m_blockSize) {
            return;
        }
        m_blockSize = px;
        setFixedSize(px, px);
        applyStyle();
    }

    /// 搜索命中态：加橙色外框；未命中且正在过滤时淡化显示（不隐藏）。
    void setSearchState(bool filtering, bool hit)
    {
        m_filtering = filtering;
        m_hit = hit;
        applyStyle();
    }

signals:
    void selected(int index);
    void mergeTo(int fromIndex, int toIndex);
    /// 右键色块：请求弹出与材料表一致的上下文菜单（参数为色索引与全局坐标）。
    void contextMenuRequested(int index, const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_pressPos = e->pos();
            m_dragStarted = false;
        }
        QToolButton::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (!m_dragStarted && (e->buttons() & Qt::LeftButton)
            && (e->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragStarted = true;
            startDrag();
        }
        QToolButton::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (m_dragStarted) {
            m_dragStarted = false;
            return; // 拖拽手势已消费，不触发选择
        }
        // 先走基类（release 时切换 checked 态）再发 selected，否则基类 toggle 会把
        // onBlockClicked 刚设置的 checked 翻回 false，导致选中色块无外框。
        QToolButton::mouseReleaseEvent(e);
        if (e->button() == Qt::LeftButton) {
            emit selected(m_index);
        }
    }

    void dragEnterEvent(QDragEnterEvent* e) override
    {
        if (e->mimeData()->hasFormat(colorMimeType())) {
            e->setDropAction(Qt::MoveAction);
            e->accept();
        } else {
            e->ignore();
        }
    }

    /// 右键色块：弹出与材料表一致的菜单（用为当前色 / 更改符号 / 替换颜色 / 删除）。
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        emit contextMenuRequested(m_index, e->globalPos());
    }

    void dragMoveEvent(QDragMoveEvent* e) override
    {
        if (e->mimeData()->hasFormat(colorMimeType())) {
            e->setDropAction(Qt::MoveAction);
            e->accept();
        } else {
            e->ignore();
        }
    }

    void dropEvent(QDropEvent* e) override
    {
        if (e->mimeData()->hasFormat(colorMimeType())) {
            const int from = QString::fromUtf8(e->mimeData()->data(colorMimeType())).toInt();
            if (from != m_index) {
                emit mergeTo(from, m_index);
            }
            e->setDropAction(Qt::MoveAction);
            e->accept();
        } else {
            e->ignore();
        }
    }

private:
    /// 选中态描边最终保障：QSS border 之外再实画一道外框。
    /// 某些平台/主题下 QToolButton 的 :checked 边框渲染不稳定，paintEvent 兜底保证
    /// 选中色块一定有完整的主题高亮色外框（点击切换、程序化选中都会触发重绘）。
    void paintEvent(QPaintEvent* event) override
    {
        QToolButton::paintEvent(event);
        if (isChecked()) {
            QPainter p(this);
            p.setPen(QPen(QApplication::palette().color(QPalette::Highlight), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(rect().adjusted(1, 1, -2, -2));
        }
    }

    void applyStyle()
    {
        const QString textColor =
            m_color.lightness() < 128 ? QStringLiteral("#ffffff") : QStringLiteral("#202020");
        // 搜索只做高亮提示，色块本身始终保持完整原色 —— 不淡化、不隐藏。
        const QString bg = m_color.name();
        // 选中描边用 QSS 伪态 border（画在控件边缘内侧，Qt 渲染可靠）。
        // hover / checked / 搜索命中分别指定边框色，checked 优先于 hover。
        const QString accent = QApplication::palette().color(QPalette::Highlight).name();
        // 字号必须写进 QSS：控件级样式表里的 font-size 优先级高于 setFont()，
        // 只调 setFont() 会被下面这条 font-size 覆盖掉。
        int fontPx = 11;
        switch (s_labelMode) {
        case 0:
            setText(m_symbol.isNull() ? QString() : QString(m_symbol));
            fontPx = 14; // 符号模式大一号，符号本身笔画少，需要更醒目
            break;
        case 1:
            setText(m_code);
            fontPx = 12; // 色号是 "310" 这类 3 位数字，字号小一号才放得下
            break;
        default:
            setText(QString());
            break;
        }
        // 紧凑档按比例缩一档字，否则 "310" 会顶满 28px 的方块。
        if (m_blockSize < kBlockSizeFull) {
            fontPx = qMax(9, fontPx - 2);
        }
        setStyleSheet(QStringLiteral(
                          "QToolButton { background-color: %1; color: %2;"
                          " border: 2px solid transparent;"
                          " font-weight: bold; font-size: %3px; padding: 0; margin: 0; }"
                          "QToolButton:hover { border-color: #888888; }"
                          "QToolButton:checked { border-color: %4; }"
                          "QToolButton:checked:hover { border-color: %4; }"
                          "QToolButton[hit=\"true\"][filtering=\"true\"] { border-color: #ff8c00; }"
                          "QToolButton:checked[hit=\"true\"][filtering=\"true\"] { border-color: #ff8c00; }")
                          .arg(bg, textColor)
                          .arg(fontPx)
                          .arg(accent));
    }

    void startDrag()
    {
        auto* drag = new QDrag(this);
        auto* mime = new QMimeData();
        mime->setData(colorMimeType(), QString::number(m_index).toUtf8());
        drag->setMimeData(mime);
        drag->setPixmap(grab());
        drag->exec(Qt::MoveAction);
    }

    int m_index = -1;
    QColor m_color;
    QString m_code;
    QChar m_symbol;
    QPoint m_pressPos;
    int m_blockSize = kBlockSizeFull;
    bool m_dragStarted = false;
    bool m_filtering = false;
    bool m_hit = true;

    /// 全局标签模式：0 = 符号 / 1 = DMC 色号 / 2 = 无文字。
    static int s_labelMode;
};

int ColorBlockButton::s_labelMode = 1;

} // namespace

int DockColors::labelMode()
{
    return ColorBlockButton::labelMode();
}

DockColors::DockColors(QWidget* parent)
    : QToolBar(parent)
{
    setObjectName(QStringLiteral("DockColors"));
    // 不可拖动 / 不可浮动：去掉左侧拖动手柄，避免切换标签模式时手柄占用最左
    // 空间、把整条色带内容右推导致错位。颜色栏固定在底部，由面板菜单控制显隐。
    setMovable(false);
    setFloatable(false);

    // 关键：QSS 里不能写任何 padding。
    // Qt 的 QSS padding 会同步改写 contentsRect()，而 enforceBarGeometry()
    // 正是拿 contentsRect() 给容器定位；一旦两者不一致，setFixedHeight 又把高度
    // 钳回整条底栏，容器就会被顶到 y=2、高 36 —— 底边溢出 2px，整栏内容整体
    // 下移一截、底部被裁。切换符号走 buildBlocks()→enforceBarGeometry()，所以
    // 表现为「一切换符号就往下挤」。这里 padding 归零，contentsRect()==rect()，
    // 几何关系自洽；左右留白交给容器布局的 contentsMargins。
    setStyleSheet(QStringLiteral(
        "QToolBar#DockColors { padding: 0; border: none; spacing: 0; }"));
    setFixedHeight(kBarHeight);
    setIconSize(QSize(18, 18));
    // 注意：不要再调用 setContentsMargins —— QSS padding 即边距，二者会叠加
    m_labelMode = 1; // 默认色号

    // 整条底栏由一个横铺容器承载，保证各控件纵向撑满、颜色块纵向居中。
    m_bar = new QWidget(this);
    m_bar->setObjectName(QStringLiteral("colorBarBody"));
    m_bar->setStyleSheet(QStringLiteral("QWidget#colorBarBody { background: transparent; }"));
    auto* barLayout = new QHBoxLayout(m_bar);
    barLayout->setContentsMargins(kBarSidePadding, kBarMarginV, kBarSidePadding, kBarMarginV);
    barLayout->setSpacing(6);

    // 左侧：清理未使用颜色（常规按钮样式，有底色与边框）
    m_cleanBtn = new QToolButton(m_bar);
    m_cleanBtn->setObjectName(QStringLiteral("cleanUnusedButton"));
    m_cleanBtn->setText(tr("Clean"));
    m_cleanBtn->setToolTip(tr("Remove colors not used by any stitch"));
    m_cleanBtn->setCursor(Qt::PointingHandCursor);
    m_cleanBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background: #fdfdfd; color: #1b1b1b; border: 1px solid #c9c9c9;"
        " border-bottom-color: #adadad; border-radius: 6px; padding: 0 6px;"
        " font-size: 9pt; }"
        "QToolButton:hover { background: #ffffff; border-color: #b0b0b0; }"
        "QToolButton:pressed { background: #e4e4e4; }"));
    m_cleanBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_cleanBtn->setFixedWidth(52);
    connect(m_cleanBtn, &QToolButton::clicked, this, &DockColors::onCleanClicked);
    barLayout->addWidget(m_cleanBtn);

    // 左侧：色块标签三态切换（符号 → 色号 → 无）
    m_labelBtn = new QToolButton(m_bar);
    m_labelBtn->setObjectName(QStringLiteral("labelModeButton"));
    m_labelBtn->setToolTip(tr("What to show on colour blocks: symbol / DMC code / nothing"));
    m_labelBtn->setCursor(Qt::PointingHandCursor);
    m_labelBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background: #fdfdfd; color: #1b1b1b; border: 1px solid #c9c9c9;"
        " border-bottom-color: #adadad; border-radius: 6px; padding: 0 6px;"
        " font-size: 9pt; }"
        "QToolButton:hover { background: #ffffff; border-color: #b0b0b0; }"
        "QToolButton:pressed { background: #e4e4e4; }"));
    m_labelBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_labelBtn->setFixedWidth(56);
    applyLabelMode();
    connect(m_labelBtn, &QToolButton::clicked, this, &DockColors::onLabelModeClicked);
    barLayout->addWidget(m_labelBtn);

    // 颜色块滚动区（横向铺满，背景全透明，无边框、无白色底衬）
    m_scroll = new QScrollArea(m_bar);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scroll->viewport()->setAutoFillBackground(false);
    // 滚动条样式：app 级 QSS 给的是 height 8px 但 handle min-height 24px，
    // 24px 塞不进 8px 的槽会溢出去啃色块，所以这里把 handle 压到槽高内。
    // 平时透明（没有颜色溢出时 QScrollArea 会自行隐藏滑块），溢出时才可见。
    m_scroll->setStyleSheet(QStringLiteral(
        "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"
        "QScrollBar:horizontal { height: 8px; margin: 0; background: transparent; }"
        "QScrollBar::handle:horizontal { min-height: 0; max-height: 6px;"
        " background: #c8c8c8; border-radius: 3px; margin: 1px 2px; }"
        "QScrollBar::handle:horizontal:hover { background: #a8a8a8; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; height: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"));
    m_container = new QWidget();
    m_container->setAutoFillBackground(false);
    m_container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* row = new QHBoxLayout(m_container);
    row->setContentsMargins(2, 0, 2, 0); // 上下不留边距，方块与按钮严格对齐
    row->setSpacing(0); // 颜色块之间不留间距，紧挨成一条色带
    m_scroll->setWidget(m_container);
    barLayout->addWidget(m_scroll, 1);

    // 右侧搜索框（收窄，纵向撑满整条底栏）
    m_search = new QLineEdit(m_bar);
    m_search->setObjectName(QStringLiteral("colorSearch"));
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(84);
    m_search->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_search->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #ffffff; color: #1b1b1b; border: 1px solid #c9c9c9;"
        " border-radius: 6px; padding: 0 6px; }"
        "QLineEdit:focus { border-color: #8a8a8a; }"));
    connect(m_search, &QLineEdit::textChanged, this, &DockColors::onSearchChanged);
    barLayout->addWidget(m_search);

    // 让容器铺满工具栏（QToolBar 不会按 Expanding 拉伸子控件）。
    // 固定高度：布局无法再把容器压回 sizeHint，避免切换标签模式后色块底部被裁。
    addWidget(m_bar);
    m_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_bar->setFixedHeight(kBarHeight);

    // 语言切换 → 完整重译（标签模式按钮 / 清理按钮 / 搜索占位 / 提示标签重建）
    connect(&TrManager::instance(), &TrManager::languageChanged,
            this, &DockColors::retranslateUi);
}

void DockColors::retranslateUi()
{
    m_cleanBtn->setText(tr("Clean"));
    m_cleanBtn->setToolTip(tr("Remove colors not used by any stitch"));
    m_labelBtn->setToolTip(tr("What to show on colour blocks: symbol / DMC code / nothing"));
    m_search->setPlaceholderText(tr("Search"));
    applyLabelMode();
    buildBlocks(); // "No chart" / "No matching color" 提示标签随语言重建
}

void DockColors::resizeEvent(QResizeEvent* event)
{
    QToolBar::resizeEvent(event);
    enforceBarGeometry();
    // 视口宽度变了，色块该放大还是缩小要重新判定（拖宽窗口后可以回到大档）。
    applyBlockSizePolicy();
}

void DockColors::refresh(const ChartSnapshot& snapshot)
{
    m_palette.clear();
    m_codes.clear();
    m_names.clear();
    m_symbols.clear();

    if (!snapshot.valid || snapshot.palette.isEmpty()) {
        buildBlocks();
        return;
    }
    for (int i = 0; i < snapshot.palette.size(); ++i) {
        m_palette.append(snapshot.palette.at(i));
        m_codes.append(snapshot.colorCodes.value(i));
        m_names.append(snapshot.colorNames.value(i));
        m_symbols.append(snapshot.symbols.value(i));
    }
    m_symbolAlphabet = snapshot.symbolAlphabet;
    buildBlocks();
}

void DockColors::setActiveIndex(int index)
{
    m_currentIndex = index;
    for (QToolButton* b : m_container->findChildren<ColorBlockButton*>()) {
        const bool on = b->property("colorIndex").toInt() == index;
        b->setChecked(on);
        if (auto* block = qobject_cast<ColorBlockButton*>(b)) {
            block->setSearchState(b->property("filtering").toBool(),
                                  b->property("hit").toBool());
        }
    }
}

void DockColors::clearBlocks()
{
    auto* row = static_cast<QHBoxLayout*>(m_container->layout());
    while (row->count() > 0) {
        QLayoutItem* item = row->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
}

/// 强制容器铺满整条底栏。
///
/// 用 rect() 而不是 contentsRect()：QSS 的 padding / margin 会改写
/// contentsRect()，一旦 QSS 内高与 m_bar 的固定高度对不上，setGeometry 的高度
/// 会被 setFixedHeight 强行拉回，容器就整体下移并溢出底边（症状：切换符号后
/// 整栏元素往下挤、底部被裁）。改用 rect() 后定位完全由本文件掌控，
/// 留白由 m_bar 内部布局的 kBarSidePadding 负责。
///
/// QToolBarLayout 在内容变化（切换标签模式、搜索过滤、增删颜色）后会异步二次
/// 排布，所以 resizeEvent 与内容重建后各调用一次。
void DockColors::enforceBarGeometry()
{
    if (m_bar && m_bar->parentWidget()) {
        m_bar->setGeometry(rect());
    }
}

/// 当前色块总宽是否已超出视口（超出才需要切紧凑档 / 出滚动条）。
///
/// 注意用「所有色块按大档尺寸的总宽」去比，而不是读 hbar 的 range：色块切到
/// 紧凑档后总宽会变短、滚动条可能就不再需要，若用 range 判断会在两档之间反复
/// 抖动（缩→不溢出→放→又溢出）。
bool DockColors::needsCompactBlocks() const
{
    const int count = m_palette.size();
    if (count <= 0) {
        return false;
    }
    // 视口宽度还没算出来（首帧 / 尚未 show）时不能判：此时 width()==0 会把
    // 「颜色其实放得下」误判成溢出，直接掉到紧凑档。
    const int viewportWidth = m_scroll->viewport()->width();
    if (viewportWidth <= 0) {
        return false;
    }
    const int left = 2; // m_container 布局的左右留白
    const int right = 2;
    // 用调色板条目数而不是 findChildren()：clearBlocks() 走 deleteLater()，
    // 上一轮的旧色块在下一个事件循环前仍是子对象，会被误计入。
    const int fullWidth = count * kBlockSizeFull + left + right;
    return fullWidth > viewportWidth;
}

/// 按视口宽度在两档尺寸间切换色块。
///
/// 大档（36，与按钮等高）是默认形态；只有颜色多到一行放不下时才降到紧凑档
/// （28），此时横向滚动条正好占掉预留的 8px，色块仍然完整可见、不被压扁。
///
/// 判定一律以「大档总宽 vs 视口宽」为准，不读滚动条的 range：紧凑档下总宽会
/// 变短，若用 range 判断就会在两档之间反复抖动（缩→不溢出→放→又溢出）。
void DockColors::applyBlockSizePolicy()
{
    const int size = needsCompactBlocks() ? kBlockSizeCompact : kBlockSizeFull;
    for (ColorBlockButton* b : m_container->findChildren<ColorBlockButton*>()) {
        b->setBlockSize(size);
    }
    // 不手动 setVisible 滚动条：策略是 ScrollBarAsNeeded，QScrollArea 会在内容
    // 溢出时自行显示。手动接管反而会在溢出时把滑块藏掉，用户就拖不动了。
    // 底栏高度恒为 kBarHeight，滚动条那 8px 一直占着，所以有无滑块色块都不跳。
}

void DockColors::buildBlocks()
{
    clearBlocks();
    auto* row = static_cast<QHBoxLayout*>(m_container->layout());

    // 搜索只做高亮提示，未命中的颜色块仍完整显示（淡化而非隐藏）
    const QString filter = m_search->text().trimmed().toLower();
    const bool filtering = !filter.isEmpty();

    int hits = 0;
    for (int i = 0; i < m_palette.size(); ++i) {
        const QString code = m_codes.value(i);
        const bool hit = !filtering || code.toLower().contains(filter)
                         || m_names.value(i).toLower().contains(filter);
        if (hit) {
            ++hits;
        }
        const QColor color = m_palette.at(i);
        auto* button = new ColorBlockButton(i, color, code.isEmpty() ? QStringLiteral("?") : code,
                                           m_symbols.value(i), m_container);
        button->setProperty("colorIndex", i);
        button->setProperty("filtering", filtering);
        button->setProperty("hit", hit);
        button->setSearchState(filtering, hit);
        connect(button, &ColorBlockButton::selected, this, &DockColors::onBlockClicked);
        connect(button, &ColorBlockButton::mergeTo, this,
                [this](int from, int to) { emit colorMerged(from, to); });
        connect(button, &ColorBlockButton::contextMenuRequested, this,
                &DockColors::onBlockContextMenu);
        row->addWidget(button);
    }

    if (m_palette.isEmpty()) {
        auto* hint = new QLabel(tr("No chart"), m_container);
        hint->setAlignment(Qt::AlignCenter);
        row->addWidget(hint);
    } else if (filtering && hits == 0) {
        auto* hint = new QLabel(tr("No matching color"), m_container);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet(QStringLiteral("color: #888888;"));
        row->addWidget(hint);
    }
    row->addStretch(1);

    // 维持 / 初始化选中态
    if (m_currentIndex >= 0 && m_currentIndex < m_palette.size()) {
        for (QToolButton* b : m_container->findChildren<ColorBlockButton*>()) {
            b->setChecked(b->property("colorIndex").toInt() == m_currentIndex);
        }
    } else if (auto* first = m_container->findChild<ColorBlockButton*>()) {
        first->setChecked(true);
        m_currentIndex = first->property("colorIndex").toInt();
        emit colorActivated(m_currentIndex);
    }
    // 内容重建后 QToolBarLayout 可能已把容器压矮，这里强制恢复一次；
    // 该布局常在本次调用返回后才对内容变更做二次排布，故再延迟修正一次确保稳定。
    enforceBarGeometry();
    // 尺寸选档依赖视口宽度，视口要等布局跑完才有最终值，所以同样延迟补一次。
    applyBlockSizePolicy();
    QTimer::singleShot(0, this, [this] {
        enforceBarGeometry();
        applyBlockSizePolicy();
    });
}

void DockColors::onSearchChanged(const QString&)
{
    buildBlocks();
}

void DockColors::onCleanClicked()
{
    emit pruneRequested();
}

void DockColors::onLabelModeClicked()
{
    // 三态循环：符号(0) → 色号(1) → 无(2)
    ColorBlockButton::setLabelMode((ColorBlockButton::labelMode() + 1) % 3);
    applyLabelMode();
    buildBlocks(); // 重建以更新色块文字
    emit labelModeChanged(ColorBlockButton::labelMode()); // 画布格子文本同步
}

void DockColors::applyLabelMode()
{
    // 三态文案随语言切换（源文为英文，中文进 beandraw_zh_CN.ts 的 DockColors）
    const int mode = ColorBlockButton::labelMode();
    m_labelMode = mode;
    if (m_labelBtn) {
        switch (mode) {
        case 0:
            m_labelBtn->setText(tr("Symbol"));
            break;
        case 1:
            m_labelBtn->setText(tr("Code"));
            break;
        default:
            m_labelBtn->setText(tr("None"));
            break;
        }
    }
}

void DockColors::onBlockClicked(int index)
{
    for (QToolButton* b : m_container->findChildren<ColorBlockButton*>()) {
        b->setChecked(b->property("colorIndex").toInt() == index);
    }
    m_currentIndex = index;
    emit colorActivated(index);
}

void DockColors::onBlockContextMenu(int index, const QPoint& globalPos)
{
    if (index < 0 || index >= m_palette.size()) {
        return;
    }
    QMenu menu(this);
    // 与材料表（DockLegend::showContextMenu）完全一致的菜单项与顺序
    QAction* symbolAction =
        menu.addAction(QCoreApplication::translate("DockLegend", "Change symbol..."));
    QAction* mergeAction =
        menu.addAction(QCoreApplication::translate("DockLegend", "Replace with another colour..."));
    QAction* deleteAction =
        menu.addAction(QCoreApplication::translate("DockLegend", "Delete colour"));

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) {
        return;
    }
    if (chosen == symbolAction) {
        const QChar sym = cmPromptChangeSymbol(this, index, m_symbols, m_symbolAlphabet, m_codes);
        if (!sym.isNull()) {
            emit symbolChangeRequested(index, static_cast<uint>(sym.unicode()));
        }
        return;
    }
    if (chosen == mergeAction) {
        // 替换颜色：页1 与已有颜色互换；页2 从色卡库选新色替换
        const CmMergeChoice choice =
            cmPromptMergeTarget(this, index, m_codes, m_names, m_palette,
                                static_cast<int>(m_craftMode));
        if (choice.kind == 0 && choice.index >= 0 && choice.index != index) {
            emit colorMerged(index, choice.index);
        } else if (choice.kind == 1 && !choice.code.isEmpty()) {
            emit colorReplaceWithCardRequested(index, choice.code);
        }
        return;
    }
    if (chosen == deleteAction) {
        emit colorDeleteRequested(index);
    }
}

#include "dock_colors.moc"
