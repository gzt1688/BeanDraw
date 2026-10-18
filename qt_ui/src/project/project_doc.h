#pragma once

#include "canvas/scene.h"
#include "ffi_bind/ffi_types.h"
#include "ffi_bind/rust_api.h"

#include <QObject>
#include <QSize>
#include <QString>
#include <QTimer>

/// .bean 工程文档（开发大纲第 3/13/14 章）：
/// 持有 Rust 图表句柄 + 命令栈；保存 / 打开 / 撤销 / 重做 / 自动保存。
class ProjectDoc : public QObject
{
    Q_OBJECT

public:
    explicit ProjectDoc(QObject* parent = nullptr);

    /// 当前图表句柄（可能为空）。
    void* chart() const { return m_chart.get(); }

    /// 同 `chart()`，语义更明确的别名（面板 / 工具调用内核接口时用）。
    void* chartHandle() const { return m_chart.get(); }

    /// 是否已存在可编辑的图表。
    bool isValid() const { return m_chart != nullptr; }

    QString currentPath() const { return m_path; }
    bool isModified() const { return m_modified; }

    /// 新建空工程（选项缺省与 Rust 侧 Default 一致）。
    void newChart(const QSize& grid, const CmCrossStitchParams& params,
                  const CmStitchOptions& options = rust_api::defaultStitchOptions());

    /// 接管生成完成的图表（来自属性面板生成流程）。
    void adoptChart(rust_api::ChartHandle chart, const QString& sourceName);

    /// 清空当前图表（切换到尚无图纸的模式时）。
    void clearChart();

    /// 当前生成参数 / 高级选项镜像。
    CmCrossStitchParams params() const { return m_params; }
    CmStitchOptions options() const { return m_options; }

    /// 直接改内核图表后的刷新（编辑操作统一入口）。
    /// 会就地更新画布并发出 chartChanged；网格尺寸变化时重建图元。
    void syncFromCore(const QString& historyLabel);

    /// 保存 / 另存 / 打开。
    bool save();
    bool saveAs(const QString& path);
    bool open(const QString& path);

    /// 命令栈。
    void pushHistory(const QString& label);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    /// 清空撤销/重做栈（切换模式图纸时调用，避免跨模式撤销串档）。
    void resetHistory();

    /// 画布 / Dock 刷新用快照。
    ChartSnapshot chartSnapshot() const;

    /// 启动定时自动保存（毫秒）。
    void startAutosave(int intervalMs = 5 * 60 * 1000);

    /// 崩溃恢复：检查自动保存目录，存在待恢复文件返回路径。
    static QString findAutosave();

    /// 打开自动保存的工程（恢复流程）。
    bool openAutosave(const QString& path);

    /// 清除自动保存文件。
    static void clearAutosave();

signals:
    void chartChanged();
    void statusMessage(const QString& text, int timeoutMs = 3000);
    void modificationChanged(bool modified);
    void undoRedoChanged();

private:
    QByteArray serializeState() const;
    void applyState(const QByteArray& state);
    void touch();
    QString autosaveFilePath() const;

    rust_api::ChartHandle m_chart{nullptr, [](void*) {}};
    rust_api::HistoryHandle m_history;
    QString m_path;
    QString m_sourceName;
    CmCrossStitchParams m_params{};
    CmStitchOptions m_options{};
    bool m_modified = false;
    QTimer m_autosaveTimer;
};
