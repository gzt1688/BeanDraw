#pragma once

#include "ffi_bind/rust_api.h"

#include "canvas/scene.h"

#include "DockManager.h"

#include <QTableWidget>
#include <QLineEdit>
#include <QWidget>

class QComboBox;
class QRadioButton;

class DockMaterial : public ads::CDockWidget
{
    Q_OBJECT

public:
    explicit DockMaterial(ads::CDockManager* manager, QWidget* parent = nullptr);

    /// 刷新色卡：按当前卡片类型加载完整色卡（绣线 / 拼豆）。
    void refreshPalette(const ChartSnapshot& snapshot);

    /// 手作品类模式联动：0/1 → DMC 绣线卡，2 → Artkal 拼豆卡。
    void setCraftMode(uint32_t mode);

    /// 语言切换：重设面板内全部 UI 文字（单选框 / 搜索占位 / 表头）。
    void retranslateUi();

signals:
    /// 双击色卡某行 → 把该色号加入底部颜色面板（图纸调色板）。
    void colorCodeDoubleClicked(const QString& code);
    /// 手作品类单选框切换（0 十字绣 / 1 珠绣 / 2 拼豆），主窗口据此切换图纸。
    void craftModeChanged(uint32_t mode);

private slots:
    void onSearchChanged(const QString& text);

private:
    void loadLibrary(const QString& kind);

    QRadioButton* m_yarnRadio = nullptr;   // 十字绣 / 珠绣（共用 DMC 卡）
    QRadioButton* m_perlerRadio = nullptr;
    /// 十字绣/珠绣共用按钮选中时回发给主窗口的手作品类
    /// （主窗口此前告诉我的模式，PERLER 之外的最近值）。
    uint32_t m_lastYarnMode = 0;
    QLineEdit* m_search = nullptr;
    QTableWidget* m_paletteTable = nullptr;
    QString m_kind = QStringLiteral("yarn");
    QString m_loadedKind;
};
