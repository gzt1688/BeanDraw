#include "craft_app.h"

#include "ui/i18n/tr_manager.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QTimer>

CraftApp::CraftApp(QObject* parent)
    : QObject(parent)
{
}

CraftApp::~CraftApp() = default;

bool CraftApp::initialize()
{
    qInfo("[CM-boot] initialize enter");
    TrManager::instance().initialize();

    // 全局字体放大（默认 9pt 偏小，专业工具需要更清晰的信息密度）
    QFont appFont = QApplication::font();
    if (appFont.pointSize() < 11) {
        appFont.setPointSize(11);
        QApplication::setFont(appFont);
    }

    if (!loadStyleSheet()) {
        // 样式缺失不阻塞启动，仅警告
        qWarning("CraftApp: style sheet not applied");
    }

    qInfo("[CM-boot] building MainWindow");
    m_mainWindow.reset(new MainWindow());
    qInfo("[CM-boot] show()");
    m_mainWindow->show();
    qInfo("[CM-boot] show done");
    return true;
}

int CraftApp::run()
{
    // 启动期面包屑：定位间歇性堆损坏崩溃的阶段（定时真间隔，非排队即执行）
    for (int ms : { 300, 1000, 2000, 3000, 4000, 6000 }) {
        QTimer::singleShot(ms, nullptr, [ms] {
            qInfo("[CM-boot] event loop t+%dms alive", ms);
        });
    }
    qInfo("[CM-boot] exec enter");
    return qApp->exec();
}

bool CraftApp::loadStyleSheet()
{
    // 跟随系统深浅色偏好：Windows 下默认浅色，后续接入系统主题检测
    const QString path = QStringLiteral(":/qss/beandraw.qss");
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }
    QString qss = QString::fromUtf8(file.readAll());
    // 高亮色占位符：启动期尚无主题配置，用默认浅色高亮（与 settings 默认一致；
    // MainWindow::applyTheme 随后会用实际主题色覆盖整套样式表）
    qss.replace(QStringLiteral("%ACCENT%"), QStringLiteral("#A11740"));
    qApp->setStyleSheet(qss);
    m_styleName = path;
    return true;
}
