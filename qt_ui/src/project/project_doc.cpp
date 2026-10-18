#include "project_doc.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

ProjectDoc::ProjectDoc(QObject* parent)
    : QObject(parent)
    , m_history(rust_api::createHistory(100))
{
    m_autosaveTimer.setSingleShot(false);
    connect(&m_autosaveTimer, &QTimer::timeout, this, [this]() {
        if (!m_chart || !m_modified) {
            return;
        }
        const QString path = autosaveFilePath();
        if (rust_api::projectSave(m_chart.get(), path) == cm::codes::OK) {
            emit statusMessage(QCoreApplication::translate("ProjectDoc", "Autosaved"), 2000);
        }
    });
}

void ProjectDoc::newChart(const QSize& grid, const CmCrossStitchParams& params,
                          const CmStitchOptions& options)
{
    // 新建：透明底生成空白网格（全部单元格为空，用户随后绘制或导入图片生成）
    QImage image(grid.width() > 0 ? grid.width() : 100,
                 grid.height() > 0 ? grid.height() : 100,
                 QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    int err = cm::codes::OK;
    rust_api::ChartHandle chart = rust_api::generateChart(image, params, options, nullptr, &err);
    if (!chart) {
        emit statusMessage(
            QCoreApplication::translate("ProjectDoc", "Failed to create project (error %1)").arg(err));
        return;
    }
    m_path.clear();
    m_sourceName.clear();
    m_modified = false;
    adoptChart(std::move(chart), QString());
    rust_api::historyInit(m_history.get(), serializeState());
    emit modificationChanged(false);
    emit undoRedoChanged();
}

void ProjectDoc::adoptChart(rust_api::ChartHandle chart, const QString& sourceName)
{
    if (!chart) {
        return;
    }
    m_chart = std::move(chart);
    m_sourceName = sourceName;
    // 参数镜像：打开工程 / 生成完成后回填 UI 面板
    rust_api::chartParams(m_chart.get(), &m_params, &m_options);
    touch();
}

void ProjectDoc::clearChart()
{
    if (!m_chart) {
        return;
    }
    m_chart = nullptr;
    m_params = {};
    m_options = rust_api::defaultStitchOptions();
    touch();
    emit chartChanged();
}

void ProjectDoc::syncFromCore(const QString& historyLabel)
{
    if (!m_chart) {
        return;
    }
    rust_api::chartParams(m_chart.get(), &m_params, &m_options);
    pushHistory(historyLabel);
    emit chartChanged();
}

bool ProjectDoc::save()
{
    if (m_path.isEmpty()) {
        return false;
    }
    const int err = rust_api::projectSave(m_chart.get(), m_path);
    if (err != cm::codes::OK) {
        emit statusMessage(QCoreApplication::translate("ProjectDoc", "Save failed (error %1)").arg(err));
        return false;
    }
    m_modified = false;
    emit modificationChanged(false);
    emit statusMessage(QCoreApplication::translate("ProjectDoc", "Saved"));
    return true;
}

bool ProjectDoc::saveAs(const QString& path)
{
    m_path = path;
    return save();
}

bool ProjectDoc::open(const QString& path)
{
    int err = cm::codes::OK;
    rust_api::ChartHandle chart = rust_api::projectOpen(path, &err);
    if (!chart) {
        emit statusMessage(
            QCoreApplication::translate("ProjectDoc", "Open failed (error %1)").arg(err));
        return false;
    }
    m_path = path;
    m_modified = false;
    adoptChart(std::move(chart), QFileInfo(path).fileName());
    rust_api::historyInit(m_history.get(), serializeState());
    emit modificationChanged(false);
    emit undoRedoChanged();
    return true;
}

void ProjectDoc::pushHistory(const QString& label)
{
    if (!m_chart) {
        return;
    }
    rust_api::historyPush(m_history.get(), label, serializeState());
    m_modified = true;
    emit modificationChanged(true);
    emit undoRedoChanged();
}

void ProjectDoc::resetHistory()
{
    rust_api::historyClear(m_history.get());
    emit undoRedoChanged();
}

void ProjectDoc::undo()
{
    const QByteArray state = rust_api::historyUndo(m_history.get());
    if (state.isEmpty()) {
        return;
    }
    applyState(state);
    m_modified = true;
    emit modificationChanged(true);
    emit undoRedoChanged();
}

void ProjectDoc::redo()
{
    const QByteArray state = rust_api::historyRedo(m_history.get());
    if (state.isEmpty()) {
        return;
    }
    applyState(state);
    m_modified = true;
    emit modificationChanged(true);
    emit undoRedoChanged();
}

bool ProjectDoc::canUndo() const
{
    return rust_api::historyCanUndo(m_history.get());
}

bool ProjectDoc::canRedo() const
{
    return rust_api::historyCanRedo(m_history.get());
}

ChartSnapshot ProjectDoc::chartSnapshot() const
{
    ChartSnapshot snapshot;
    if (!m_chart) {
        return snapshot;
    }
    snapshot.valid = true;
    snapshot.gridSize = rust_api::chartGridSize(m_chart.get());
    snapshot.cells = rust_api::chartCells(m_chart.get());
    snapshot.stitchTotal = rust_api::chartStitchTotal(m_chart.get());
    snapshot.params = m_params;
    snapshot.options = m_options;
    snapshot.fabricSize = rust_api::chartFabricSize(m_chart.get());

    const QVector<CmLegendEntry> legend = rust_api::chartLegend(m_chart.get());
    snapshot.legend = legend;
    snapshot.palette.reserve(legend.size());
    snapshot.colorCodes.reserve(legend.size());
    snapshot.colorNames.reserve(legend.size());
    for (int i = 0; i < legend.size(); ++i) {
        snapshot.palette.push_back(rust_api::chartColorRgb(m_chart.get(), i));
        snapshot.colorCodes.push_back(rust_api::chartColorCode(m_chart.get(), i));
        snapshot.colorNames.push_back(rust_api::chartColorName(m_chart.get(), i));
    }
    // 符号取内核实际分配结果（按用量降序分配，非静态序列）
    snapshot.symbols = rust_api::chartSymbols(m_chart.get());
    // 符号集全集：供材料表「更改符号」列出候选。
    // symbol_in_set 越界会回绕，因此取到重复字符即认为已取完整集，停止。
    snapshot.symbolAlphabet.reserve(64);
    for (int i = 0; i < 64; ++i) {
        const QChar c = rust_api::symbolInSet(snapshot.options.symbol_set, i);
        if (c.isNull() || snapshot.symbolAlphabet.contains(c)) {
            break;
        }
        snapshot.symbolAlphabet.push_back(c);
    }
    return snapshot;
}

void ProjectDoc::startAutosave(int intervalMs)
{
    if (intervalMs > 0) {
        m_autosaveTimer.start(intervalMs);
    }
}

QString ProjectDoc::findAutosave()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/BeanDraw");
    const QString path = dir + QStringLiteral("/autosave.bean");
    return QFileInfo::exists(path) ? path : QString();
}

bool ProjectDoc::openAutosave(const QString& path)
{
    return open(path);
}

void ProjectDoc::clearAutosave()
{
    const QString path = findAutosave();
    if (!path.isEmpty()) {
        QFile::remove(path);
    }
}

QByteArray ProjectDoc::serializeState() const
{
    return rust_api::chartToBytes(m_chart.get());
}

void ProjectDoc::applyState(const QByteArray& state)
{
    rust_api::ChartHandle chart = rust_api::chartFromBytes(state);
    if (chart) {
        m_chart = std::move(chart);
        touch();
    }
}

void ProjectDoc::touch()
{
    emit chartChanged();
}

QString ProjectDoc::autosaveFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/BeanDraw");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/autosave.bean");
}
