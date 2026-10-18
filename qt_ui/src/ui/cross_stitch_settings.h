#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/// 多盘系统盘数上限（类拓竹多平台）。
inline constexpr int kMaxPlateCount = 25;

/// 十字绣专用设置（用量估算 + 布料 + 图案生成 + 画布显示）。
///
/// 与 `ThemeConfig` 同为「应用级偏好」，持久化在 QSettings 的
/// `crossStitch/*` 分组下，与具体工程无关。
///
/// 用量估算公式（全部可由用户调整，避免黑箱）：
///
/// ```text
/// 单针工作线长  = stitchLengthMm × (14 / fabricCount)   // 面料计数越高，单针越省线
/// 单束可用长度  = skeinMeters × 1000 × (6 / strands)    // 6 股线按所用股数分成若干捆
/// 每束可用针数  = 单束可用长度 / 单针工作线长
/// 含余量针数    = 每束可用针数 / (1 + flossMarginPct/100) + extraStitchesPerColor
/// ```
///
/// 其中 `extraStitchesPerColor` 覆盖每色起针 / 收针的额外消耗。
struct CrossStitchSettings
{
    // ---- 用线量计算 ----
    /// 单针工作线长（毫米，含两根对角与背面走线）。14ct 经验值 9mm。
    double stitchLengthMm = 9.0;
    /// 绣线股数（1..6）。2 股为 14ct 常规用法。
    int strands = 2;
    /// 每束绣线长度（米）。DMC 标准 6 股线为 8 米。
    double skeinMeters = 8.0;
    /// 用线余量（百分比），覆盖起收针、翻面、断线等损耗。
    int flossMarginPct = 20;

    // ---- 珠绣 ----
    /// 珠子余量（百分比）。珠绣易掉珠 / 需补绣，余量通常高于用线。
    int beadMarginPct = 15;
    /// 珠子规格（毫米），常用 2 / 3 / 4 / 5 mm 米字珠。
    double beadSizeMm = 3.0;
    /// 珠绣时每格是否按 1 粒珠计算（关闭则珠子数 = 针数 × 0）。
    bool countBeads = true;

    // ---- 布料 ----
    /// 面料计数 ct（每英寸格数，6..32）。
    int fabricCount = 14;
    /// 成品四周额外留边（毫米），用于装裱。装框常留 75mm（3 英寸）。
    int borderMarginMm = 75;
    /// 每色起收针额外针数。
    int extraStitchesPerColor = 0;
    /// 图案覆盖度（百分比）：实际落针格数占网格总数的比例，用于整体预算。
    int coveragePct = 100;

    // ---- 图案生成 ----
    /// 最大颜色数（1..64）。
    int maxColors = 16;
    /// 符号集：0 字母 / 1 数字 / 2 几何 / 3 混合。
    int symbolSet = 0;
    /// 去杂点强度：0 关 / 1 弱 / 2 强。
    int cleanup = 1;
    /// 稀有色合并阈值（千分数，0 = 不合并）。
    int rareColorPpt = 3;
    /// 抖动（半调）：0 关 / 1 开。
    bool dither = false;

    // ---- 画布显示 ----
    /// 是否显示符号。
    bool showSymbols = true;
    /// 是否显示网格线。
    bool showGrid = true;
    /// 粗网格线间隔（每 N 格一条，用于数格）。
    int majorGridEvery = 10;
    /// 盘数（1..25）：大图分成几块独立盘（类拓竹多平台），默认单盘。
    int plateCount = 1;
    /// 每盘四周留边格数：盘 = 图案 + 四周留边（默认 10 格）。
    int plateMarginCells = 10;
    /// 自定义盘名（空 = 用默认"盘 N"；仅前 plateCount 项生效）。
    QStringList plateNames;

    // ---- 材料表面板列显示 ----
    /// 是否显示「名称」列（DMC 官方色名，多数图案不需要，默认关）。
    bool showColorNameColumn = false;
    /// 是否显示「针数」列。
    bool showStitchesColumn = true;
    /// 是否显示「珠子数」列。
    bool showBeadsColumn = true;
    /// 是否显示「用线」列。
    bool showThreadColumn = true;

    // ---- 快捷键（拼豆设置 → 快捷键页设置；QKeySequence::toString 存储）----
    /// 吸管（取色）工具快捷键，默认 Q。
    QString keyPicker = QStringLiteral("Q");
    /// 画布最大化（看图模式）快捷键，默认空格。
    QString keyViewMax = QStringLiteral("Space");

    // ---- 打印（图纸分页：按格自动拆页，每页带格号标尺，可拼回完整图）----
    /// 打印纸张：0 A5 / 1 A4（默认）/ 2 A3 / 3 Letter / 4 B5。
    int printPaperKind = 1;
    /// 打印方向：false 纵向 / true 横向。
    bool printLandscape = false;
    /// 每格打印尺寸（毫米）：决定单页能装下多少格（越小页数越多、越清晰）。
    double printCellMm = 6.0;
    /// 打印页边距（毫米）：四周留白（不含格号标尺带），0-30，默认 10。
    double printMarginMm = 10.0;
    /// 材料表打印整体缩放（%）：字号 / 内边距按比例缩放。100% = 放大后的
    /// 初始大小（即原 12pt 基准 ×2.8 的 34pt 档），50-300 在基准上再缩放。
    int printLegendScalePct = 100;

    // ---- 盘锁定（随盘存活：锁定后不可编辑，也不接收生成的图纸）----
    /// 每盘锁定状态（下标 = 盘号，缺省 = 未锁定；与 plateNames 同生命周期）。
    QVector<bool> plateLocked;

    /// 出厂默认。
    static CrossStitchSettings defaults();

    /// 从 QSettings 读取（缺失项回落默认值）。
    void load();
    /// 写回 QSettings。
    void save() const;

    /// 当前面料计数下单针工作线长（毫米）。
    double effectiveStitchLengthMm() const;

    /// 某色用 `stitches` 针需要几束绣线（向上取整，至少 1）。
    int skeinsFor(quint32 stitches) const;

    /// 某色用 `stitches` 针需要多少粒珠子（含余量，向上取整）。
    int beadsFor(quint32 stitches) const;

    /// `cells` 格宽的布料需要裁多大（毫米，含额外留边）。
    int requiredFabricSideMm(int cells) const;

    /// 把非法取值夹回有效区间（读取外部配置后调用）。
    void sanitize();
};
