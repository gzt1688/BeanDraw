#pragma once

#include <QColor>
#include <QIcon>

/// 工具栏 / 面板图标工厂（开发大纲 ui/icons）。
///
/// 用 QPainterPath 矢量绘制而非位图资源：
/// - 无需新增 png/svg 资源与 qrc 条目
/// - 可按当前主题自动取色，深浅主题均清晰（第 7 章 UI 适配）
/// - 任意 DPI 下不失真
class IconFactory
{
public:
    /// 图标语义标识。
    enum class Kind {
        Select,      ///< 选择 / 浏览
        Pan,         ///< 平移
        Zoom,        ///< 缩放
        ZoomIn,      ///< 缩放放大（放大镜 + 镜内加号）
        ZoomOut,     ///< 缩放缩小（放大镜 + 镜内减号）
        FitView,     ///< 适应窗口
        Stitch,      ///< 绣笔（单格涂色）
        Fill,        ///< 油漆桶（区域填充）
        RectFill,    ///< 矩形填充
        Picker,      ///< 吸管（拾取绣色）
        Eraser,      ///< 橡皮（擦除）
        Cleanup,     ///< 去杂点
        Merge,       ///< 稀有色合并
        Undo,        ///< 撤销
        Redo,        ///< 重做
        Symbol,      ///< 符号视图
        ColorBlock,  ///< 色块视图
        StitchPreview, ///< 十字针迹预览
        Import,      ///< 导入图片
        Generate,    ///< 生成图纸
        Perler,      ///< 拼豆圆环视图
        AddPlate,    ///< 新增盘（盘 + 加号）
        CircleFill,  ///< 圆形填充
        EllipseFill, ///< 椭圆填充
        PolygonFill, ///< 多边形填充
        SelectRegion, ///< 选区工具（虚线框 + 控制柄）
        SelectionDelete, ///< 删除选区
        Mirror,      ///< 镜像工具（十字虚轴 + 四向箭头，画布点选镜像轴）
        MirrorH,     ///< 水平镜像
        MirrorV,     ///< 垂直镜像
        Highlight,   ///< 高亮区域（区域识别观察：网格内亮 + 虚线外框）
        Text,        ///< 文字（大写字母 A）
        Print,       ///< 打印（打印机外形：纸槽 + 机身 + 出纸口 + 指示灯）
    };

    /// 生成图标（32×32 逻辑像素，多倍图标引擎自适应）。
    static QIcon make(Kind kind, const QColor& foreground, int logicalSize = 24);

    /// 图标语义名（用于 QSS objectName 定位与无障碍描述）。
    static const char* name(Kind kind);

    /// 工具栏图标边长（逻辑像素）。
    ///
    /// 24 在 QToolButton 的 padding 里显得偏小，图形再饱满也撑不起视觉重量；
    /// 30 能让图标成为按钮里的绝对主体，同时工具栏总高仍在 48px 以内。
    static constexpr int kToolBarIconSize = 30;
};
