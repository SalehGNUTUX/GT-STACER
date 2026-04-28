#include "startup_tool.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

QString StartupTool::autostartDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QDir().mkpath(dir);
    return dir;
}

QVector<StartupEntry> StartupTool::entries()
{
    QVector<StartupEntry> result;
    QDir dir(autostartDir());
    if (!dir.exists()) return result;

    for (const auto &file : dir.entryList({"*.desktop"}, QDir::Files)) {
        QString path = dir.filePath(file);
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup("Desktop Entry");

        StartupEntry e;
        e.filePath = path;
        e.name     = ini.value("Name").toString();
        e.exec     = ini.value("Exec").toString();
        e.comment  = ini.value("Comment").toString();
        e.icon     = ini.value("Icon").toString();
        e.enabled  = ini.value("X-GNOME-Autostart-enabled", true).toBool();
        result << e;
    }
    return result;
}

bool StartupTool::enable(const QString &filePath)
{
    QSettings ini(filePath, QSettings::IniFormat);
    ini.beginGroup("Desktop Entry");
    ini.setValue("X-GNOME-Autostart-enabled", true);
    return true;
}

bool StartupTool::disable(const QString &filePath)
{
    QSettings ini(filePath, QSettings::IniFormat);
    ini.beginGroup("Desktop Entry");
    ini.setValue("X-GNOME-Autostart-enabled", false);
    return true;
}

bool StartupTool::remove(const QString &filePath)
{
    return QFile::remove(filePath);
}

bool StartupTool::add(const StartupEntry &entry)
{
    QString path = autostartDir() + "/" + entry.name.simplified().replace(' ', '-') + ".desktop";
    QSettings ini(path, QSettings::IniFormat);
    ini.beginGroup("Desktop Entry");
    ini.setValue("Type",    "Application");
    ini.setValue("Name",    entry.name);
    ini.setValue("Exec",    entry.exec);
    ini.setValue("Comment", entry.comment);
    ini.setValue("Icon",    entry.icon);
    ini.setValue("X-GNOME-Autostart-enabled", entry.enabled);
    return true;
}
