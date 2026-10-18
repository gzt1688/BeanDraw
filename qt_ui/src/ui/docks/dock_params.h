#pragma once

#include "DockManager.h"

#include <QFont>
#include <QList>
#include <QString>

class QAbstractButton;
class QCheckBox;
class QFontComboBox;
class QGroupBox;
class QLabel;
class QTextEdit;
class QSpinBox;

/// 变换面板（与图纸向导同停靠区，Tab 切换）：集中放置变换命令参数。
/// 当前承载镜像（左右/上下全图、自定义两点轴、自定义 XY 轴）、
/// 移动（X 移动 / Y 移动，支持方向键移动选区内容）与
/// 文字（文字转豆子：内容 / 字体 / 字高 / 旋转 / 粗体，文字工具点击画布放置）。
///
/// 交互约定：两个"模式"按钮（自定义镜像 / 自定义轴）独立可勾选，
/// 互斥由 MainWindow 在进入模式时统一处理（静默点亮本模式、取消另一）；
/// 本类用 m_silent 标志区分"用户点击（发信号）"与"程序置位（静默）"，
/// 避免工具切换过程中程序置位触发信号造成连锁。
class DockParams : public ads::CDockWidget
{
    Q_OBJECT

public:
    explicit DockParams(ads::CDockManager* manager, QWidget* parent = nullptr);
    ~DockParams() override;

    /// 文字组当前参数（文字工具盖章用，逐项来自面板输入框）。
    struct TextStampParams {
        QString text;       ///< 文字内容（已去首尾空白）
        QFont font;         ///< 字体（粗体由 bold 另控）
        int cellHeight = 12; ///< 字高（格，1 格 = 1 豆）
        int angleDeg = 0;   ///< 旋转角度（度，顺时针为正）
        bool bold = false;  ///< 粗体
    };
    /// 汇总文字组参数；text 为空表示尚未输入（主窗口据其提示）。
    TextStampParams textStampParams() const;

    /// 静默取消当前勾选的模式按钮（工具切换时调用，不发信号）。
    void setMirrorModeOff();
    /// 静默点亮指定模式按钮并取消另一模式（互斥，不发信号）。
    void setMirrorModeOn(const QString& objectName);

    /// 语言切换：重设面板内全部 UI 文字（组框标题 / 按钮 / 后缀 / 占位符）。
    void retranslateUi();

signals:
    /// 全图镜像：horizontal=true 左右镜像，false 上下镜像（按钮点击立即执行）。
    void mirrorWholeRequested(bool horizontal);
    /// 自定义镜像（两点轴）模式开关（仅用户点击触发）。
    void customMirrorToggled(bool on);
    /// 自定义轴（XY 轴）模式开关（仅用户点击触发）。
    void xyAxisToggled(bool on);
    /// 移动选区内容（dx/dy 为格子偏移，可负）。
    void moveRequested(int dx, int dy);
    /// 文字组任一参数变化（预览实时刷新由主窗口处理）。
    void textParamsChanged();
    /// 「放置」按钮：切到文字工具（点击画布格放置 / 移动预览）。
    void textPlaceRequested();
    /// 「确认生成」按钮：把当前文字预览写入豆子格子。
    void textConfirmRequested();

private:
    void applyCheckedState(const QString& objectName, bool checked);

    QList<QAbstractButton*> m_modeButtons;
    bool m_silent = false;

    // 文字组（文字转豆子）
    QTextEdit* m_textContent = nullptr;
    QFontComboBox* m_textFont = nullptr;
    QSpinBox* m_textHeight = nullptr;
    QSpinBox* m_textAngle = nullptr;
    QCheckBox* m_textBold = nullptr;

    // 组框与无 objectName 的标签（语言切换 retranslateUi 用）
    QGroupBox* m_mirrorBox = nullptr;
    QGroupBox* m_moveBox = nullptr;
    QGroupBox* m_textBox = nullptr;
    QLabel* m_moveXLabel = nullptr;
    QLabel* m_moveYLabel = nullptr;
};
