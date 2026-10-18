#include "dock_legend.h"

#include "legend_menu_helpers.h"
#include "ui/i18n/tr_manager.h"
#include "ui/icon_factory.h"

#include "ffi_bind/ffi_types.h"
#include "ffi_bind/rust_api.h"
#include "project/project_doc.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QToolButton>
#include <QInputDialog>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QTextDocument>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QPalette>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QPoint>
#include <QStringList>
#include <QTableWidget>
#include <QVariant>
#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QMargins>
#include <QPageSize>
#include <QPageLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {
/// 列索引：颜色列已移除（符号列底色即颜色）。
constexpr int kColumnNo = 0;
constexpr int kColumnSymbol = 1;
constexpr int kColumnCode = 2;
constexpr int kColumnName = 3;
constexpr int kColumnStitches = 4;
constexpr int kColumnBeads = 5;
constexpr int kColumnLength = 6;
constexpr int kColumnCount = 7;
} // namespace

// ------------------------------------------------------------
// LegendTable
// ------------------------------------------------------------

LegendTable::LegendTable(QWidget* parent)
    : QTableWidget(parent)
{
}

void LegendTable::startDrag(Qt::DropActions actions)
{
    QTableWidget::startDrag(actions);
}

QMimeData* LegendTable::mimeData(const QList<QTableWidgetItem*>& items) const
{
    QMimeData* mime = QTableWidget::mimeData(items);
    for (QTableWidgetItem* item : items) {
        const QVariant v = item->data(Qt::UserRole);
        if (v.isValid()) {
            mime->setData(QString::fromLatin1(kLegendDragMime), QString::number(v.toInt()).toUtf8());
            break;
        }
    }
    return mime;
}

void LegendTable::dragMoveEvent(QDragMoveEvent* e)
{
    if (e->mimeData()->hasFormat(QString::fromLatin1(kLegendDragMime))
        && rowAt(e->position().toPoint().y()) >= 0) {
        e->setDropAction(Qt::MoveAction);
        e->accept();
        return;
    }
    e->ignore();
}

void LegendTable::dropEvent(QDropEvent* e)
{
    const QString mimeType = QString::fromLatin1(kLegendDragMime);
    if (!e->mimeData()->hasFormat(mimeType)) {
        e->ignore();
        return;
    }
    const int from = QString::fromUtf8(e->mimeData()->data(mimeType)).toInt();
    const QModelIndex idx = indexAt(e->position().toPoint());
    const int to = idx.isValid() ? idx.data(Qt::UserRole).toInt() : -1;
    if (to < 0 || from < 0 || from == to) {
        e->ignore();
        return;
    }
    // 仅 CopyAction 接收（合并由 mergeRequested 信号驱动，不依赖 Move 的返回值）。
    // 若返回 MoveAction，基类 QAbstractItemView::startDrag 会调用 clearOrRemove()
    // 把「当前选中行」的所有单元格文字清空——而 merge 触发的 refresh() 恰好刚刚
    // setCurrentCell 选中了第一行 → 材料表合并后第一行变空白。
    e->setDropAction(Qt::CopyAction);
    e->accept();
    emit mergeRequested(from, to);
}

/// 材料表自定义绘制：选中时只高亮「序号」列底色（用主题高亮色，白字），
/// 并在整行外缘画完整的高亮外框（同样用主题高亮色）。
class DockLegend;
class LegendItemDelegate : public QStyledItemDelegate
{
public:
    LegendItemDelegate(DockLegend* owner, LegendTable* table, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_owner(owner)
        , m_table(table)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const bool selected = (option.state & QStyle::State_Selected) != 0;
        QStyleOptionViewItem opt = option;
        // 去掉默认选中态，避免基类 / 样式表画出全行蓝色填充盖住颜色。
        opt.state &= ~QStyle::State_Selected;
        initStyleOption(&opt, index);

        QStyledItemDelegate::paint(painter, opt, index);

        if (!selected) {
            return;
        }

        // 选中行：序号格整块主题高亮 + 白字 + 整行完整外框——同一帧绘制，无延迟。
        const QColor accent = m_owner ? m_owner->accentColor()
                                      : QApplication::palette().color(QPalette::Highlight);
        const QRect r = opt.rect;
        painter->save();
        if (index.column() == kColumnNo) {
            painter->fillRect(r, accent);
            painter->setPen(Qt::white);
            painter->drawText(r, opt.displayAlignment, opt.text);
        }
        painter->setPen(QPen(accent, 2));
        painter->drawLine(r.topLeft(), r.topRight());
        painter->drawLine(r.bottomLeft(), r.bottomRight());
        int firstVis = -1;
        int lastVis = -1;
        const int cols = m_table ? m_table->columnCount() : 0;
        for (int c = 0; c < cols; ++c) {
            if (m_table->isColumnHidden(c)) {
                continue;
            }
            if (firstVis < 0) {
                firstVis = c;
            }
            lastVis = c;
        }
        if (index.column() == firstVis) {
            painter->drawLine(r.topLeft(), r.bottomLeft());
        }
        if (index.column() == lastVis) {
            painter->drawLine(r.topRight(), r.bottomRight());
        }
        painter->restore();
    }

private:
    DockLegend* m_owner = nullptr;
    LegendTable* m_table = nullptr;
};

