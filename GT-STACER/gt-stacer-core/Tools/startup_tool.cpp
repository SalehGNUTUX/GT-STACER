#include "startup_tool.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
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

QVector<StartupEntry> StartupTool::systemApplications()
{
    QVector<StartupEntry> result;

    // XDG_DATA_DIRS lookup. Defaults to /usr/local/share:/usr/share when unset.
    QStringList searchDirs;
    QByteArray xdgData = qgetenv("XDG_DATA_DIRS");
    QStringList raw = QString::fromLocal8Bit(xdgData).split(':', Qt::SkipEmptyParts);
    if (raw.isEmpty()) raw = {"/usr/local/share", "/usr/share"};
    for (const auto &d : raw) searchDirs << d + "/applications";
    // User-local entries from XDG_DATA_HOME / ~/.local/share.
    QByteArray xdgHome = qgetenv("XDG_DATA_HOME");
    QString userBase = xdgHome.isEmpty()
        ? QDir::homePath() + "/.local/share"
        : QString::fromLocal8Bit(xdgHome);
    searchDirs << userBase + "/applications";
    // Flatpak exports.
    searchDirs << "/var/lib/flatpak/exports/share/applications"
               << userBase + "/flatpak/exports/share/applications";

    QSet<QString> seen; // by basename, so user-local entries override system-wide.

    for (const QString &dir : searchDirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        const auto files = d.entryList({"*.desktop"}, QDir::Files | QDir::NoSymLinks);
        for (const QString &f : files) {
            if (seen.contains(f)) continue;
            seen.insert(f);

            QString path = d.filePath(f);
            QSettings ini(path, QSettings::IniFormat);
            ini.beginGroup("Desktop Entry");

            // Skip hidden / no-display entries — they're not user-launchable.
            if (ini.value("NoDisplay").toBool())  continue;
            if (ini.value("Hidden").toBool())     continue;
            if (ini.value("Type").toString() != "Application") continue;

            StartupEntry e;
            e.filePath = path;
            e.name     = ini.value("Name").toString();
            e.exec     = ini.value("Exec").toString();
            e.comment  = ini.value("Comment").toString();
            e.icon     = ini.value("Icon").toString();
            e.enabled  = true;
            if (e.name.isEmpty()) e.name = QFileInfo(f).baseName();
            result << e;
        }
    }
    return result;
}
