#pragma once

// RustApi：Rust FFI 的 C++ 封装层（开发大纲第 8/12 章）。
// - RAII 句柄管理，杜绝泄漏 / 双重释放
// - 进度回调经 Qt 事件队列投递到 UI 线程
// - 错误码 → i18n key 映射（文案在 UI 层翻译）

#include "ffi_types.h"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QVector>
#include <cstddef>
#include <functional>
#include <memory>

namespace rust_api {

/// 图表句柄（独占所有权）。
using ChartHandle = std::unique_ptr<void, void (*)(void*)>;

/// 纸样路径句柄。
using PathsHandle = std::unique_ptr<void, void (*)(void*)>;

/// 历史栈句柄。
using HistoryHandle = std::unique_ptr<void, void (*)(void*)>;

/// 模块信息（UI 层用）。
struct ModuleInfo {
    QString id;
    QString nameKey;
    QString version;
};

/// 版本号。
QString coreVersion();

/// 内置模块列表。
QVector<ModuleInfo> modules();

/// 十字绣默认参数（与 Rust 侧 Default 保持一致：100×100 网格 / 16 色 / 14ct）。
CmCrossStitchParams defaultCrossStitchParams();

/// 十字绣默认高级选项（字母符号 / 众数归并 / 弱去杂点 / 3‰ 稀有色合并）。
CmStitchOptions defaultStitchOptions();

/// 生成十字绣图纸。
/// onProgress(stage, progress) 在 UI 线程调用；返回空句柄表示失败（errOut 输出错误码）。
ChartHandle generateChart(const QImage& image,
                          const CmCrossStitchParams& params,
                          const CmStitchOptions& options,
                          const std::function<void(int stage, float progress)>& onProgress,
                          int* errOut = nullptr);

/// 网格数据（拷贝，安全跨线程）。
QVector<CmGridCell> chartCells(const void* chart);

/// 图例 / 统计（拷贝）。
QVector<CmLegendEntry> chartLegend(void* chart);

/// 实际分配到的符号（与图例一一对应；区别于 symbolChar 的静态序列）。
QVector<QChar> chartSymbols(void* chart);

/// 色号 / 色名（index 越界返回空串）。
QString chartColorCode(const void* chart, int index);
QString chartColorName(const void* chart, int index);

/// 调色板颜色（index 越界返回无效 QColor）。
QColor chartColorRgb(const void* chart, int index);

/// 符号字符（与 Rust util::SYMBOL_SEQUENCE 一致）。
QChar symbolChar(int index);

/// 符号集内第 index 个符号（越界回绕）。
QChar symbolInSet(int symbolSet, int index);

/// 总针数 / 网格尺寸。
quint32 chartStitchTotal(const void* chart);
QSize chartGridSize(const void* chart);
/// 生成参数与高级选项镜像（打开工程后回填 UI）。
void chartParams(const void* chart, CmCrossStitchParams* outParams, CmStitchOptions* outOptions);

/// 成品尺寸（毫米）。
CmFabricSize chartFabricSize(const void* chart);

/// 由目标成品尺寸反推网格数。
QSize gridForSize(quint32 widthMm, quint32 heightMm, quint32 fabricCount);

/// 编辑单元格符号（返回错误码，0 = 成功）。
int chartSetSymbol(void* chart, quint32 x, quint32 y, quint32 symbolIndex);

// ---- 图纸编辑（画布工具 → 命令栈 → 内核）----
/// 单格涂色 / 擦除（colorIndex = cm::kColorNone 擦除）。返回改动格数，-1 = 失败。
int chartPaintCell(void* chart, quint32 x, quint32 y, quint32 colorIndex);
/// 区域填充（油漆桶）。返回改动格数，-1 = 失败。
int chartFloodFill(void* chart, quint32 x, quint32 y, quint32 colorIndex);
/// 矩形填充。返回改动格数，-1 = 失败。
int chartRectFill(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1, quint32 colorIndex);
/// 批量填色（圆形 / 椭圆 / 多边形内部格子）。返回改动格数，-1 = 失败。
int chartFillCells(void* chart, const QVector<quint32>& xs, const QVector<quint32>& ys,
                   quint32 colorIndex);
/// 区域镜像（horizontal=true 左右，false 上下）。返回发生变化格数，-1 = 失败。
int chartMirrorRegion(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                      bool horizontal);
/// 任意直线轴镜像：轴由 (ax0,ay0)-(ax1,ay1) 两点定义（格子单位，可半格，如
/// 竖格线 x=k 用 (k,0)-(k,1)）。返回实际写入格数，-1 = 失败。
int chartMirrorRegionAxis(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                          double ax0, double ay0, double ax1, double ay1);
/// 区域缩放（最近邻重采样写回 dst 起点选区）。返回实际改变格数，-1 = 失败。
int chartScaleRegion(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                     quint32 dstX0, quint32 dstY0, quint32 newW, quint32 newH);
/// 移动选区内容（剪切平移，不缩放）。dx/dy 为格子偏移，可正可负。
/// 返回实际改变格数（含清除与写入），-1 = 失败。
int chartMoveRegion(void* chart, quint32 x0, quint32 y0, quint32 x1, quint32 y1,
                    int dx, int dy);
/// 4-连通同色区域识别：cells 输出与 (x,y) 同色相连区域的格子索引
/// （索引 = y*gridW + x，顺序不定）。返回区域格数（空格子区域为 0），-1 = 失败。
int chartFloodRegion(void* chart, quint32 x, quint32 y, QVector<quint32>& cells);
/// 全局换色。返回改动格数，-1 = 失败。
int chartReplaceColor(void* chart, quint32 from, quint32 to);
/// 合并颜色：from 的针全部改用 to，并把 from 从调色板移除（色块随之消失）。返回改动格数。
int chartMergeColor(void* chart, quint32 from, quint32 to);
/// 删除颜色：该色所有针清空并从调色板移除。返回被清空的格数，-1 = 失败。
int chartDeleteColor(void* chart, quint32 colorIndex);
/// 更改某色符号（newSymbol 为 Unicode 码点）。目标符号被占用时两色交换。
/// 返回被交换的另一色索引，-1 = 无冲突，-2 = 失败。
int chartSetColorSymbol(void* chart, quint32 colorIndex, char32_t newSymbol);
/// 去杂点（strength: 0/1/2）。返回改动格数。
int chartCleanup(void* chart, quint32 strength);
/// 稀有色合并（ppt = 千分比）。返回被合并的颜色种数。
int chartMergeRare(void* chart, quint32 ppt);
/// 清理未使用颜色（删除用量为 0 的调色板项）。返回被删除的颜色种数。
int chartPruneUnused(void* chart);
/// 切换符号集。
int chartSetSymbolSet(void* chart, quint32 symbolSet);
/// 按当前用量重新分配符号。
int chartRedistributeSymbols(void* chart);
/// 把指定 DMC 色号加入图纸调色板（已存在则返回已有索引）。返回索引，失败返回负数。
int chartAddColor(void* chart, const QString& code);

/// 序列化 / 反序列化（工程状态快照，供命令栈使用）。
QByteArray chartToBytes(const void* chart);
ChartHandle chartFromBytes(const QByteArray& bytes);

/// 纸样：带缝份矩形。
PathsHandle seamRect(double widthMm, double heightMm, double seamMm, int* errOut = nullptr);
QVector<CmPoint> pathsOutline(const void* paths);
QVector<CmPoint> pathsSeam(const void* paths);

/// 历史栈。
HistoryHandle createHistory(int maxDepth = 100);
void historyInit(void* history, const QByteArray& state);
void historyPush(void* history, const QString& label, const QByteArray& state);
QByteArray historyUndo(void* history);   // 空 = 无可撤销
QByteArray historyRedo(void* history);   // 空 = 无可重做
bool historyCanUndo(void* history);
bool historyCanRedo(void* history);
void historyClear(void* history);

/// 导出。返回错误码（0 = 成功）。
int exportCsv(const void* chart, const QString& path);
int exportPng(const void* chart, const QString& path, int cellPx);
int exportPdf(const void* chart, const QString& path, const QString& title);
int exportDxfPiece(const void* paths, const QString& path);

/// 工程文件。
int projectSave(const void* chart, const QString& path);
ChartHandle projectOpen(const QString& path, int* errOut = nullptr);

/// 素材库。
void* materialDbNew();
void materialDbFree(void* db);
/// 查询 DMC 色（返回 false = 未找到）。
bool materialFindDmc(void* db, const QString& code, CmDmcColor* out);
int materialCount(void* db, const QString& kind);
/// 枚举某大类全部素材到 out 缓冲区（容量 outCap）。返回总数；out 为 nullptr 时仅探测容量。
int materialAll(void* db, const QString& kind, CmDmcColor* out, std::size_t outCap);

/// 错误码 → i18n key（文案由 tr() 翻译，Rust 不持有本地化字符串）。
QString errorMessageKey(int code);

} // namespace rust_api