DockLegend::DockLegend(ProjectDoc* project, ads::CDockManager* manager, QWidget* parent)
    : ads::CDockWidget(manager, QString(), parent)
    , m_project(project)
{
    setObjectName(QStringLiteral("DockLegend"));
    buildUi();
    // 语言切换 → 面板内部完整重译（标题由主窗口 retranslateUi 统一维护）
    connect(&TrManager::instance(), &TrManager::languageChanged,
            this, &DockLegend::retranslateUi);
}

void DockLegend::buildUi()
{
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // 手作品类三选一：控制材料表显示哪种模式需要的参数列
    m_crossRadio = new QRadioButton(tr("Cross stitch"), widget);
    m_crossRadio->setObjectName(QStringLiteral("legendCrossRadio"));
    m_beadRadio = new QRadioButton(tr("Bead embroidery"), widget);
    m_beadRadio->setObjectName(QStringLiteral("legendBeadRadio"));
    m_perlerRadio = new QRadioButton(tr("Perler beads"), widget);
    m_perlerRadio->setObjectName(QStringLiteral("legendPerlerRadio"));

    auto* modeRow = new QWidget(widget);
    auto* modeLayout = new QHBoxLayout(modeRow);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(8);
    modeLayout->addWidget(m_perlerRadio);
    modeLayout->addWidget(m_beadRadio);
    modeLayout->addWidget(m_crossRadio);

    // 盘选择下拉：总盘 / 盘1 / 盘2 / 盘3（多盘系统，类拓竹多平台）
    m_plateCombo = new QComboBox(modeRow);
    m_plateCombo->setObjectName(QStringLiteral("legendPlateCombo"));
    m_plateCombo->setMaximumWidth(96);
    m_plateCombo->setToolTip(tr("All plates side by side, or a single plate"));
    modeLayout->addSpacing(4);
    modeLayout->addWidget(m_plateCombo);

    // 「新增盘」按钮：把大图再切分出一块新盘（上限 3 盘）
    m_addPlateButton = new QToolButton(modeRow);
    m_addPlateButton->setObjectName(QStringLiteral("legendAddPlateButton"));
    m_addPlateButton->setText(QStringLiteral("+"));
    m_addPlateButton->setFixedSize(22, 22);
    m_addPlateButton->setToolTip(tr("Add a plate (split the chart into one more plate)"));
    m_addPlateButton->setEnabled(false);
    modeLayout->addWidget(m_addPlateButton);

    modeLayout->addStretch();
    layout->addWidget(modeRow);
    setPlateCount(1);

    connect(m_plateCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        // 首项为总盘（-1），其后依次为盘1..盘N
        m_currentPlate = index - 1;
        emit plateSelected(m_currentPlate);
    });
    connect(m_addPlateButton, &QToolButton::clicked, this,
            [this]() { emit plateAddRequested(); });

    const auto onModeChanged = [this]() {
        m_craftMode = m_perlerRadio->isChecked() ? cm::craft::PERLER
                     : m_beadRadio->isChecked()  ? cm::craft::BEAD
                                                 : cm::craft::CROSS_STITCH;
        applyModeColumns();
        // m_table 可能尚未创建：buildUi() 里 setChecked(true) 会同步触发
        // toggled → 早于表格创建的 lambda，而表格在更后面才 new 出来。空指针调用
        // rowCount() 会在 Qt6Widgets 内部解引用空 this（启动崩溃）。
        if (m_table && m_table->rowCount() > 0) {
            refresh(m_snapshot);
        }
        emit craftModeChanged(m_craftMode);
    };
    connect(m_crossRadio, &QRadioButton::toggled, this, [onModeChanged](bool on) {
        if (on) {
            onModeChanged();
        }
    });
    connect(m_beadRadio, &QRadioButton::toggled, this, [onModeChanged](bool on) {
        if (on) {
            onModeChanged();
        }
    });
    connect(m_perlerRadio, &QRadioButton::toggled, this, [onModeChanged](bool on) {
        if (on) {
            onModeChanged();
        }
    });
    // 默认模式：拼豆（软件打开即拼豆材料表，与色卡面板一致）
    m_perlerRadio->setChecked(true);

    // 顶部汇总行：只放针数 / 珠子 / 颜色（尺寸与布料移到底部信息条）
    m_summaryLabel = new QLabel(tr("No chart"), widget);
    m_summaryLabel->setObjectName(QStringLiteral("legendSummary"));
    layout->addWidget(m_summaryLabel);

    m_table = new LegendTable(widget);
    m_table->setColumnCount(kColumnCount);
    m_table->setHorizontalHeaderLabels({tr("No."), tr("Symbol"), tr("DMC"),
                                        tr("Name"), tr("Stitches"), tr("Beads"),
                                        tr("Thread")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setObjectName(QStringLiteral("legendTable"));
    // 自定义选中样式：仅高亮序号列 + 整行外框（见 LegendItemDelegate）
    m_table->setItemDelegate(new LegendItemDelegate(this, m_table, m_table));

    // 表格内拖拽合并：把一行拖到另一行上
    m_table->setDragEnabled(true);
    m_table->setAcceptDrops(true);
    m_table->setDropIndicatorShown(true);
    m_table->setDragDropMode(QAbstractItemView::DragDrop);
    m_table->setDefaultDropAction(Qt::MoveAction);

    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &DockLegend::showContextMenu);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QVariant v = m_table->item(row, kColumnCode)->data(Qt::UserRole);
        if (v.isValid()) {
            m_selectedIndex = v.toInt();
            emit colorActivated(v.toInt());
        }
    });
    connect(m_table, &LegendTable::mergeRequested, this, [this](int from, int to) {
        emit colorMerged(from, to);
    });

    layout->addWidget(m_table);

    // 底部同一行：成品尺寸 / 颜色统计信息条（左，自动换行） + 打印材料表按钮（右）
    m_footLabel = new QLabel(widget);
    m_footLabel->setObjectName(QStringLiteral("legendFooter"));
    m_footLabel->setWordWrap(true);
    m_footLabel->setStyleSheet(QStringLiteral("color: #5d5d5d;"));
    auto* footRow = new QWidget(widget);
    auto* footRowLayout = new QHBoxLayout(footRow);
    footRowLayout->setContentsMargins(0, 0, 0, 0);
    footRowLayout->setSpacing(8);
    footRowLayout->addWidget(m_footLabel, 1);
    m_printButton = new QPushButton(footRow);
    m_printButton->setObjectName(QStringLiteral("legendPrintButton"));
    m_printButton->setText(tr("Print"));
    m_printButton->setIcon(IconFactory::make(
        IconFactory::Kind::Print, palette().color(QPalette::WindowText)));
    footRowLayout->addWidget(m_printButton);
    layout->addWidget(footRow);
    connect(m_printButton, &QPushButton::clicked,
            this, &DockLegend::printMaterialList);
    setWidget(widget);
}

