#include "startup_tool.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace {

// Strip freedesktop field codes (%f %F %u %U %i %c %k …) from an Exec line.
// Autostart entries run with no file/URL arguments, so leaving the codes in
// would pass literal "%F" to the program (or make a sh -c wrapper malformed).
QString stripFieldCodes(QString exec)
{
    exec.replace(QRegularExpression("%[fFuUdDnNickvm]"), "");
    return exec.simplified();
}

// Read a .desktop file's [Desktop Entry] group into an ordered key→value map.
// QSettings parses freedesktop files fine for *reading*; the trouble is only
// on write (it re-quotes/escapes values in a way desktops reject), so all
// writing below goes through writeDesktopEntry() as plain UTF-8 text instead.
QMap<QString, QString> readDesktopEntry(const QString &path)
{
    QMap<QString, QString> map;
    QSettings ini(path, QSettings::IniFormat);
    ini.beginGroup("Desktop Entry");
    for (const QString &key : ini.childKeys())
        map.insert(key, ini.value(key).toString());
    return map;
}

// Write a freedesktop-compliant .desktop file as plain UTF-8 text. This is the
// fix for "Add didn't work": QSettings mangled Exec/Name (quoting, unicode
// escapes) so desktop environments silently ignored the generated entry.
bool writeDesktopEntry(const QString &path, const QMap<QString, QString> &map)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << "[Desktop Entry]\n";
    // Type first for readability; the rest in insertion/sorted order.
    if (map.contains("Type"))
        out << "Type=" << map.value("Type") << "\n";
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.key() == "Type") continue;
        out << it.key() << "=" << it.value() << "\n";
    }
    out.flush();
    f.close();
    return true;
}

// Wrap a command so it launches `delay` seconds after login. Portable across
// desktops (unlike the GNOME/KDE-specific delay keys) — we shell out to a
// sleep. The original command is stored separately so the UI can show it clean.
QString buildDelayedExec(const QString &original, int delay)
{
    if (delay <= 0) return original;
    QString inner = stripFieldCodes(original);
    inner.replace('\\', "\\\\").replace('"', "\\\"");
    return QString("sh -c \"sleep %1 && exec %2\"").arg(delay).arg(inner);
}

} // namespace

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
        const auto map = readDesktopEntry(path);

        StartupEntry e;
        e.filePath = path;
        e.name     = map.value("Name");
        e.comment  = map.value("Comment");
        e.icon     = map.value("Icon");
        // X-GNOME-Autostart-enabled defaults to true when absent.
        e.enabled  = map.value("X-GNOME-Autostart-enabled", "true").toLower() != "false";
        // If we wrote a delayed wrapper, prefer the stored original command +
        // delay so the row shows the real program, not the sh -c wrapper.
        if (map.contains("X-GTStacer-Exec")) {
            e.exec         = map.value("X-GTStacer-Exec");
            e.delaySeconds = map.value("X-GTStacer-Delay", "0").toInt();
        } else {
            e.exec = map.value("Exec");
        }
        result << e;
    }
    return result;
}

bool StartupTool::enable(const QString &filePath)
{
    auto map = readDesktopEntry(filePath);
    map["X-GNOME-Autostart-enabled"] = "true";
    return writeDesktopEntry(filePath, map);
}

bool StartupTool::disable(const QString &filePath)
{
    auto map = readDesktopEntry(filePath);
    map["X-GNOME-Autostart-enabled"] = "false";
    return writeDesktopEntry(filePath, map);
}

bool StartupTool::remove(const QString &filePath)
{
    return QFile::remove(filePath);
}

bool StartupTool::add(const StartupEntry &entry)
{
    QString base = entry.name.simplified().replace(' ', '-');
    if (base.isEmpty()) base = "gt-stacer-entry";
    QString path = autostartDir() + "/" + base + ".desktop";

    QMap<QString, QString> map;
    map["Type"]    = "Application";
    map["Name"]    = entry.name;
    map["Exec"]    = buildDelayedExec(entry.exec, entry.delaySeconds);
    map["Comment"] = entry.comment;
    if (!entry.icon.isEmpty())
        map["Icon"] = entry.icon;
    map["Terminal"] = "false";
    map["X-GNOME-Autostart-enabled"] = entry.enabled ? "true" : "false";
    // Round-trip metadata so entries() can restore the clean command + delay.
    if (entry.delaySeconds > 0) {
        map["X-GTStacer-Exec"]  = entry.exec;
        map["X-GTStacer-Delay"] = QString::number(entry.delaySeconds);
    }
    return writeDesktopEntry(path, map);
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
    // Flatpak exports (system-wide + per-user).
    searchDirs << "/var/lib/flatpak/exports/share/applications"
               << userBase + "/flatpak/exports/share/applications";
    // Snap desktop files.
    searchDirs << "/var/lib/snapd/desktop/applications";

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
