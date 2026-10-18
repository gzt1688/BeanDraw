#include "tr_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>

TrManager::TrManager(QObject* parent)
    : QObject(parent)
{
}

TrManager& TrManager::instance()
{
    static TrManager instance;
    return instance;
}

QStringList TrManager::supportedLanguages()
{
    return {QStringLiteral("zh_CN"), QStringLiteral("en_US")};
}

void TrManager::initialize()
{
    QString language = QLocale::system().name(); // 例如 zh_CN
    if (!supportedLanguages().contains(language)) {
        language = QStringLiteral("en_US");
    }
    if (!switchLanguage(language)) {
        switchLanguage(QStringLiteral("en_US"));
    }
}

bool TrManager::switchLanguage(const QString& language)
{
    if (!installTranslator(language)) {
        return false;
    }
    m_current = language;
    QLocale::setDefault(QLocale(language));
    emit languageChanged(language);
    return true;
}

bool TrManager::installTranslator(const QString& language)
{
    QCoreApplication::removeTranslator(&m_appTranslator);

    // qm 查找顺序：exe 同目录 translations/ → exe 同目录 → 资源 /i18n
    //（qt_add_translations 生成的 qrc 前缀是 /i18n，不带 translations 子目录）
    const QString baseName = QStringLiteral("beandraw_%1").arg(language);
    const QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
        QCoreApplication::applicationDirPath(),
        QStringLiteral(":/i18n"),
    };

    for (const QString& dir : searchPaths) {
        const QString filePath = QDir(dir).absoluteFilePath(baseName + QStringLiteral(".qm"));
        if (QFile::exists(filePath) && m_appTranslator.load(filePath)) {
            QCoreApplication::installTranslator(&m_appTranslator);
            return true;
        }
    }
    return false;
}
