#include "craft_app.h"

#include "ffi_bind/rust_api.h"
#include "project/project_doc.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QEventLoop>
#include <QFile>
#include <QFutureWatcher>
#include <QImage>
#include <QMutex>
#include <QTextStream>
#include <QDateTime>
#include <QtConcurrent>

/// 日志文件实时写入（qDebug/qWarning 均捕获，Release 下 qDebug 不存在）
static void cmMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    Q_UNUSED(type)
    Q_UNUSED(ctx)
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    static QFile file(QStringLiteral("F:/softai/BeanDraw/output/cm_debug.log"));
    if (!file.isOpen()) {
        file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    if (file.isOpen()) {
        QTextStream ts(&file);
        ts << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << ' ' << msg << '\n';
        ts.flush();
    }
}

/// 无头冒烟测试（CI / RUN.bat 用）：
/// 与 UI 生成路径完全一致（QtConcurrent + FFI），跑通
/// 生成 → 统计 → 工程保存 → 工程打开 → PDF/CSV 导出 全链路。
static int runSmokeTest()
{
    using namespace rust_api;

    // 1. 构造测试图（红蓝棋盘）
    QImage image(32, 32, QImage::Format_RGBA8888);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            image.setPixelColor(x, y, ((x / 8 + y / 8) % 2 == 0) ? QColor(200, 30, 30)
                                                                 : QColor(30, 30, 200));
        }
    }

    // 2. 异步生成（与 DockProperty::onGenerate 同一代码路径）
    CmCrossStitchParams params = defaultCrossStitchParams();
    params.grid_width = 16;
    params.grid_height = 16;
    params.max_colors = 8;

    auto future = QtConcurrent::run([image, params]() -> void* {
        ChartHandle chart = generateChart(image, params, defaultStitchOptions(), nullptr);
        return chart.release();
    });
    QFutureWatcher<void*> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<void*>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(future);
    loop.exec();

    ChartHandle chart(watcher.result(), [](void* p) {
        if (p) {
            cm_chart_free(p);
        }
    });
    if (!chart) {
        qCritical("smoke: chart generation failed");
        return 1;
    }

    // 3. 统计校验
    const quint32 stitches = chartStitchTotal(chart.get());
    if (stitches != 256) {
        qCritical("smoke: unexpected stitch total %u (expected 256)", stitches);
        return 1;
    }
    const QVector<CmGridCell> cells = chartCells(chart.get());
    if (cells.size() != 256) {
        qCritical("smoke: unexpected cell count %d", cells.size());
        return 1;
    }
    qInfo("smoke: generated %ux%u grid, %u stitches, core %s",
          chartGridSize(chart.get()).width(),
          chartGridSize(chart.get()).height(),
          stitches,
          qPrintable(coreVersion()));

    // 4. 工程保存 / 打开
    const QString dir = QDir::tempPath();
    const QString beanPath = dir + QStringLiteral("/beandraw_smoke.bean");
    if (projectSave(chart.get(), beanPath) != cm::codes::OK) {
        qCritical("smoke: project save failed");
        return 1;
    }
    ChartHandle reopened = projectOpen(beanPath);
    if (!reopened || chartStitchTotal(reopened.get()) != stitches) {
        qCritical("smoke: project open failed");
        return 1;
    }

    // 5. 导出矩阵
    if (exportPdf(chart.get(), dir + QStringLiteral("/beandraw_smoke.pdf"),
                  QStringLiteral("Smoke")) != cm::codes::OK) {
        qCritical("smoke: pdf export failed");
        return 1;
    }
    if (exportCsv(chart.get(), dir + QStringLiteral("/beandraw_smoke.csv")) != cm::codes::OK) {
        qCritical("smoke: csv export failed");
        return 1;
    }

    qInfo("smoke: PASS (project + pdf + csv roundtrip)");
    return 0;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/favicon.ico")));
    QApplication::setApplicationName("BeanDraw");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("BeanDraw");
    QApplication::setOrganizationDomain("beandraw.local");

    if (app.arguments().contains(QStringLiteral("--smoke"))) {
        return runSmokeTest();
    }

    qInstallMessageHandler(cmMessageHandler);
    CraftApp craftApp;
    if (!craftApp.initialize()) {
        return 1;
    }
    return craftApp.run();
}
