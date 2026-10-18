#include "dock_material.h"

#include "ui/i18n/tr_manager.h"
#include "ffi_bind/rust_api.h"
#include "ffi_bind/ffi_types.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cstddef>

DockMaterial::DockMaterial(ads::CDockManager* manager, QWidget* parent)
    : ads::CDockWidget(manager, QString(), parent)
{
    setObjectName(QStringLiteral("DockMaterial"));

    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // 手作品类二选一：驱动显示哪套材料色卡
    // 十字绣 / 珠绣共用 DMC 色卡（珠绣按珠色号同 DMC 体系）→ 合并为一个按钮；
    // 拼豆用 Artkal S 卡。
    m_yarnRadio = new QRadioButton(tr("Cross stitch / Bead embroidery"), widget);
    m_yarnRadio->setObjectName(QStringLiteral("materialYarnRadio"));
    m_perlerRadio = new QRadioButton(tr("Perler beads"), widget);
    m_perlerRadio->setObjectName(QStringLiteral("materialPerlerRadio"));

    auto* modeRow = new QWidget(widget);
    auto* modeLayout = new QHBoxLayout(modeRow);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(8);
    modeLayout->addWidget(m_perlerRadio);
    modeLayout->addWidget(m_yarnRadio);
    modeLayout->addStretch();
    layout->addWidget(modeRow);

    const auto reload = [this]() {
        // 构造早期 setChecked 会同步触发本回调，此时色卡表尚未创建
        if (!m_paletteTable) {
            return;
        }
        m_kind = m_perlerRadio->isChecked() ? QStringLiteral("perler")
                                            : QStringLiteral("yarn");
        loadLibrary(m_kind);
        // 切到拼豆 → 回发 PERLER；切回十字绣/珠绣 → 回发主窗口之前的模式
        // （两针法共用一张 DMC 卡，不再抢占当前针法状态）
        const uint32_t mode = m_perlerRadio->isChecked()
                                  ? cm::craft::PERLER
                                  : (m_lastYarnMode != cm::craft::PERLER
                                         ? m_lastYarnMode
                                         : cm::craft::CROSS_STITCH);
        emit craftModeChanged(mode);
    };
    connect(m_perlerRadio, &QRadioButton::toggled, this, [reload](bool on) {
        if (on) {
            reload();
        }
    });
    connect(m_yarnRadio, &QRadioButton::toggled, this, [reload](bool on) {
        if (on) {
            reload();
        }
    });
    // 默认模式：拼豆（软件打开即拼豆色卡，与材料表面板一致）。
    // 注意：不能在这里 setChecked——m_paletteTable 尚未创建，
    // toggled → reload → loadLibrary 会空指针解引用（启动崩溃）；
    // 已移到 m_paletteTable 创建之后（见下方 loadLibrary(m_kind) 前）。

    // 搜索框（按色号 / 名称过滤）
    m_search = new QLineEdit(widget);
    m_search->setPlaceholderText(tr("Search color / code..."));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &DockMaterial::onSearchChanged);
    layout->addWidget(m_search);

    // 完整色卡表：色块 / 色号 / 名称
    m_paletteTable = new QTableWidget(widget);
    m_paletteTable->setColumnCount(3);
    m_paletteTable->setHorizontalHeaderLabels({tr("Swatch"), tr("Code"), tr("Name")});
    m_paletteTable->horizontalHeader()->setStretchLastSection(true);
    m_paletteTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_paletteTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_paletteTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_paletteTable->setShowGrid(false);
    m_paletteTable->verticalHeader()->setVisible(false);
    m_paletteTable->setAlternatingRowColors(true);
    connect(m_paletteTable, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) {
                const auto* item = m_paletteTable->item(row, 1);
                if (item) {
                    emit colorCodeDoubleClicked(item->text());
                }
            });
    layout->addWidget(m_paletteTable);

    setWidget(widget);
    // 默认拼豆：表格已就绪，setChecked 触发的 reload → loadLibrary 可安全执行
    m_perlerRadio->setChecked(true);
    loadLibrary(m_kind);

    // 语言切换 → 面板内部完整重译
    connect(&TrManager::instance(), &TrManager::languageChanged,
            this, &DockMaterial::retranslateUi);
}

void DockMaterial::retranslateUi()
{
    m_yarnRadio->setText(tr("Cross stitch / Bead embroidery"));
    m_perlerRadio->setText(tr("Perler beads"));
    m_search->setPlaceholderText(tr("Search color / code..."));
    m_paletteTable->setHorizontalHeaderLabels({tr("Swatch"), tr("Code"), tr("Name")});
}

void DockMaterial::loadLibrary(const QString& kind)
{
    if (kind == m_loadedKind) {
        return;
    }
    m_loadedKind = kind;

    void* db = rust_api::materialDbNew();
    if (!db) {
        return;
    }
    const int total = rust_api::materialAll(db, kind, nullptr, 0);
    if (total > 0) {
        QVector<CmDmcColor> buf(total);
        const int got =
            rust_api::materialAll(db, kind, buf.data(), static_cast<std::size_t>(total));
        m_paletteTable->setRowCount(qMax(0, got));
        for (int i = 0; i < got; ++i) {
            const CmDmcColor& c = buf.at(i);
            const QColor color(c.rgb[0], c.rgb[1], c.rgb[2]);
            const QString code = QString::fromUtf8(c.code);
            const QString name = QString::fromUtf8(c.name);

            auto* swatch = new QTableWidgetItem();
            swatch->setBackground(color);
            swatch->setFlags(Qt::ItemIsEnabled);
            m_paletteTable->setItem(i, 0, swatch);
            m_paletteTable->setItem(i, 1, new QTableWidgetItem(code));
            m_paletteTable->setItem(i, 2, new QTableWidgetItem(name));
        }
    }
    rust_api::materialDbFree(db);
    onSearchChanged(m_search->text());
}

void DockMaterial::refreshPalette(const ChartSnapshot&)
{
    // 卡片表固定，无需刷新
}

void DockMaterial::setCraftMode(uint32_t mode)
{
    const bool perler = mode == cm::craft::PERLER;
    {
        QSignalBlocker block1(m_yarnRadio);
        QSignalBlocker block2(m_perlerRadio);
        m_perlerRadio->setChecked(perler);
        m_yarnRadio->setChecked(!perler);
    }
    m_kind = perler ? QStringLiteral("perler") : QStringLiteral("yarn");
    if (!perler) {
        // 记住当前针法：共用按钮被点回时原样回发（不改变主窗口的针法状态）
        m_lastYarnMode = mode;
    }
    loadLibrary(m_kind);
}

void DockMaterial::onSearchChanged(const QString& text)
{
    const QString t = text.trimmed().toLower();
    for (int i = 0; i < m_paletteTable->rowCount(); ++i) {
        bool visible = true;
        if (!t.isEmpty()) {
            const QString code = m_paletteTable->item(i, 1)->text().toLower();
            const QString name = m_paletteTable->item(i, 2)->text().toLower();
            visible = code.contains(t) || name.contains(t);
        }
        m_paletteTable->setRowHidden(i, !visible);
    }
}