void DockLegend::retranslateUi()
{
    m_crossRadio->setText(tr("Cross stitch"));
    m_beadRadio->setText(tr("Bead embroidery"));
    m_perlerRadio->setText(tr("Perler beads"));
    m_plateCombo->setToolTip(tr("All plates side by side, or a single plate"));
    m_addPlateButton->setToolTip(tr("Add a plate (split the chart into one more plate)"));
    m_table->setHorizontalHeaderLabels({tr("No."), tr("Symbol"), tr("DMC"),
                                        tr("Name"), tr("Stitches"), tr("Beads"),
                                        tr("Thread")});
    m_printButton->setText(tr("Print"));
    setPlateCount(m_plateCount); // 重建盘下拉项（All plates / 盘N 文案）
    refresh(m_snapshot); // 汇总行 / 底部尺寸条 / 符号 tooltip / 珠子列头随语言更新
}

void DockLegend::refresh(const ChartSnapshot& snapshot)
{
    m_snapshot = snapshot;
    m_table->setRowCount(0);

    const bool beadMode = m_craftMode != cm::craft::CROSS_STITCH;

    // 按模式与设置切换列可见性（颜色列已移除；名称列默认不显示）
    // 十字绣：针数 + 用线；珠绣/拼豆：珠子（豆）数，不显示用线。
    m_table->setColumnHidden(kColumnName, !m_usage.showColorNameColumn);
    m_table->setColumnHidden(kColumnStitches,
                             !(m_craftMode == cm::craft::CROSS_STITCH
                               && m_usage.showStitchesColumn));
    m_table->setColumnHidden(kColumnBeads,
                             !(beadMode && m_usage.showBeadsColumn));
    m_table->setColumnHidden(kColumnLength,
                             !(m_craftMode == cm::craft::CROSS_STITCH
                               && m_usage.showThreadColumn));

    // 珠子列标题随模式：珠绣 → 珠数；拼豆 → 豆数
    if (auto* headerItem = m_table->horizontalHeaderItem(kColumnBeads)) {
        headerItem->setText(m_craftMode == cm::craft::PERLER ? tr("Beads (perler)")
                                                            : tr("Beads"));
    }

    if (!snapshot.valid || snapshot.cells.isEmpty() || snapshot.legend.isEmpty()) {
        m_summaryLabel->setText(tr("No chart"));
        m_footLabel->setText(QString());
        m_table->setRowCount(0); // 清空旧行：合并、删除颜色后不留空行残留。
        return;
    }

    // 过滤空条目：合并/编辑后理论不应出现；出现则跳过，避免表格出现整行空白
    QVector<CmLegendEntry> validEntries;
    validEntries.reserve(snapshot.legend.size());
    for (const CmLegendEntry& e : snapshot.legend) {
        const int eidx = static_cast<int>(e.color_index);
        if (eidx < 0 || eidx >= snapshot.colorCodes.size()) {
            continue;
        }
        if (snapshot.colorCodes.at(eidx).isEmpty()) {
            continue;
        }
        validEntries.push_back(e);
    }
    const int rows = validEntries.size();
    m_table->setRowCount(rows);

    quint32 beadTotal = 0;
    for (int r = 0; r < rows; ++r) {
        const CmLegendEntry& entry = validEntries.at(r);
        const int idx = static_cast<int>(entry.color_index);
        const QString code =
            idx < snapshot.colorCodes.size() ? snapshot.colorCodes.at(idx) : QString();
        const QString name =
            idx < snapshot.colorNames.size() ? snapshot.colorNames.at(idx) : QString();
        const QChar sym = idx < snapshot.symbols.size() ? snapshot.symbols.at(idx) : QChar('?');
        const QColor color = idx < snapshot.palette.size() ? snapshot.palette.at(idx) : QColor();
        beadTotal += entry.bead_count;

        // 序号（材料表里的第几号，便于按顺序核对用量）
        auto* noItem = new QTableWidgetItem(QString::number(r + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        noItem->setData(Qt::UserRole, idx);
        noItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_table->setItem(r, kColumnNo, noItem);

        // 符号（衬在色块上，与画布所见一致；可拖到其他行上完成合并）
        auto* symbolItem = new QTableWidgetItem(QString(sym));
        symbolItem->setBackground(color);
        symbolItem->setForeground(color.lightness() < 128 ? Qt::white : Qt::black);
        symbolItem->setTextAlignment(Qt::AlignCenter);
        symbolItem->setData(Qt::UserRole, idx);
        symbolItem->setToolTip(tr("Click to use this floss, drag onto another row to merge"));
        symbolItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_table->setItem(r, kColumnSymbol, symbolItem);

        auto* codeItem = new QTableWidgetItem(code);
        codeItem->setData(Qt::UserRole, idx);
        codeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_table->setItem(r, kColumnCode, codeItem);
        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, idx);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_table->setItem(r, kColumnName, nameItem);

        auto* stitchesItem = new QTableWidgetItem(QString::number(entry.stitch_count));
        stitchesItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stitchesItem->setData(Qt::UserRole, idx);
        stitchesItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_table->setItem(r, kColumnStitches, stitchesItem);

        auto* beadsItem = new QTableWidgetItem(
            beadMode ? QString::number(m_usage.beadsFor(entry.bead_count)) : QStringLiteral("-"));
        beadsItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        beadsItem->setData(Qt::UserRole, idx);
        beadsItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_table->setItem(r, kColumnBeads, beadsItem);

        // 用线：按设置里的单针线长 / 余量 / 股数换算成厘米，并给出所需线束数
        const int skeins = m_usage.skeinsFor(entry.stitch_count);
        const double lengthCm =
            entry.stitch_count * m_usage.effectiveStitchLengthMm() / 10.0
            * (1.0 + m_usage.flossMarginPct / 100.0);
        auto* lengthItem = new QTableWidgetItem(
            skeins > 0 ? tr("%1 cm · %2 skein").arg(QString::number(qRound(lengthCm))).arg(skeins)
                       : QStringLiteral("-"));
        lengthItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lengthItem->setData(Qt::UserRole, idx);
        lengthItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_table->setItem(r, kColumnLength, lengthItem);
    }

    m_table->resizeColumnsToContents();
    m_table->setColumnWidth(kColumnNo, 40);
    m_table->setColumnWidth(kColumnSymbol, 52); // 符号列加宽，衬底色块更清晰

    // 默认选中：有图时始终有一行被选中（作为添加/涂抹颜色的当前项）。
    // 之前选中的颜色仍存在 → 保持选中不打断画笔；否则默认第一行并激活它。
    int targetRow = -1;
    for (int r = 0; r < rows; ++r) {
        if (m_table->item(r, kColumnCode)->data(Qt::UserRole).toInt() == m_selectedIndex) {
            targetRow = r;
            break;
        }
    }
    if (targetRow < 0) {
        targetRow = 0;
        m_selectedIndex = m_table->item(0, kColumnCode)->data(Qt::UserRole).toInt();
        emit colorActivated(m_selectedIndex);
    }
    m_table->setCurrentCell(targetRow, kColumnNo);

    // 顶部汇总：只放针数 / 珠子量（颜色合计放底部）
    if (beadMode) {
        m_summaryLabel->setText(tr("Stitches: %1   Beads: %2 (incl. margin)")
                                    .arg(snapshot.stitchTotal)
                                    .arg(m_usage.beadsFor(beadTotal)));
    } else {
        m_summaryLabel->setText(tr("Stitches: %1").arg(snapshot.stitchTotal));
    }
    // 底部信息条：颜色合计；十字绣附布料信息（拼豆/珠绣不需要布料）
    if (m_craftMode == cm::craft::CROSS_STITCH) {
        m_footLabel->setText(
            tr("Colors: %1   \u00b7   Finished size: %2 \u00d7 %3 mm   \u00b7   "
               "Fabric needed: %4 \u00d7 %5 mm (%6ct, %7 mm border per side)")
                .arg(rows)
                .arg(snapshot.fabricSize.width_mm)
                .arg(snapshot.fabricSize.height_mm)
                .arg(m_usage.requiredFabricSideMm(snapshot.gridSize.width()))
                .arg(m_usage.requiredFabricSideMm(snapshot.gridSize.height()))
                .arg(m_usage.fabricCount)
                .arg(m_usage.borderMarginMm));
    } else {
        m_footLabel->setText(tr("Colors: %1").arg(rows));
    }
}

/// 材料表打印 HTML：面板当前显示什么就打印什么 —— 直接镜像 QTableWidget 的
/// 可见列（列可见性 / 符号底色 / 换算后的用量字符串），分页交给 QTextDocument。
QString DockLegend::materialListHtml() const
{
    const int rows = m_table->rowCount();
    // 可见列集合（隐藏列不打印；符号、序号恒打印）
    struct ColumnDef { int index; bool alignRight; };
    QVector<ColumnDef> cols;
    cols.append({ kColumnNo, false });
    cols.append({ kColumnSymbol, false });
    for (int c = kColumnCode; c < kColumnCount; ++c) {
        if (!m_table->isColumnHidden(c)) {
            cols.append({ c, c == kColumnStitches });
        }
    }

    // 整体缩放（打印预览顶栏可调 %）：字号与内边距按比例缩放。
    // 100% 基准 = 34pt（初始大小，原 12pt 基准的 280% 档放大而来），
    // 预览顶栏 50%–300% 在该基准上再缩放。
    const int scalePct = qBound(50, m_usage.printLegendScalePct, 300);
    const double scale = scalePct / 100.0;
    const int fontPt = qRound(34.0 * scale);
    const int cellPad = qMax(1, qRound(14.0 * scale));
    const int headPadV = cellPad;
    const int headPadH = qMax(2, qRound(34.0 * scale));

    // —— HTML ——
    // 作用范围（总盘 / 盘 N）放进标题，多盘拼图时便于分清打印的是哪块
    const QString scope = m_currentPlate < 0
        ? tr("All plates") : tr("Plate %1").arg(m_currentPlate + 1);
    QString html = QStringLiteral(
                       "<html><body>"
                       "<h2 align=center>%1 · %2</h2>"
                       // 不设 table 宽度：列宽按内容生成（不占满页面），字号加大便于阅读
                       "<table border=\"1\" cellspacing=\"0\" "
                       "cellpadding=\"%3\" style=\"font-size:%4pt;\">"
                       "<tr>")
                       .arg(tr("Material list").toHtmlEscaped(), scope.toHtmlEscaped(),
                            QString::number(cellPad), QString::number(fontPt));
    for (const ColumnDef& cd : cols) {
        const QString h =
            m_table->horizontalHeaderItem(cd.index)
                ? m_table->horizontalHeaderItem(cd.index)->text()
                : QString();
        html += QStringLiteral("<th align=center style=\"padding:%1px %2px;\">%3</th>")
                    .arg(QString::number(headPadV), QString::number(headPadH),
                         h.toHtmlEscaped());
    }
    html += QStringLiteral("</tr>");
    for (int r = 0; r < rows; ++r) {
        html += QStringLiteral("<tr>");
        for (const ColumnDef& cd : cols) {
            QTableWidgetItem* it = m_table->item(r, cd.index);
            const QString text = it ? it->text().toHtmlEscaped() : QString();
            if (cd.index == kColumnSymbol && it) {
                // 符号 = 色块底 + 反差文字（与画布所见一致）
                const QColor bg = it->background().color();
                const QString fg =
                    bg.lightness() < 128 ? QStringLiteral("white")
                                         : QStringLiteral("black");
                html += QStringLiteral(
                            "<td align=center bgcolor=\"%1\" style=\"font-weight:bold;\">"
                            "<font color=%2>%3</font></td>")
                            .arg(bg.name(), fg, text);
            } else {
                html += QStringLiteral("<td align=%1>%2</td>")
                            .arg(cd.alignRight ? QStringLiteral("right")
                                               : QStringLiteral("center"),
                                 text);
            }
        }
        html += QStringLiteral("</tr>");
    }
    html += QStringLiteral("</table></body></html>");
    return html;
}

/// 打印材料表：先弹拼版预览（顶部纸张/方向/边距实时重算，总览 / 单张 +
/// 上一张 下一张 + 缩放），预览点「打印」才弹系统打印对话框落纸。
/// 与图纸打印同一套交互（预览永远先行））。
void DockLegend::printMaterialList()
{
    if (m_table->rowCount() <= 0) {
        emit statusMessage(tr("No chart"), 3000);
        return;
    }
    qInfo("[CM-print] legend preview rows=%d", m_table->rowCount());
    QTextDocument doc;
    doc.setDocumentMargin(0); // 页边距统一由打印机（顶栏「边距」）控制
    doc.setHtml(materialListHtml());

    // 参考打印机：纸张/方向/边距（初值取图纸打印设置，顶栏可改）
    QPrinter refPrinter(QPrinter::HighResolution);
    refPrinter.setFullPage(false);

    QDialog preview(this);
    preview.setWindowTitle(tr("Print material list"));
    preview.resize(940, 720);
    auto* previewLay = new QVBoxLayout(&preview);

    auto* topLay = new QHBoxLayout;
    topLay->addWidget(new QLabel(tr("Paper size:"), &preview));
    auto* paperCombo = new QComboBox(&preview);
    const QList<QPair<int, QString>> papers = {
        { 0, QStringLiteral("A5") }, { 1, QStringLiteral("A4") },
        { 2, QStringLiteral("A3") }, { 3, QStringLiteral("Letter") },
        { 4, QStringLiteral("B5") },
    };
    for (const auto& p : papers) {
        paperCombo->addItem(p.second, p.first);
    }
    const int kindIdx = paperCombo->findData(m_usage.printPaperKind);
    paperCombo->setCurrentIndex(kindIdx < 0 ? 1 : kindIdx);
    topLay->addWidget(paperCombo);

    auto* orientCombo = new QComboBox(&preview);
    orientCombo->addItem(tr("Portrait"), 0);
    orientCombo->addItem(tr("Landscape"), 1);
    orientCombo->setCurrentIndex(m_usage.printLandscape ? 1 : 0);
    topLay->addWidget(orientCombo);

    topLay->addSpacing(8);
    topLay->addWidget(new QLabel(tr("Margin:"), &preview));
    auto* marginSpin = new QDoubleSpinBox(&preview);
    marginSpin->setRange(0.0, 30.0);
    marginSpin->setSingleStep(1.0);
    marginSpin->setDecimals(1);
    marginSpin->setSuffix(tr(" mm"));
    marginSpin->setFixedWidth(88); // 留足单位显示宽度
    marginSpin->setValue(m_usage.printMarginMm);
    topLay->addWidget(marginSpin);

    topLay->addSpacing(8);
    topLay->addWidget(new QLabel(tr("Scale:"), &preview));
    auto* scaleSpin = new QSpinBox(&preview);
    scaleSpin->setRange(50, 300); // 整体缩放：50% ~ 300%
    scaleSpin->setSingleStep(5);
    scaleSpin->setSuffix(tr(" %"));
    scaleSpin->setFixedWidth(80);
    scaleSpin->setValue(m_usage.printLegendScalePct);
    topLay->addWidget(scaleSpin);

    topLay->addSpacing(10);
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

    const auto* topPal = &preview.palette();
    auto* zoomOutBtn = new QPushButton(&preview);
    zoomOutBtn->setIcon(IconFactory::make(IconFactory::Kind::ZoomOut,
                                          topPal->color(QPalette::WindowText)));
    zoomOutBtn->setIconSize(QSize(16, 16));
    zoomOutBtn->setToolTip(tr("Zoom out"));
    zoomOutBtn->setFixedWidth(32);
    auto* zoomInBtn = new QPushButton(&preview);
    zoomInBtn->setIcon(IconFactory::make(IconFactory::Kind::ZoomIn,
                                         topPal->color(QPalette::WindowText)));
    zoomInBtn->setIconSize(QSize(16, 16));
    zoomInBtn->setToolTip(tr("Zoom in"));
    zoomInBtn->setFixedWidth(32);
    auto* zoomLabel = new QLabel(QStringLiteral("100%"), &preview);
    topLay->addWidget(zoomOutBtn);
    topLay->addWidget(zoomInBtn);
    topLay->addWidget(zoomLabel);
    auto* prevBtn = new QPushButton(&preview);
    prevBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    prevBtn->setIconSize(QSize(16, 16));
    prevBtn->setToolTip(tr("Previous page"));
    prevBtn->setFixedWidth(36);
    auto* nextBtn = new QPushButton(&preview);
    nextBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    nextBtn->setIconSize(QSize(16, 16));
    nextBtn->setToolTip(tr("Next page"));
    nextBtn->setFixedWidth(36);
    topLay->addWidget(prevBtn);
    topLay->addWidget(nextBtn);
    auto* pageLabel = new QLabel(&preview);
    topLay->addWidget(pageLabel);
    topLay->addStretch(1);
    previewLay->addLayout(topLay);

    // 视图区：0 = 总览网格，1 = 单张
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

    // —— 状态与重建：纸张/方向/边距改动 → 重排版 → 重渲染 ——
    qreal pageWDev = 0, pageHDev = 0; // 单页设备像素（参考打印机）
    int pageCount = 0;
    int viewMode = 0;
    double zoom = 1.0;
    int curPage = 0;
    auto configureRef = [&]() {
        static const QPageSize::PageSizeId kPaperIds[] = {
            QPageSize::A5, QPageSize::A4, QPageSize::A3, QPageSize::Letter, QPageSize::B5,
        };
        refPrinter.setPageSize(QPageSize(
            kPaperIds[qBound(0, paperCombo->currentData().toInt(), 4)]));
        refPrinter.setPageOrientation(orientCombo->currentIndex() == 1
                                          ? QPageLayout::Landscape : QPageLayout::Portrait);
        const qreal mm = marginSpin->value();
        refPrinter.setPageMargins(QMarginsF(mm, mm, mm, mm), QPageLayout::Millimeter);
    };
    auto updateNav = [&]() {
        const bool single = viewMode == 1;
        curPage = pageCount > 0 ? qBound(0, curPage, pageCount - 1) : 0;
        prevBtn->setEnabled(single && pageCount > 0 && curPage > 0);
        nextBtn->setEnabled(single && pageCount > 0 && curPage < pageCount - 1);
        zoomInBtn->setEnabled(single && pageCount > 0 && zoom < 4.0);
        zoomOutBtn->setEnabled(single && pageCount > 0 && zoom > 0.4);
        if (pageCount > 0) {
            pageLabel->setText(tr("Page %1 of %2").arg(curPage + 1).arg(pageCount));
        }
    };
    // 画第 pageIdx 页（renderTarget = 参考打印机几何），返回画好的 QPixmap
    auto renderDocPage = [&](int pageIdx, qreal targetW) {
        const qreal scale = targetW / qMax(1.0, pageWDev);
        // 总览无灰边；单张四周灰底衬托白纸边界
        const bool padded = targetW > 400.0;
        const int pad = padded ? 14 : 2;
        QPixmap pm(int(qCeil(pageWDev * scale)) + pad * 2,
                   int(qCeil(pageHDev * scale)) + pad * 2);
        pm.fill(padded ? QColor(168, 168, 168) : Qt::white);
        QPainter p(&pm);
        p.translate(pad, pad);
        p.scale(scale, scale);
        if (padded) {
            p.fillRect(QRectF(0, 0, pageWDev, pageHDev), Qt::white);
        }
        // 文档第 pageIdx 页：按页高平移 + 绘制内容区
        p.translate(0, -pageIdx * pageHDev);
        doc.drawContents(&p, QRectF(0, pageIdx * pageHDev, pageWDev, pageHDev));
        p.end();
        return pm;
    };
    auto renderSingle = [&]() {
        if (pageCount > 0) {
            const qreal singleW = qMax(360.0, qreal(preview.width() - 72)) * zoom;
            singleCard->setPixmap(renderDocPage(curPage, singleW));
            singleCard->adjustSize();
        } else {
            singleCard->setPixmap(QPixmap());
        }
        updateNav();
    };
    auto rebuildAll = [&]() {
        configureRef();
        qInfo("[CM-print] legend rebuild paper=%d landscape=%d margin=%.1f scale=%d%%",
              paperCombo->currentData().toInt(), orientCombo->currentIndex(),
              marginSpin->value(), scaleSpin->value());
        // 缩放（字号/内边距）变化 → 重新生成 HTML 再排版（表格内容不变，代价很小）
        doc.setHtml(materialListHtml());
        const QRectF page = refPrinter.pageRect(QPrinter::DevicePixel);
        pageWDev = page.width();
        pageHDev = page.height();
        doc.setPageSize(QSizeF(pageWDev, pageHDev));
        pageCount = qMax(1, doc.pageCount());
        curPage = 0;
        pageLabel->setText(tr("Page %1 of %2").arg(curPage + 1).arg(pageCount));
        // 总览网格重建
        while (auto* child = grid->takeAt(0)) {
            if (child->widget()) {
                delete child->widget();
            }
            delete child;
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const qreal thumbW = 240.0;
        for (int pi = 0; pi < pageCount; ++pi) {
            auto* card = new QLabel(&preview);
            card->setPixmap(renderDocPage(pi, thumbW));
            card->setAlignment(Qt::AlignCenter);
            card->setFrameShape(QFrame::StyledPanel);
            card->setToolTip(tr("Page %1 of %2").arg(pi + 1).arg(pageCount));
            grid->addWidget(card, pi / 4, pi % 4); // 中缝 4 列排布（行序同页序）
        }
        QApplication::restoreOverrideCursor();
        renderSingle();
    };

    auto* debounce = new QTimer(&preview);
    debounce->setSingleShot(true);
    debounce->setInterval(250);
    connect(debounce, &QTimer::timeout, &preview, rebuildAll);
    connect(paperCombo, &QComboBox::currentIndexChanged, &preview,
            [debounce](int) { debounce->start(); });
    connect(orientCombo, &QComboBox::currentIndexChanged, &preview,
            [debounce](int) { debounce->start(); });
    connect(marginSpin, &QDoubleSpinBox::valueChanged, &preview,
            [debounce](double) { debounce->start(); });
    connect(scaleSpin, &QSpinBox::valueChanged, &preview,
            [this, debounce, scaleSpin](int value) {
                m_usage.printLegendScalePct = scaleSpin->value();
                // 与图纸打印参数一致：改动即时写回（主窗口持久化该字段）
                emit printLegendScaleChanged(value);
                debounce->start();
            });
    connect(viewGroup, &QButtonGroup::idClicked, &preview, [&](int idx) {
        viewMode = idx;
        stack->setCurrentIndex(idx);
        updateNav();
        if (idx == 1) {
            renderSingle();
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
        if (curPage < pageCount - 1) {
            curPage++;
            renderSingle();
        }
    });

    rebuildAll();
    qInfo("[CM-print] legend preview built pages=%d", pageCount);

    if (preview.exec() == QDialog::Accepted) {
        // 正式打印：与预览一致的纸张/方向/边距（pageLayout 一次性带入），
        // QPrintDialog 允许改打印机，字体等仍按预览排版（QTextDocument 重排）
        QPrinter printer(QPrinter::HighResolution);
        printer.setFullPage(false);
        printer.setPageLayout(refPrinter.pageLayout());
        QPrintDialog dialog(&printer, this);
        dialog.setWindowTitle(tr("Print material list"));
        const int verdict = dialog.exec();
        qInfo("[CM-print] legend print dialog verdict=%d dpi=%d", verdict,
              printer.logicalDpiX());
        if (verdict != QDialog::Accepted) {
            return;
        }
        doc.print(&printer);
        emit statusMessage(tr("Material list printed"), 4000);
        qInfo("[CM-print] legend print done");
    }
}

void DockLegend::setCraftMode(uint32_t mode)
{
    {
        QSignalBlocker block1(m_crossRadio);
        QSignalBlocker block2(m_beadRadio);
        QSignalBlocker block3(m_perlerRadio);
        if (mode == cm::craft::PERLER) {
            m_perlerRadio->setChecked(true);
        } else if (mode == cm::craft::BEAD) {
            m_beadRadio->setChecked(true);
        } else {
            m_crossRadio->setChecked(true);
        }
    }
    m_craftMode = mode;
    applyModeColumns();
    if (m_table->rowCount() > 0) {
        refresh(m_snapshot);
    }
}

void DockLegend::applyModeColumns()
{
    if (!m_table) {
        return;
    }
    const bool cross = m_craftMode == cm::craft::CROSS_STITCH;
    const bool beadMode = !cross;
    m_table->setColumnHidden(kColumnName, !m_usage.showColorNameColumn);
    m_table->setColumnHidden(kColumnStitches, !(cross && m_usage.showStitchesColumn));
    m_table->setColumnHidden(kColumnBeads, !(beadMode && m_usage.showBeadsColumn));
    m_table->setColumnHidden(kColumnLength, !(cross && m_usage.showThreadColumn));
    if (auto* headerItem = m_table->horizontalHeaderItem(kColumnBeads)) {
        headerItem->setText(m_craftMode == cm::craft::PERLER ? tr("Beads (perler)")
                                                            : tr("Beads"));
    }
}

void DockLegend::setUsageSettings(const CrossStitchSettings& settings)
{
    m_usage = settings;
    m_usage.sanitize();
    if (m_table && m_table->rowCount() > 0) {
        refresh(m_snapshot); // 用新设置重算用线 / 珠子等
    }
}

void DockLegend::setPlateCount(int count)
{
    m_plateCount = qBound(1, count, kMaxPlateCount);
    if (!m_plateCombo) {
        return;
    }
    QSignalBlocker block(m_plateCombo);
    const int oldIndex = m_plateCombo->currentIndex();
    m_plateCombo->clear();
    m_plateCombo->addItem(tr("All plates"), -1);
    for (int i = 0; i < m_plateCount; ++i) {
        m_plateCombo->addItem(tr("Plate %1").arg(i + 1), i);
    }
    // 尽量保持之前的选中项（新盘数下最多到盘N）
    const int newIndex = qBound(0, oldIndex, m_plateCount);
    m_plateCombo->setCurrentIndex(newIndex);
    m_currentPlate = newIndex - 1;
    m_addPlateButton->setEnabled(m_plateCount < kMaxPlateCount);
}

void DockLegend::setCurrentPlateOption(int plate, bool emitChanged)
{
    if (!m_plateCombo) {
        return;
    }
    const int idx = plate + 1;
    if (idx < 0 || idx >= m_plateCombo->count()) {
        return;
    }
    if (emitChanged) {
        m_plateCombo->setCurrentIndex(idx); // 触发 currentIndexChanged → plateSelected
    } else {
        QSignalBlocker block(m_plateCombo);
        m_plateCombo->setCurrentIndex(idx);
    }
}

void DockLegend::setAccentColor(const QColor& color)
{
    m_accentColor = color;
    // 主题色变了：整表重绘（序号高亮与外框由 delegate 同帧绘制）
    if (m_table) {
        m_table->viewport()->update();
    }
}

void DockLegend::selectColorIndex(int colorIndex)
{
    m_selectedIndex = colorIndex;
    if (!m_table || m_table->rowCount() == 0) {
        return;
    }
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, kColumnCode)->data(Qt::UserRole).toInt() == colorIndex) {
            m_table->setCurrentCell(r, kColumnNo);
            return;
        }
    }
    // 新色未出现在当前表格：回退第一行
    m_selectedIndex = m_table->item(0, kColumnCode)->data(Qt::UserRole).toInt();
    m_table->setCurrentCell(0, kColumnNo);
}

