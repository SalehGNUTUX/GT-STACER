#include "backup_tool.h"
#include <QDir>

QString BackupTool::homePath()
{
    return QDir::homePath();
}

QStringList BackupTool::defaultExcludes()
{
    // Regenerable / churny / sensitive-to-copy paths (relative to home).
    return {
        ".cache",
        ".local/share/Trash",
        ".thumbnails",
        ".local/share/baloo",      // KDE file index
        ".local/share/akonadi",    // KDE PIM cache
        ".mozilla/*/Cache*",
        ".config/*/Cache*",
        ".steam", ".local/share/Steam",
        "*/node_modules",
        ".gvfs",
        "*.tmp", "*.part",
    };
}

QStringList BackupTool::rsyncArgs(const QString &dest,
                                  const QStringList &excludes,
                                  bool mirror)
{
    // -a archive, --info=progress2 gives an overall % the page can parse,
    // --human-readable for the summary, --delete for a true mirror.
    QStringList a = {"-a", "--info=progress2", "--human-readable"};
    if (mirror) a << "--delete";
    for (const QString &e : excludes) a << "--exclude" << e;
    // Trailing slash on the source copies its *contents* into dest.
    a << homePath() + "/" << dest + "/";
    return a;
}

QStringList BackupTool::restoreArgs(const QString &backupDir,
                                    const QStringList &excludes)
{
    // Restore is NOT a mirror (never --delete into the live home).
    QStringList a = {"-a", "--info=progress2", "--human-readable"};
    for (const QString &e : excludes) a << "--exclude" << e;
    a << backupDir + "/" << homePath() + "/";
    return a;
}
