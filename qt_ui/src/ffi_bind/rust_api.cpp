#include "rust_api.h"

#include <QCoreApplication>
#include <QColor>
#include <QFileInfo>
#include <QMutex>
#include <QObject>

#include <cstring>

namespace rust_api {

namespace {

// 图表句柄删除器
void freeChart(void* p) noexcept
{
    if (p) {
        cm_chart_free(p);
    }
}

void freePaths(void* p) noexcept
{
    if (p) {
        cm_paths_free(p);
    }
}

void freeHistory(void* p) noexcept
{
    if (p) {
        cm_history_free(p);
    }
}

/// 进度回调上下文：Rust 工作线程 → Qt UI 线程投递。
struct ProgressContext {
    std::function<void(int, float)> fn;
};

extern "C" void progressTrampoline(void* userData, float progress, int stage)
{
    auto* ctx = static_cast<ProgressContext*>(userData);
    if (!ctx || !ctx->fn) {
        return;
    }
    // 捕获到 shared_ptr 保证异步执行期间上下文存活
    auto fn = std::make_shared<std::function<void(int, float)>>(ctx->fn);
    QObject* receiver = QCoreApplication::instance();
    if (!receiver) {
        // 无事件循环（如单元测试）：直接调用，调用方需自行保证线程安全
        (*fn)(stage, progress);
        return;
    }
    QMetaObject::invokeMethod(
        receiver,
        [fn, stage, progress]() { (*fn)(stage, progress); },
        Qt::QueuedConnection);
}

/// RGBA8 连续缓冲（QImage → FFI 数组）。
QImage toRgba8(const QImage& image)
{
    if (image.isNull()) {
        return QImage();
    }
    return image.convertToFormat(QImage::Format_RGBA8888);
}

QString fromCmString(const char* s)
{
    return s ? QString::fromUtf8(s) : QString();
}

QByteArray localFilePath(const QString& path)
{
    // Windows 上 FFI 路径需为本地 8 位编码；Qt6 默认 UTF-16，此处按系统编码转换
    return QFileInfo(path).absoluteFilePath().toLocal8Bit();
}

} // namespace

QString coreVersion()
{
    return fromCmString(cm_version());
}

CmCrossStitchParams defaultCrossStitchParams()
{
    CmCrossStitchParams p;
    p.grid_width = 100;
    p.grid_height = 100;
    p.max_colors = 16;
    p.bead_mode = 0;
    p.fabric_count = 14;
    p.dither = 0;
    return p;
}

QVector<ModuleInfo> modules()
{
    QVector<ModuleInfo> result;
    size_t count = 0;
    const CmModuleInfo* infos = cm_modules_list(&count);
    if (!infos) {
        return result;
    }
    result.reserve(static_cast<int>(count));
    for (size_t i = 0; i < count; ++i) {
        result.push_back({fromCmString(infos[i].id),
                          fromCmString(infos[i].name_key),
                          fromCmString(infos[i].version)});
    }
    return result;
}

CmStitchOptions defaultStitchOptions()
{
    CmStitchOptions o;
    o.symbol_set = cm::symbol::LETTERS;
    o.sample_mode = cm::sample::MODE;
    o.cleanup = cm::cleanup::LIGHT;
    o.rare_color_ppt = 3;
    o.craft_mode = 0;
    return o;
}

ChartHandle generateChart(const QImage& image,
                          const CmCrossStitchParams& params,
                          const CmStitchOptions& options,
                          const std::function<void(int stage, float progress)>& onProgress,
                          int* errOut)
{
    const QImage rgba = toRgba8(image);
    if (rgba.isNull()) {
        if (errOut) {
            *errOut = cm::codes::INVALID_ARG;
        }
        return ChartHandle(nullptr, freeChart);
    }

    CmProgressFn cb = nullptr;
    void* userData = nullptr;
    auto ctx = std::make_shared<ProgressContext>();
    if (onProgress) {
        ctx->fn = onProgress;
        cb = &progressTrampoline;
        userData = ctx.get();
    }

    int32_t err = cm::codes::OK;
    void* chart = cm_cross_stitch_generate(rgba.constBits(),
                                           static_cast<size_t>(rgba.sizeInBytes()),
                                           static_cast<uint32_t>(rgba.width()),
                                           static_cast<uint32_t>(rgba.height()),
                                           &params,
                                           &options,
                                           cb,
                                           userData,
                                           &err);
    if (errOut) {
        *errOut = err;
    }
    return ChartHandle(chart, freeChart);
}

QVector<CmGridCell> chartCells(const void* chart)
{
    QVector<CmGridCell> result;
    if (!chart) {
        return result;
    }
    const CmGridCell* ptr = nullptr;
    size_t len = 0;
    if (cm_chart_cells(chart, &ptr, &len) != cm::codes::OK || !ptr || len == 0) {
        return result;
    }
    result.resize(static_cast<int>(len));
    std::memcpy(result.data(), ptr, len * sizeof(CmGridCell));
    return result;
}

QVector<CmLegendEntry> chartLegend(void* chart)
{
    QVector<CmLegendEntry> result;
    if (!chart) {
        return result;
    }
    const CmLegendEntry* ptr = nullptr;
    size_t len = 0;
    if (cm_chart_legend(chart, &ptr, &len) != cm::codes::OK || !ptr || len == 0) {
        return result;
    }
    result.resize(static_cast<int>(len));
    std::memcpy(result.data(), ptr, len * sizeof(CmLegendEntry));
    return result;
}

QVector<QChar> chartSymbols(void* chart)
{
    QVector<QChar> result;
    if (!chart) {
        return result;
    }
    const uint32_t* ptr = nullptr;
    size_t len = 0;
    if (cm_chart_symbols(chart, &ptr, &len) != cm::codes::OK || !ptr || len == 0) {
        return result;
    }
    result.reserve(static_cast<int>(len));
    for (size_t i = 0; i < len; ++i) {
        result.push_back(QChar(static_cast<uint>(ptr[i])));
    }
    return result;
}

QString chartColorCode(const void* chart, int index)
{
    if (!chart || index < 0) {
        return QString();
    }
    return fromCmString(cm_chart_color_code(chart, static_cast<size_t>(index)));
}

QString chartColorName(const void* chart, int index)
{
    if (!chart || index < 0) {
        return QString();
    }
    return fromCmString(cm_chart_color_name(chart, static_cast<size_t>(index)));
}

QColor chartColorRgb(const void* chart, int index)
{
    if (!chart || index < 0) {
        return QColor();
    }
    uint8_t rgb[3] = {0, 0, 0};
    if (cm_chart_color_rgb(chart, static_cast<size_t>(index), rgb) != cm::codes::OK) {
        return QColor();
    }
    return QColor(rgb[0], rgb[1], rgb[2]);
}

QChar symbolChar(int index)
{
    if (index < 0) {
        return QChar('?');
    }
    return QChar(static_cast<uint>(cm_symbol_at(static_cast<uint32_t>(index))));
}

QChar symbolInSet(int symbolSet, int index)
{
    if (index < 0 || symbolSet < 0) {
        return QChar('?');
    }
    return QChar(static_cast<uint>(
        cm_symbol_in_set(static_cast<uint32_t>(symbolSet), static_cast<uint32_t>(index))));
}

quint32 chartStitchTotal(const void* chart)
{
    return chart ? cm_chart_stitch_total(chart) : 0;
}

void chartParams(const void* chart, CmCrossStitchParams* outParams, CmStitchOptions* outOptions)
{
    if (!chart) {
        return;
    }
    cm_chart_params(chart, outParams, outOptions);
}

CmFabricSize chartFabricSize(const void* chart)
{
    CmFabricSize size{};
    if (!chart) {
        return size;
    }
    cm_chart_fabric_size(chart, &size);
    return size;
}

QSize gridForSize(quint32 widthMm, quint32 heightMm, quint32 fabricCount)
{
    uint32_t w = 0;
    uint32_t h = 0;
    if (cm_grid_for_size(widthMm, heightMm, fabricCount, &w, &h) != cm::codes::OK) {
        return QSize();
    }
    return QSize(static_cast<int>(w), static_cast<int>(h));
}

int chartPaintCell(void* chart, quint32 x, quint32 y, quint32 colorIndex)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_paint_cell(chart, x, y, colorIndex, &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartFloodFill(void* chart, quint32 x, quint32 y, quint32 colorIndex)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_flood_fill(chart, x, y, colorIndex, &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartRectFill(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1, quint32 colorIndex)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_rect_fill(chart, x0, y0, x1, y1, colorIndex, &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartFillCells(void* chart, const QVector<quint32>& xs, const QVector<quint32>& ys,
                   quint32 colorIndex)
{
    if (!chart || xs.size() != ys.size() || xs.isEmpty()) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_fill_cells(chart, xs.constData(), ys.constData(), xs.size(),
                            colorIndex, &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartMirrorRegion(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                      bool horizontal)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_mirror_region(chart, x0, y0, x1, y1, horizontal ? 1 : 0,
                               &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartMirrorRegionAxis(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                          double ax0, double ay0, double ax1, double ay1)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_mirror_region_axis(chart, x0, y0, x1, y1, ax0, ay0, ax1, ay1,
                                    &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartScaleRegion(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                     quint32 dstX0, quint32 dstY0, quint32 newW, quint32 newH)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_scale_region(chart, x0, y0, x1, y1, dstX0, dstY0, newW, newH,
                              &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartMoveRegion(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                    int dx, int dy)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_move_region(chart, x0, y0, x1, y1, dx, dy, &count, &err)
        != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartFloodRegion(void* chart, quint32 x, quint32 y, QVector<quint32>& cells)
{
    cells.clear();
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    // 先取区域格数，再按容量预分配写入索引（避免上界未知的栈/堆分配）
    if (cm_chart_flood_region(chart, x, y, nullptr, &count, &err) != cm::codes::OK) {
        return -1;
    }
    cells.resize(count);
    if (count > 0) {
        if (cm_chart_flood_region(chart, x, y, cells.data(), &count, &err)
            != cm::codes::OK) {
            cells.clear();
            return -1;
        }
    }
    return static_cast<int>(count);
}

int chartReplaceColor(void* chart, quint32 from, quint32 to)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_replace_color(chart, from, to, &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartMergeColor(void* chart, quint32 from, quint32 to)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_merge_color(chart, from, to, &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartDeleteColor(void* chart, quint32 colorIndex)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_delete_color(chart, colorIndex, &count, &err) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartSetColorSymbol(void* chart, quint32 colorIndex, char32_t newSymbol)
{
    if (!chart) {
        return -2;
    }
    int32_t swapped = -1;
    int32_t err = cm::codes::OK;
    if (cm_chart_set_color_symbol(chart, colorIndex, static_cast<uint32_t>(newSymbol), &swapped,
                                  &err)
        != cm::codes::OK) {
        return -2;
    }
    return swapped;
}

int chartCleanup(void* chart, quint32 strength)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    cm_chart_cleanup(chart, strength, &count);
    return static_cast<int>(count);
}

int chartMergeRare(void* chart, quint32 ppt)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    cm_chart_merge_rare(chart, ppt, &count);
    return static_cast<int>(count);
}

int chartPruneUnused(void* chart)
{
    if (!chart) {
        return -1;
    }
    uint32_t count = 0;
    if (cm_chart_prune_unused(chart, &count) != cm::codes::OK) {
        return -1;
    }
    return static_cast<int>(count);
}

int chartSetSymbolSet(void* chart, quint32 symbolSet)
{
    return chart ? cm_chart_set_symbol_set(chart, symbolSet) : -1;
}

int chartRedistributeSymbols(void* chart)
{
    return chart ? cm_chart_redistribute_symbols(chart) : -1;
}

int chartAddColor(void* chart, const QString& code)
{
    if (!chart) {
        return -1;
    }
    const QByteArray local = code.toUtf8();
    return cm_chart_add_color(chart, local.constData());
}

QSize chartGridSize(const void* chart)
{
    if (!chart) {
        return QSize();
    }
    uint32_t w = 0;
    uint32_t h = 0;
    cm_chart_dimensions(chart, &w, &h);
    return QSize(static_cast<int>(w), static_cast<int>(h));
}

int chartSetSymbol(void* chart, quint32 x, quint32 y, quint32 symbolIndex)
{
    if (!chart) {
        return cm::codes::NULL_PTR;
    }
    int32_t err = cm::codes::OK;
    cm_chart_set_symbol(chart, x, y, symbolIndex, &err);
    return err;
}

QByteArray chartToBytes(const void* chart)
{
    if (!chart) {
        return QByteArray();
    }
    uint8_t* ptr = nullptr;
    size_t len = 0;
    int32_t err = cm::codes::OK;
    if (cm_chart_to_bytes(chart, &ptr, &len, &err) != cm::codes::OK || !ptr || len == 0) {
        return QByteArray();
    }
    QByteArray bytes(reinterpret_cast<const char*>(ptr), static_cast<int>(len));
    cm_bytes_free(ptr, len);
    return bytes;
}

ChartHandle chartFromBytes(const QByteArray& bytes)
{
    if (bytes.isEmpty()) {
        return ChartHandle(nullptr, freeChart);
    }
    int32_t err = cm::codes::OK;
    void* chart = cm_chart_from_bytes(reinterpret_cast<const uint8_t*>(bytes.constData()),
                                      static_cast<size_t>(bytes.size()),
                                      &err);
    return ChartHandle(chart, freeChart);
}

PathsHandle seamRect(double widthMm, double heightMm, double seamMm, int* errOut)
{
    int32_t err = cm::codes::OK;
    void* paths = cm_pattern_seam_rect(widthMm, heightMm, seamMm, &err);
    if (errOut) {
        *errOut = err;
    }
    return PathsHandle(paths, freePaths);
}

QVector<CmPoint> pathsOutline(const void* paths)
{
    QVector<CmPoint> result;
    if (!paths) {
        return result;
    }
    const CmPoint* ptr = nullptr;
    size_t len = 0;
    if (cm_paths_outline(paths, &ptr, &len) != cm::codes::OK || !ptr || len == 0) {
        return result;
    }
    result.resize(static_cast<int>(len));
    std::memcpy(result.data(), ptr, len * sizeof(CmPoint));
    return result;
}

QVector<CmPoint> pathsSeam(const void* paths)
{
    QVector<CmPoint> result;
    if (!paths) {
        return result;
    }
    const CmPoint* ptr = nullptr;
    size_t len = 0;
    if (cm_paths_seam(paths, &ptr, &len) != cm::codes::OK || !ptr || len == 0) {
        return result;
    }
    result.resize(static_cast<int>(len));
    std::memcpy(result.data(), ptr, len * sizeof(CmPoint));
    return result;
}

HistoryHandle createHistory(int maxDepth)
{
    return HistoryHandle(cm_history_new(static_cast<size_t>(maxDepth > 2 ? maxDepth : 2)),
                         freeHistory);
}

void historyInit(void* history, const QByteArray& state)
{
    if (history) {
        cm_history_init(history,
                        reinterpret_cast<const uint8_t*>(state.constData()),
                        static_cast<size_t>(state.size()));
    }
}

void historyPush(void* history, const QString& label, const QByteArray& state)
{
    if (history) {
        const QByteArray labelLocal = label.toUtf8();
        cm_history_push(history,
                        labelLocal.constData(),
                        reinterpret_cast<const uint8_t*>(state.constData()),
                        static_cast<size_t>(state.size()));
    }
}

QByteArray historyUndo(void* history)
{
    if (!history) {
        return QByteArray();
    }
    const uint8_t* ptr = nullptr;
    size_t len = 0;
    if (cm_history_undo(history, &ptr, &len) != 1 || !ptr || len == 0) {
        return QByteArray();
    }
    return QByteArray(reinterpret_cast<const char*>(ptr), static_cast<int>(len));
}

QByteArray historyRedo(void* history)
{
    if (!history) {
        return QByteArray();
    }
    const uint8_t* ptr = nullptr;
    size_t len = 0;
    if (cm_history_redo(history, &ptr, &len) != 1 || !ptr || len == 0) {
        return QByteArray();
    }
    return QByteArray(reinterpret_cast<const char*>(ptr), static_cast<int>(len));
}

bool historyCanUndo(void* history)
{
    return history && cm_history_can_undo(history) == 1;
}

bool historyCanRedo(void* history)
{
    return history && cm_history_can_redo(history) == 1;
}

void historyClear(void* history)
{
    if (history) {
        cm_history_clear(history);
    }
}

int exportCsv(const void* chart, const QString& path)
{
    if (!chart) {
        return cm::codes::NULL_PTR;
    }
    const QByteArray local = localFilePath(path);
    int32_t err = cm::codes::OK;
    cm_export_csv(chart, local.constData(), &err);
    return err;
}

int exportPng(const void* chart, const QString& path, int cellPx)
{
    if (!chart) {
        return cm::codes::NULL_PTR;
    }
    const QByteArray local = localFilePath(path);
    int32_t err = cm::codes::OK;
    cm_export_png(chart, local.constData(), static_cast<uint32_t>(cellPx), &err);
    return err;
}

int exportPdf(const void* chart, const QString& path, const QString& title)
{
    if (!chart) {
        return cm::codes::NULL_PTR;
    }
    const QByteArray local = localFilePath(path);
    const QByteArray titleUtf8 = title.toUtf8();
    int32_t err = cm::codes::OK;
    cm_export_pdf(chart, local.constData(), titleUtf8.constData(), &err);
    return err;
}

int exportDxfPiece(const void* paths, const QString& path)
{
    if (!paths) {
        return cm::codes::NULL_PTR;
    }
    const QByteArray local = localFilePath(path);
    int32_t err = cm::codes::OK;
    cm_export_dxf_piece(paths, local.constData(), &err);
    return err;
}

int projectSave(const void* chart, const QString& path)
{
    if (!chart) {
        return cm::codes::NULL_PTR;
    }
    const QByteArray local = localFilePath(path);
    int32_t err = cm::codes::OK;
    cm_project_save(chart, local.constData(), &err);
    return err;
}

ChartHandle projectOpen(const QString& path, int* errOut)
{
    const QByteArray local = localFilePath(path);
    int32_t err = cm::codes::OK;
    void* chart = cm_project_open(local.constData(), &err);
    if (errOut) {
        *errOut = err;
    }
    return ChartHandle(chart, freeChart);
}

void* materialDbNew()
{
    return cm_material_db_new();
}

void materialDbFree(void* db)
{
    if (db) {
        cm_material_db_free(db);
    }
}

bool materialFindDmc(void* db, const QString& code, CmDmcColor* out)
{
    if (!db || !out) {
        return false;
    }
    const QByteArray local = code.toUtf8();
    return cm_material_find_dmc(db, local.constData(), out) == 1;
}

int materialCount(void* db, const QString& kind)
{
    if (!db) {
        return -1;
    }
    const QByteArray local = kind.toUtf8();
    return cm_material_count(db, local.constData());
}

int materialAll(void* db, const QString& kind, CmDmcColor* out, std::size_t outCap)
{
    if (!db) {
        return -1;
    }
    const QByteArray local = kind.toUtf8();
    return cm_material_all(db, local.constData(), out, outCap);
}

QString errorMessageKey(int code)
{
    switch (code) {
    case cm::codes::OK:
        return QStringLiteral("error.ok");
    case cm::codes::INVALID_ARG:
        return QStringLiteral("error.invalid_arg");
    case cm::codes::NULL_PTR:
        return QStringLiteral("error.null_ptr");
    case cm::codes::UTF8:
        return QStringLiteral("error.utf8");
    case cm::codes::INTERNAL:
        return QStringLiteral("error.internal");
    case cm::codes::IMAGE_DECODE:
        return QStringLiteral("error.image_decode");
    case cm::codes::IMAGE_TOO_LARGE:
        return QStringLiteral("error.image_too_large");
    case cm::codes::QUANTIZE_FAILED:
        return QStringLiteral("error.quantize_failed");
    case cm::codes::GEOMETRY_INVALID:
        return QStringLiteral("error.geometry_invalid");
    case cm::codes::DB_ERROR:
        return QStringLiteral("error.db");
    case cm::codes::PDF_FAILED:
        return QStringLiteral("error.pdf_failed");
    case cm::codes::PROJECT_CORRUPT:
        return QStringLiteral("error.project_corrupt");
    case cm::codes::PROJECT_VERSION:
        return QStringLiteral("error.project_version");
    case cm::codes::PROJECT_UNSUPPORTED_MODULE:
        return QStringLiteral("error.project_unsupported_module");
    default:
        return QStringLiteral("error.unknown");
    }
}

} // namespace rust_api