void DockLegend::showContextMenu(const QPoint& pos)
{
    const int row = m_table->rowAt(pos.y());
    if (row < 0) {
        return;
    }
    const int colorIndex = m_table->item(row, kColumnCode)->data(Qt::UserRole).toInt();
    const QString code =
        colorIndex < m_snapshot.colorCodes.size() ? m_snapshot.colorCodes.at(colorIndex) : QString();

    QMenu menu(this);
    QAction* symbolAction = menu.addAction(tr("Change symbol..."));
    QAction* mergeAction = menu.addAction(tr("Replace with another colour..."));
    QAction* deleteAction = menu.addAction(tr("Delete colour"));

    QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (!chosen) {
        return;
    }
    if (chosen == symbolAction) {
        onChangeSymbol(colorIndex);
        return;
    }
    if (chosen == mergeAction) {
        onMergeColor();
        return;
    }
    if (chosen == deleteAction) {
        emit colorDeleteRequested(colorIndex);
    }
}

void DockLegend::onChangeSymbol(int colorIndex)
{
    if (colorIndex < 0 || colorIndex >= m_snapshot.symbols.size()) {
        return;
    }
    // 类型可切换的符号对话框（字母 / 数字 / 特殊符号）
    const QChar sym = cmPromptChangeSymbol(this, colorIndex, m_snapshot.symbols,
                                           m_snapshot.symbolAlphabet, m_snapshot.colorCodes);
    if (!sym.isNull()) {
        emit symbolChangeRequested(colorIndex, static_cast<uint>(sym.unicode()));
    }
}

void DockLegend::onMergeColor()
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_project || !m_project->isValid()) {
        return;
    }
    const int from = m_table->item(row, kColumnCode)->data(Qt::UserRole).toInt();

    // 替换颜色：页1 与已有颜色互换；或  从色卡库选新色替换。
    const CmMergeChoice choice = cmPromptMergeTarget(this, from, m_snapshot.colorCodes,
                                                     m_snapshot.colorNames,
                                                     m_snapshot.palette,
                                                     static_cast<int>(m_craftMode));
    if (choice.kind == 0 && choice.index >= 0 && choice.index != from) {
        // 与已有颜色互换（本色针全部改用目标色）
        emit colorMerged(from, choice.index);
    } else if (choice.kind == 1 && !choice.code.isEmpty()) {
        // 从色卡库选的新色：由 MainWindow 加色后合并（替换）
        emit colorReplaceWithCardRequested(from, choice.code);
    }
}

// LegendTable 在本文件内定义且无 Q_OBJECT，需包含 AUTOMOC 生成的
// moc
#include "dock_legend.moc"
