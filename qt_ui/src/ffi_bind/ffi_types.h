// BeanDraw FFI C ABI 声明 —— 与 rust_core/src/common/ffi_types.rs 一一对应。
// 修改必须双边同步（开发大纲第 12 章）。

#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {

// ------------------------------------------------------------
// 基础类型
// ------------------------------------------------------------
typedef void (*CmProgressFn)(void* user_data, float progress, int stage);

typedef struct {
    double x;
    double y;
} CmPoint;

typedef struct {
    uint32_t grid_width;
    uint32_t grid_height;
    uint32_t max_colors;
    uint32_t bead_mode;      // 0 关 / 1 开
    uint32_t fabric_count;   // ct
    uint32_t dither;         // 0 关 / 1 开
} CmCrossStitchParams;

/// 十字绣高级生成选项（独立于 params，保证旧工程 schema 可迁移）。
typedef struct {
    uint32_t symbol_set;      // 0 字母 / 1 数字 / 2 几何 / 3 混合
    uint32_t sample_mode;     // 0 盒式平均 / 1 众数 / 2 中心色(medoid)
    uint32_t cleanup;         // 0 关 / 1 弱 / 2 强
    uint32_t rare_color_ppt;  // 稀有色合并阈值（千分数，0 = 不合并）
    uint32_t craft_mode;      // 0 十字绣 / 1 珠绣 / 2 拼豆（原对齐位）
} CmStitchOptions;

/// 成品尺寸（网格 × 面料计数换算）。
typedef struct {
    uint32_t width_mm;
    uint32_t height_mm;
    uint32_t cell_mm_x100;    // 单格边长（毫米 × 100）
} CmFabricSize;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t color_index;    // UINT32_MAX = 空
    uint32_t symbol_index;
} CmGridCell;

typedef struct {
    uint32_t color_index;
    uint32_t symbol_index;
    uint32_t stitch_count;
    uint32_t bead_count;
    uint32_t length_cm_x100;
} CmLegendEntry;

typedef struct {
    const char* id;
    const char* name_key;    // i18n key，由 Qt 翻译
    const char* version;
} CmModuleInfo;

typedef struct {
    uint8_t rgb[3];
    char code[16];           // NUL 结尾，最长 15 字符
    char name[48];           // NUL 结尾，最长 47 字符
} CmDmcColor;

// ------------------------------------------------------------
// 版本 / 模块注册表
// ------------------------------------------------------------
const char* cm_version();
const CmModuleInfo* cm_modules_list(size_t* out_len);

// ------------------------------------------------------------
// 十字绣
// ------------------------------------------------------------
void* cm_cross_stitch_generate(const uint8_t* rgba, size_t rgba_len,
                               uint32_t width, uint32_t height,
                               const CmCrossStitchParams* params,
                               const CmStitchOptions* options,
                               CmProgressFn progress_cb, void* user_data,
                               int32_t* err);
/// 创建空图（指定网格尺寸，调色板为空；空盘自由添加颜色后直接编辑）
void* cm_cross_stitch_empty(uint32_t width, uint32_t height,
                            const CmCrossStitchParams* params,
                            const CmStitchOptions* options);
void cm_chart_free(void* chart);
int32_t cm_chart_cells(const void* chart, const CmGridCell** out_ptr, size_t* out_len);
int32_t cm_chart_legend(void* chart, const CmLegendEntry** out_ptr, size_t* out_len);
/// 实际分配到的符号（UTF-32 码点数组，与 legend 一一对应）
int32_t cm_chart_symbols(void* chart, const uint32_t** out_ptr, size_t* out_len);
const char* cm_chart_color_code(const void* chart, size_t index);
const char* cm_chart_color_name(const void* chart, size_t index);
int32_t cm_chart_color_rgb(const void* chart, size_t index, uint8_t* out_rgb);
uint32_t cm_symbol_at(uint32_t index);
/// 符号集内第 index 个符号（UTF-32 码点，越界回绕）
uint32_t cm_symbol_in_set(uint32_t symbol_set, uint32_t index);
uint32_t cm_chart_stitch_total(const void* chart);
int32_t cm_chart_dimensions(const void* chart, uint32_t* out_w, uint32_t* out_h);
/// 调整图案网格尺寸（画布裁剪/扩展，保留图案内容；anchor_left=1 左边固定，
/// =0 右边固定；anchor_top 同理控制上/下边）
int32_t cm_chart_resize(void* chart, uint32_t new_w, uint32_t new_h,
                        uint32_t anchor_left, uint32_t anchor_top);
