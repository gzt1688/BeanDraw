#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QString>

class QApplication;
class MainWindow;

/// 应用全局入口：i18n 初始化、样式、主窗口创建与事件循环。
class CraftApp : public QObject
{
    Q_OBJECT

public:
    explicit CraftApp(QObject* parent = nullptr);
    ~CraftApp() override;

    /// 初始化翻译、样式等。失败返回 false（已弹错误框）。
    bool initialize();

    /// 进入事件循环，返回退出码。
    int run();

private:
    bool loadStyleSheet();

    QScopedPointer<MainWindow> m_mainWindow;
    QString m_styleName;
};
