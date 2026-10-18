#include "workspace.h"

#include <QMainWindow>
#include <QSettings>

Workspace& Workspace::instance()
{
    static Workspace instance;
    return instance;
}

QString Workspace::settingsGroup()
{
    return QStringLiteral("workspaces");
}

void Workspace::save(const QString& name, QMainWindow* window, const QByteArray& dockState,
                     const QString& currentTool)
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.beginGroup(name);
    settings.setValue(QStringLiteral("dockState"), dockState);
    settings.setValue(QStringLiteral("currentTool"), currentTool);
    settings.endGroup();
    settings.endGroup();
    Q_UNUSED(window)
}

bool Workspace::restore(const QString& name, QMainWindow* window, QString* currentTool)
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.beginGroup(name);
    const QByteArray state = settings.value(QStringLiteral("dockState")).toByteArray();
    const QString tool = settings.value(QStringLiteral("currentTool")).toString();
    settings.endGroup();
    settings.endGroup();

    if (state.isEmpty()) {
        return false;
    }
    applyDockState(window, state);
    if (currentTool) {
        *currentTool = tool;
    }
    return true;
}

void Workspace::applyDockState(QMainWindow* window, const QByteArray& state)
{
    if (window && !state.isEmpty()) {
        window->restoreState(state);
    }
}

QByteArray Workspace::currentDockState(QMainWindow* window)
{
    return window ? window->saveState() : QByteArray();
}

QStringList Workspace::names() const
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const QStringList result = settings.childGroups();
    settings.endGroup();
    return result;
}