int32_t cm_chart_set_symbol(void* chart, uint32_t x, uint32_t y, uint32_t symbol_index, int32_t* err);
int32_t cm_chart_to_bytes(const void* chart, uint8_t** out_ptr, size_t* out_len, int32_t* err);
void* cm_chart_from_bytes(const uint8_t* ptr, size_t len, int32_t* err);
void cm_bytes_free(uint8_t* ptr, size_t len);

/// 工程的生成参数与高级选项镜像
int32_t cm_chart_params(const void* chart, CmCrossStitchParams* out_params,
                        CmStitchOptions* out_options);
/// 成品尺寸（网格 × 面料计数 → 毫米）
int32_t cm_chart_fabric_size(const void* chart, CmFabricSize* out);
/// 由目标成品尺寸反推网格数
int32_t cm_grid_for_size(uint32_t width_mm, uint32_t height_mm, uint32_t fabric_count,
                         uint32_t* out_w, uint32_t* out_h);

// ---- 图纸编辑（画布工具 → 命令栈 → 内核） ----
int32_t cm_chart_paint_cell(void* chart, uint32_t x, uint32_t y, uint32_t color_index,
                            uint32_t* out_count, int32_t* err);
int32_t cm_chart_flood_fill(void* chart, uint32_t x, uint32_t y, uint32_t color_index,
                            uint32_t* out_count, int32_t* err);
int32_t cm_chart_rect_fill(void* chart, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                           uint32_t color_index, uint32_t* out_count, int32_t* err);
int32_t cm_chart_fill_cells(void* chart, const uint32_t* xs, const uint32_t* ys, size_t count,
                            uint32_t color_index, uint32_t* out_count, int32_t* err);
int32_t cm_chart_mirror_region(void* chart, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                               int32_t horizontal, uint32_t* out_count, int32_t* err);
int32_t cm_chart_mirror_region_axis(void* chart, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                                    double ax0, double ay0, double ax1, double ay1,
                                    uint32_t* out_count, int32_t* err);
int32_t cm_chart_scale_region(void* chart, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                              uint32_t dst_x0, uint32_t dst_y0,
                              uint32_t new_w, uint32_t new_h,
                              uint32_t* out_count, int32_t* err);
int32_t cm_chart_move_region(void* chart, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                             int32_t dx, int32_t dy,
                             uint32_t* out_count, int32_t* err);
int32_t cm_chart_flood_region(void* chart, uint32_t x, uint32_t y,
                              uint32_t* out_indices, uint32_t* out_count, int32_t* err);
int32_t cm_chart_replace_color(void* chart, uint32_t from, uint32_t to,
                               uint32_t* out_count, int32_t* err);
int32_t cm_chart_merge_color(void* chart, uint32_t from, uint32_t to,
                             uint32_t* out_count, int32_t* err);
int32_t cm_chart_delete_color(void* chart, uint32_t color_index,
                              uint32_t* out_count, int32_t* err);
int32_t cm_chart_set_color_symbol(void* chart, uint32_t color_index, uint32_t new_symbol,
                                  int32_t* out_swapped, int32_t* err);
int32_t cm_chart_cleanup(void* chart, uint32_t strength, uint32_t* out_count);
int32_t cm_chart_merge_rare(void* chart, uint32_t ppt, uint32_t* out_count);
int32_t cm_chart_prune_unused(void* chart, uint32_t* out_count);
int32_t cm_chart_set_symbol_set(void* chart, uint32_t symbol_set);
int32_t cm_chart_redistribute_symbols(void* chart);
int32_t cm_chart_add_color(void* chart, const char* code);

// ------------------------------------------------------------
// 纸样几何
// ------------------------------------------------------------
void* cm_pattern_seam_rect(double width_mm, double height_mm, double seam_mm, int32_t* err);
int32_t cm_paths_outline(const void* paths, const CmPoint** out_ptr, size_t* out_len);
int32_t cm_paths_seam(const void* paths, const CmPoint** out_ptr, size_t* out_len);
void cm_paths_free(void* paths);

