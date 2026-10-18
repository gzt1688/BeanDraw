#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>

/// 多语言管理（开发大纲第 7 章）。
/// UI 文本一律 tr()；本类负责 qm 加载 / 切换 / 重翻译事件。
class TrManager : public QObject
{
    Q_OBJECT

public:
    static TrManager& instance();

    /// 加载翻译文件（按系统 locale，回落 en_US）。
    void initialize();

    /// 切换语言，如 "zh_CN" / "en_US"。成功返回 true。
    bool switchLanguage(const QString& language);

    QString currentLanguage() const { return m_current; }

    /// 支持的语言列表（与 ts 文件对应）。
    static QStringList supportedLanguages();

signals:
    /// 语言切换后发出，窗口据此 retranslateUi。
    void languageChanged(const QString& language);

private:
    explicit TrManager(QObject* parent = nullptr);
    bool installTranslator(const QString& language);

    QTranslator m_appTranslator;
    QString m_current;
};
