#pragma once

// 共享：材料表与底部颜色栏的右键菜单里"更改符号 / 替换颜色"两个对话框逻辑。
// 抽出为内联自由函数，让两个 dock 复用同一套交互与同一份翻译（context 固定为
// "DockLegend"，复用已有 .ts 条目），避免两处各写一遍、翻译对不上。

#include "ffi_bind/rust_api.h"

#include <QApplication>
#include <QButtonGroup>
#include <QChar>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QBrush>
#include <QIcon>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QVBoxLayout>
#include <QVector>

/// 弹出"更改符号"对话框（类型可切换：字母 / 数字 / 特殊符号）。
/// 返回用户选中的符号字符；取消时返回 null 字符。
inline QChar cmPromptChangeSymbol(QWidget* parent, int colorIndex,
                                  const QVector<QChar>& symbols,
                                  const QVector<QChar>& alphabet,
                                  const QVector<QString>& codes)
{
    Q_UNUSED(alphabet) // 候选改为三类型固定集，不再依赖 symbol_alphabet
    if (colorIndex < 0 || colorIndex >= symbols.size()) {
        return QChar();
    }
    const QChar current = symbols.at(colorIndex);
    const QString code =
        colorIndex < codes.size() ? codes.at(colorIndex) : QString::number(colorIndex);

    // 三类型候选：字母 A-Z / 数字 0-9 / 常用特殊符号
    QVector<QVector<QChar>> groups(3);
    for (char c = 'A'; c <= 'Z'; ++c) {
        groups[0].append(QChar::fromLatin1(c));
    }
    for (char c = '0'; c <= '9'; ++c) {
        groups[1].append(QChar::fromLatin1(c));
    }
    const QString specials = QStringLiteral("\u2605\u2606\u25cf\u25cb\u25c6\u25c7\u25b2\u25b3"
                                           "\u25a0\u25a1\u2665\u2666\u260e\u2709\u273f\u271a"
                                           "\u271c\u263a\u2660\u2663\u266b\u266a\u2600\u263e");
    for (const QChar& c : specials) {
        groups[2].append(c);
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QCoreApplication::translate("DockLegend", "Change symbol"));
    auto* lay = new QVBoxLayout(&dlg);

    // 类型切换：三个互斥按钮（字母 / 数字 / 特殊符号）
    const QString accent = QApplication::palette().color(QPalette::Highlight).name();
    // 类型按钮：选中整块主题高亮 + 白字；等宽均分，切换类型时位置不跳动
    const QString typeBtnStyle = QStringLiteral(
        "QPushButton { padding: 4px 12px; border: 1px solid #999; background: transparent; }"
        "QPushButton:hover { border: 2px solid #888; }"
        "QPushButton:checked { background-color: %1; color: #ffffff; border: 2px solid %1;"
        " font-weight: bold; }")
                                     .arg(accent);
    auto* typeRow = new QWidget(&dlg);
    auto* typeLay = new QHBoxLayout(typeRow);
    typeLay->setContentsMargins(0, 0, 0, 0);
    typeLay->setSpacing(6);
    auto* typeGroup = new QButtonGroup(&dlg);
    typeGroup->setExclusive(true);
    const QStringList typeLabels = {
        QCoreApplication::translate("DockLegend", "Letters"),
        QCoreApplication::translate("DockLegend", "Digits"),
        QCoreApplication::translate("DockLegend", "Special symbols"),
    };
    for (int i = 0; i < typeLabels.size(); ++i) {
        auto* b = new QPushButton(typeLabels.at(i), typeRow);
        b->setCheckable(true);
        b->setStyleSheet(typeBtnStyle);
        typeGroup->addButton(b, i);
        typeLay->addWidget(b, 1);
    }
    typeGroup->button(0)->setChecked(true);
    lay->addWidget(typeRow);

    auto* gridWidget = new QWidget(&dlg);
    auto* grid = new QGridLayout(gridWidget);
    grid->setSpacing(4);
    // 固定内容区高度（按 4 行符号 + 间距算）：
    // 切换类型时按钮行位置绝对不动，只是下方网格内容变化
    gridWidget->setMinimumHeight(4 * 42 + 3 * 4 + 8);
    lay->addWidget(gridWidget);

    QChar result;
    const auto rebuild = [&](int gi) {
        while (grid->count() > 0) {
            QLayoutItem* item = grid->takeAt(0);
            if (QWidget* w = item->widget()) {
                w->deleteLater();
            }
            delete item;
        }
        int col = 0, row = 0;
        for (const QChar& c : groups.at(gi)) {
            auto* btn = new QPushButton(QString(c), gridWidget);
            btn->setFixedSize(42, 42);
            btn->setCheckable(true);
            btn->setChecked(c == current);
            if (symbols.contains(c) && c != current) {
                const int owner = symbols.indexOf(c);
                const QString ownerCode =
                    owner < codes.size() ? codes.at(owner) : QString::number(owner);
                btn->setToolTip(QCoreApplication::translate("DockLegend", "Swap with %1")
                                    .arg(ownerCode));
            } else if (c == current) {
                btn->setToolTip(QCoreApplication::translate("DockLegend", "Current symbol"));
            }
            // 控件级样式覆盖 app 级 QSS：全局按钮样式若带 padding/font 会把字符挤裁，
            // 这里强制 padding 0、大字号、checked 整块主题高亮（与类型按钮一致）。
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { padding: 0; margin: 0; border: 1px solid #999;"
                " font-size: 18px; background: transparent; }"
                "QPushButton:hover { border: 2px solid #888; }"
                "QPushButton:checked { background-color: %1; color: #ffffff;"
                " border: 2px solid %1; }")
                .arg(accent));
            QObject::connect(btn, &QPushButton::clicked, &dlg,
                    [&dlg, &result, c]() { result = c; dlg.accept(); });
            grid->addWidget(btn, row, col);
            if (++col >= 8) {
                col = 0;
                ++row;
            }
        }
    };
    QObject::connect(typeGroup, &QButtonGroup::idClicked, &dlg,
            [&rebuild](int i) { rebuild(i); });
    rebuild(0);
    dlg.exec();
    return result;
}

