#pragma once

#include <QByteArray>
#include <QString>

class QMainWindow;

/// 工作区布局管理（开发大纲第 15 章）：
/// ADS 停靠状态 + 当前工具 + 主题，按命名工作区存取 QSettings。
class Workspace
{
public:
    static Workspace& instance();

    /// 保存当前布局为命名工作区。
    void save(const QString& name, QMainWindow* window, const QByteArray& dockState,
              const QString& currentTool);

    /// 恢复命名工作区；不存在返回 false。
    bool restore(const QString& name, QMainWindow* window, QString* currentTool);

    /// 应用 dock 状态。
    static void applyDockState(QMainWindow* window, const QByteArray& state);

    /// 当前 dock 状态。
    static QByteArray currentDockState(QMainWindow* window);

    /// 已保存的工作区名。
    QStringList names() const;

private:
    Workspace() = default;
    static QString settingsGroup();
};
