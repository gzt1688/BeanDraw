#pragma once

#include "canvas/scene.h"

#include <QToolBar>
#include <QColor>
#include <QVector>

class QScrollArea;
class QWidget;
class QLabel;
class QLineEdit;
class QToolButton;

class DockColors : public QToolBar
{
    Q_OBJECT

public:
    explicit DockColors(QWidget* parent = nullptr);

    /// 从图表快照刷新底部颜色面板（图纸调色板）。
    void refresh(const ChartSnapshot& snapshot);

    /// 程序化设置当前选中色（不触发 colorActivated）。
    void setActiveIndex(int index);

    /// 当前色块标签模式：0 符号 / 1 色号 / 2 无（画布格子文本与之同步）。
    static int labelMode();

    /// 手作品类模式（0 十字绣 / 1 珠绣 / 2 拼豆）：决定"从色卡替换"的色卡类型。
    void setCraftMode(uint32_t mode) { m_craftMode = mode; }

    /// 语言切换：重设面板内全部 UI 文字（按钮 / 占位符 / 标签模式文案）。
    void retranslateUi();

signals:
    /// 选中的绣色索引（图纸调色板索引）。
    void colorActivated(int index);
    /// 请求清理未使用颜色。
    void pruneRequested();
    /// 颜色块拖拽合并：fromIndex 被合并进 toIndex（全局换色，from 色块随之消失）。
    void colorMerged(int fromIndex, int toIndex);
    /// 从色卡库选新色号替换（from 色的针全部改用该新色，from 随之移除）。
    void colorReplaceWithCardRequested(int from, const QString& code);
    /// 右击色块"更改符号"：与材料表一致的语义。
    void symbolChangeRequested(int colorIndex, uint symbol);
    /// 右击色块"删除颜色"：与材料表一致的语义。
    void colorDeleteRequested(int colorIndex);
    /// 色块标签三态切换（符号/色号/无）：画布格子文本同步跟随。
    void labelModeChanged(int mode);

protected:
    /// 强制内容容器铺满工具栏内区（QToolBarLayout 垂直方向不按 Expanding 拉伸）。
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSearchChanged(const QString& text);
    void onCleanClicked();
    /// 色块标签三态切换：符号 → 色号 → 无。
    void onLabelModeClicked();

private:
    void clearBlocks();
    void buildBlocks();
    void onBlockClicked(int index);
    /// 右击色块：弹出与材料表一致的上下文菜单并处理各项动作。
    void onBlockContextMenu(int index, const QPoint& globalPos);
    /// 把当前标签模式同步到按钮文字。
    void applyLabelMode();
    /// 强制内容容器铺满底栏内区（修复 QToolBarLayout 按 sizeHint 压矮容器）。
    void enforceBarGeometry();
    /// 按「色块总宽是否超出视口」在两档尺寸间自动切换：放得下用大档，
    /// 放不下切紧凑档，让出横向滚动条需要的那 8px。
    void applyBlockSizePolicy();
    /// 当前是否处于紧凑档（放不下、需要滚动条）。
    bool needsCompactBlocks() const;

    QWidget* m_bar = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_container = nullptr;
    QLineEdit* m_search = nullptr;
    QToolButton* m_cleanBtn = nullptr;
    QToolButton* m_labelBtn = nullptr;

    QVector<QColor> m_palette;
    QVector<QString> m_codes;
    QVector<QString> m_names;
    QVector<QChar> m_symbols;
    QVector<QChar> m_symbolAlphabet; ///< 候选符号集（更改符号对话框用）
    int m_currentIndex = -1;
    int m_labelMode = 1; ///< 0 符号 / 1 色号 / 2 无
    uint32_t m_craftMode = 0; ///< 0 十字绣 / 1 珠绣 / 2 拼豆（色卡替换类型）
};