/// 替换颜色选择结果：kind 0 = 与已有颜色互换（index 有效）；kind 1 = 从色卡库选新色（code 有效）。
struct CmMergeChoice {
    int kind = 0;
    int index = -1;
    QString code;
};

/// 弹出"替换颜色"对话框：页1 与已有颜色互换；页2 从色卡库搜索选色。
/// colors 为图内已有颜色（页1 色块）；craftMode 决定色卡类型（拼豆→perler，其他→yarn）。
inline CmMergeChoice cmPromptMergeTarget(QWidget* parent, int fromIndex,
                                         const QVector<QString>& codes,
                                         const QVector<QString>& names,
                                         const QVector<QColor>& colors,
                                         int craftMode)
{
    CmMergeChoice out;
    const auto tr = [](const char* s) {
        return QCoreApplication::translate("DockLegend", s);
    };
    QDialog dlg(parent);
    dlg.setWindowTitle(QCoreApplication::translate("DockLegend", "Replace colour"));
    auto* lay = new QVBoxLayout(&dlg);

    // 类型切换：两个互斥按钮（与已有颜色互换 / 从色卡库选色替换）
    const QString accent = QApplication::palette().color(QPalette::Highlight).name();
    // 类型按钮：选中整块主题高亮 + 白字；等宽均分，切换类型时位置不跳动
    const QString typeBtnStyle = QStringLiteral(
        "QPushButton { padding: 4px 12px; border: 1px solid #999; background: transparent; }"
        "QPushButton:hover { border: 2px solid #888; }"
        "QPushButton:checked { background-color: %1; color: #ffffff; border: 2px solid %1;"
        " font-weight: bold; }")
                                     .arg(accent);
    auto* typeRow = new QWidget(&dlg);
    auto* typeLay = new QHBoxLayout(typeRow);
    typeLay->setContentsMargins(0, 0, 0, 0);
    typeLay->setSpacing(6);
    auto* typeGroup = new QButtonGroup(&dlg);
    typeGroup->setExclusive(true);
    const QStringList typeLabels = {QCoreApplication::translate("DockLegend",
                                        "Existing colour in chart"),
                                    QCoreApplication::translate("DockLegend",
                                        "Pick from colour card")};
    for (int i = 0; i < typeLabels.size(); ++i) {
        auto* b = new QPushButton(typeLabels.at(i), typeRow);
        b->setCheckable(true);
        b->setStyleSheet(typeBtnStyle);
        typeGroup->addButton(b, i);
        typeLay->addWidget(b, 1);
    }
    typeGroup->button(0)->setChecked(true);
    lay->addWidget(typeRow);

    auto* stack = new QStackedWidget(&dlg);
    lay->addWidget(stack);

    // 三列表格通用配置：色块 / 色号 / 名称 + 网格线（垂直格线清晰分隔三列）
    const QString tableStyle = QStringLiteral(
        "QTableView { gridline-color: #c0c0c0; selection-background-color: %1;"
        " selection-color: #ffffff; }")
                                   .arg(accent);
    const auto makeTable = [&](QWidget* parent) {
        auto* table = new QTableView(parent);
        auto* model = new QStandardItemModel(0, 3, table);
        model->setHeaderData(0, Qt::Horizontal, QCoreApplication::translate("DockLegend", "Colour"));
        model->setHeaderData(1, Qt::Horizontal, QCoreApplication::translate("DockLegend", "Code"));
        model->setHeaderData(2, Qt::Horizontal, tr("Name"));
        table->setModel(model);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(true);
        table->setGridStyle(Qt::SolidLine);
        table->setStyleSheet(tableStyle);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->setColumnWidth(0, 62);
        table->setColumnWidth(1, 92);
        table->setMinimumHeight(260);
        return table;
    };

    // 页0：已有颜色表格（色块 + 色号 + 名称）
    auto* table0 = makeTable(stack);
    {
        auto* model = qobject_cast<QStandardItemModel*>(table0->model());
        for (int idx = 0; idx < codes.size(); ++idx) {
            if (idx == fromIndex) {
                continue;
            }
            const QString name = idx < names.size() ? names.at(idx) : QString();
            const QColor col = idx < colors.size() ? colors.at(idx) : QColor();
            auto* ic = new QStandardItem(QString());
            ic->setBackground(QBrush(col.isValid() ? col : QColor(180, 180, 180))); // 整格填色占满
            auto* icd = new QStandardItem(codes.at(idx));
            auto* inm = new QStandardItem(name);
            ic->setData(idx, Qt::UserRole);
            model->appendRow({ic, icd, inm});
        }
    }
    stack->addWidget(table0);

    // 页1：色卡库（搜索框 + 三列表格，色块 + 色号 + 名称）
    auto* page1 = new QWidget(stack);
    auto* l1 = new QVBoxLayout(page1);
    auto* search = new QLineEdit(page1);
    search->setPlaceholderText(QCoreApplication::translate("DockLegend",
                                                "Search color / code..."));
    auto* table1 = makeTable(page1);
    l1->addWidget(search);
    l1->addWidget(table1);
    stack->addWidget(page1);

    // 色卡数据：按模式选卡（拼豆→perler，其他→yarn，与色卡面板一致）
    const QString kind = (craftMode == cm::craft::PERLER) ? QStringLiteral("perler")
                                                          : QStringLiteral("yarn");
    QVector<CmDmcColor> card;
    void* db = rust_api::materialDbNew();
    if (db) {
        const int total = rust_api::materialAll(db, kind, nullptr, 0);
        if (total > 0) {
            card.resize(total);
            rust_api::materialAll(db, kind, card.data(), static_cast<std::size_t>(total));
        }
        rust_api::materialDbFree(db);
    }
    const auto fillCard = [&](const QString& filter) {
        auto* model = qobject_cast<QStandardItemModel*>(table1->model());
        model->removeRows(0, model->rowCount());
        for (const CmDmcColor& c : card) {
            const QString code = QString::fromUtf8(c.code);
            const QString name = QString::fromUtf8(c.name);
            if (!filter.isEmpty() && !code.contains(filter, Qt::CaseInsensitive)
                && !name.contains(filter, Qt::CaseInsensitive)) {
                continue;
            }
            auto* ic = new QStandardItem(QString());
            ic->setBackground(QBrush(QColor(c.rgb[0], c.rgb[1], c.rgb[2]))); // 整格填色占满
            auto* icd = new QStandardItem(code);
            auto* inm = new QStandardItem(name);
            ic->setData(code, Qt::UserRole);
            model->appendRow({ic, icd, inm});
        }
    };
    fillCard(QString());
    QObject::connect(search, &QLineEdit::textChanged, &dlg, [&fillCard](const QString& t) {
        fillCard(t.trimmed());
    });

    QObject::connect(typeGroup, &QButtonGroup::idClicked, &dlg,
            [stack](int i) { stack->setCurrentIndex(i); });

    const auto onAccept = [&]() {
        if (typeGroup->checkedId() == 0) {
            const QModelIndex cur = table0->currentIndex();
            if (!cur.isValid()) {
                return;
            }
            auto* model = qobject_cast<QStandardItemModel*>(table0->model());
            out.kind = 0;
            out.index = model->item(cur.row(), 0)->data(Qt::UserRole).toInt();
            dlg.accept();
        } else {
            const QModelIndex cur = table1->currentIndex();
            if (!cur.isValid()) {
                return;
            }
            auto* model = qobject_cast<QStandardItemModel*>(table1->model());
            out.kind = 1;
            out.code = model->item(cur.row(), 0)->data(Qt::UserRole).toString();
            dlg.accept();
        }
    };
    QObject::connect(table0, &QTableView::doubleClicked, &dlg,
            [&onAccept](const QModelIndex&) { onAccept(); });
    QObject::connect(table1, &QTableView::doubleClicked, &dlg,
            [&onAccept](const QModelIndex&) { onAccept(); });
    auto* okBtn = new QPushButton(QCoreApplication::translate("DockLegend", "OK"), &dlg);
    auto* cancelBtn =
        new QPushButton(QCoreApplication::translate("DockLegend", "Cancel"), &dlg);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, [&onAccept]() { onAccept(); });
    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    auto* btnLay = new QHBoxLayout();
    btnLay->addStretch(1);
    btnLay->addWidget(cancelBtn);
    btnLay->addWidget(okBtn);
    lay->addLayout(btnLay);

    dlg.exec();
    return out;
}