// ------------------------------------------------------------
// 历史栈（撤销 / 重做）
// ------------------------------------------------------------
void* cm_history_new(size_t max_depth);
void cm_history_free(void* history);
int32_t cm_history_init(void* history, const uint8_t* state, size_t len);
int32_t cm_history_push(void* history, const char* label, const uint8_t* state, size_t len);
int32_t cm_history_undo(void* history, const uint8_t** out_ptr, size_t* out_len);
int32_t cm_history_redo(void* history, const uint8_t** out_ptr, size_t* out_len);
int32_t cm_history_can_undo(void* history);
int32_t cm_history_can_redo(void* history);
int32_t cm_history_current(void* history, const uint8_t** out_ptr, size_t* out_len);
int32_t cm_history_clear(void* history);

// ------------------------------------------------------------
// 导出
// ------------------------------------------------------------
int32_t cm_export_csv(const void* chart, const char* path, int32_t* err);
int32_t cm_export_png(const void* chart, const char* path, uint32_t cell_px, int32_t* err);
int32_t cm_export_pdf(const void* chart, const char* path, const char* title, int32_t* err);
int32_t cm_export_dxf_piece(const void* paths, const char* path, int32_t* err);

// ------------------------------------------------------------
// 工程文件（.bean）
// ------------------------------------------------------------
int32_t cm_project_save(const void* chart, const char* path, int32_t* err);
void* cm_project_open(const char* path, int32_t* err);

// ------------------------------------------------------------
// 素材库
// ------------------------------------------------------------
void* cm_material_db_new(void);
void cm_material_db_free(void* db);
int32_t cm_material_find_dmc(void* db, const char* code, CmDmcColor* out);
int32_t cm_material_count(void* db, const char* kind);
int32_t cm_material_all(void* db, const char* kind, CmDmcColor* out, size_t out_cap);

} // extern "C"

// 错误码（与 rust_core common::error::codes 对应）
namespace cm {
namespace codes {
constexpr int OK = 0;
constexpr int INVALID_ARG = 1;
constexpr int NULL_PTR = 2;
constexpr int UTF8 = 3;
constexpr int INTERNAL = 4;
constexpr int IMAGE_DECODE = 1001;
constexpr int IMAGE_TOO_LARGE = 1002;
constexpr int QUANTIZE_FAILED = 1003;
constexpr int GEOMETRY_INVALID = 2001;
constexpr int DB_ERROR = 3001;
constexpr int PDF_FAILED = 4001;
constexpr int PROJECT_CORRUPT = 5001;
constexpr int PROJECT_VERSION = 5002;
constexpr int PROJECT_UNSUPPORTED_MODULE = 5003;
} // namespace codes

/// 符号集（与 rust ffi_types::symbol 一致）
namespace symbol {
constexpr uint32_t LETTERS = 0;
constexpr uint32_t DIGITS = 1;
constexpr uint32_t GEOMETRIC = 2;
constexpr uint32_t MIXED = 3;
} // namespace symbol

/// 下采样归并策略（与 rust ffi_types::sample 一致）
namespace sample {
constexpr uint32_t MEAN = 0;
constexpr uint32_t MODE = 1;
constexpr uint32_t MEDOID = 2;
} // namespace sample

/// 去杂点强度（与 rust ffi_types::cleanup 一致）
namespace cleanup {
constexpr uint32_t OFF = 0;
constexpr uint32_t LIGHT = 1;
constexpr uint32_t STRONG = 2;
} // namespace cleanup

/// 手作品类模式（与 rust ffi_types::craft 一致）
namespace craft {
constexpr uint32_t CROSS_STITCH = 0;  ///< 十字绣（默认）：DMC 绣线色卡
constexpr uint32_t BEAD = 1;          ///< 珠绣：每格一粒珠，DMC 珠色卡
constexpr uint32_t PERLER = 2;        ///< 拼豆：每格一粒 5mm 豆，Artkal S 色卡
} // namespace craft

/// 空颜色哨兵（color_index = UINT32_MAX 表示该格留空）
constexpr uint32_t kColorNone = 0xFFFFFFFFu;
} // namespace cm
