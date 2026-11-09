#include "startup_tool.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSaveFile>
#include <QLocale>
#include "../Utils/desktop_entry.h"
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

// .desktop files are read and written with Utils/desktop_entry (26.12), never
// QSettings: its INI parser treats ';' as a comment, re-joins quoted Exec
// values and turns ',' into lists — and the old writer dropped every group
// except [Desktop Entry] (e.g. [Desktop Action …]) on each enable/disable.
bool isTrue(const QString &v) { return v.trimmed().compare("true", Qt::CaseInsensitive) == 0; }

bool writeEntry(const QString &path, const DesktopEntry &de)
{
    QSaveFile f(path);                   // atomic: never a half-written launcher
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(de.serialize());
    return f.commit();
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
        const DesktopEntry de = DesktopEntry::read(path);
        if (!de.isValid()) continue;
        const QString loc = QLocale().name();

        StartupEntry e;
        e.filePath = path;
        e.name     = de.localized("Name", loc);
        e.comment  = de.localized("Comment", loc);
        e.icon     = de.value("Icon");
        if (e.name.isEmpty()) e.name = QFileInfo(file).completeBaseName();
        // X-GNOME-Autostart-enabled defaults to true when absent; Hidden=true
        // means "disabled" for every desktop (freedesktop autostart spec).
        e.enabled  = (!de.contains("X-GNOME-Autostart-enabled") || isTrue(de.value("X-GNOME-Autostart-enabled")))
                  && !isTrue(de.value("Hidden"));
        // If we wrote a delayed wrapper, prefer the stored original command +
        // delay so the row shows the real program, not the sh -c wrapper.
        if (de.contains("X-GTStacer-Exec")) {
            e.exec         = de.value("X-GTStacer-Exec");
            e.delaySeconds = de.value("X-GTStacer-Delay").toInt();
        } else {
            e.exec = de.value("Exec");
        }
        result << e;
    }
    return result;
}

bool StartupTool::enable(const QString &filePath)
{
    DesktopEntry de = DesktopEntry::read(filePath);
    if (!de.isValid()) return false;
    de.set("X-GNOME-Autostart-enabled", "true");
    if (isTrue(de.value("Hidden"))) de.removeKey("Hidden");   // Hidden=true also disables
    return writeEntry(filePath, de);
}

bool StartupTool::disable(const QString &filePath)
{
    DesktopEntry de = DesktopEntry::read(filePath);
    if (!de.isValid()) return false;
    de.set("X-GNOME-Autostart-enabled", "false");
    return writeEntry(filePath, de);
}

bool StartupTool::remove(const QString &filePath)
{
    return QFile::remove(filePath);
}

bool StartupTool::add(const StartupEntry &entry)
{
    // A plain, portable file name (no quotes, slashes or spaces) that never
    // overwrites another entry with the same name.
    QString base = entry.name.simplified();
    base.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "-");
    base.remove(QRegularExpression("^[-.]+|-+$"));
    if (base.isEmpty()) base = "gt-stacer-entry";
    QString path = autostartDir() + "/" + base + ".desktop";
    for (int n = 2; QFile::exists(path); ++n)
        path = autostartDir() + "/" + base + "-" + QString::number(n) + ".desktop";

    DesktopEntry de = DesktopEntry::parse("[Desktop Entry]\n");
    de.set("Type", "Application");
    de.set("Name", entry.name);
    de.set("Exec", buildDelayedExec(entry.exec, entry.delaySeconds));
    if (!entry.comment.isEmpty()) de.set("Comment", entry.comment);
    if (!entry.icon.isEmpty())    de.set("Icon", entry.icon);
    de.set("Terminal", "false");
    de.set("X-GNOME-Autostart-enabled", entry.enabled ? "true" : "false");
    // Round-trip metadata so entries() can restore the clean command + delay.
    if (entry.delaySeconds > 0) {
        de.set("X-GTStacer-Exec",  entry.exec);
        de.set("X-GTStacer-Delay", QString::number(entry.delaySeconds));
    }
    return writeEntry(path, de);
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
            const DesktopEntry de = DesktopEntry::read(path);
            if (!de.isValid()) continue;

            // Skip hidden / no-display entries — they're not user-launchable.
            if (isTrue(de.value("NoDisplay")))  continue;
            if (isTrue(de.value("Hidden")))     continue;
            if (de.value("Type") != "Application") continue;

            const QString loc = QLocale().name();
            StartupEntry e;
            e.filePath = path;
            e.name     = de.localized("Name", loc);
            e.exec     = de.value("Exec");
            e.comment  = de.localized("Comment", loc);
            e.icon     = de.value("Icon");
            e.enabled  = true;
            if (e.name.isEmpty()) e.name = QFileInfo(f).baseName();
            result << e;
        }
    }
    return result;
}
