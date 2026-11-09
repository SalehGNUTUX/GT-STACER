#include "backup_tool.h"
#include "../Utils/command_util.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

QString BackupTool::homePath()
{
    return QDir::homePath();
}

QString BackupTool::markerName() { return QStringLiteral(".gt-stacer-backup"); }

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

BackupTool::FsKind BackupTool::classify(const QString &t)
{
    const QString f = t.toLower();
    if (f == "vfat" || f == "msdos" || f == "fat" || f == "umsdos") return FsKind::Fat32;
    if (f == "exfat")                                               return FsKind::ExFat;
    if (f == "ntfs" || f == "ntfs3" || f == "ntfs-3g")              return FsKind::Ntfs;
    if (f == "cifs" || f == "smb3" || f == "smbfs" || f == "fuse.smbnetfs") return FsKind::Smb;
    return FsKind::Posix;
}

BackupTool::Destination BackupTool::inspect(const QString &destIn)
{
    Destination d;
    d.path = QDir::cleanPath(QFileInfo(destIn).absoluteFilePath());
    const QString home = QDir::cleanPath(homePath());

    d.containsHome = (d.path == home) || home.startsWith(d.path == "/" ? "/" : d.path + '/');
    d.insideHome   = d.path.startsWith(home + '/');

    const QStorageInfo dst(d.path), hm(home);
    d.fsType = QString::fromLatin1(dst.fileSystemType());
    // FUSE block mounts ("fuseblk") hide the real type — ntfs-3g and exfat-fuse
    // both look like that. Ask the block device.
    if (d.fsType == "fuseblk" || d.fsType.isEmpty()) {
        const QString src = CommandUtil::execProgramOutput(
            "findmnt", {"-n", "-o", "SOURCE", "--target", d.path}, 5000).trimmed();
        if (src.startsWith("/dev/")) {
            const QString t = CommandUtil::execProgramOutput("lsblk", {"-no", "FSTYPE", src}, 5000).trimmed();
            if (!t.isEmpty()) d.fsType = t;
        }
        if (d.fsType.isEmpty())
            d.fsType = CommandUtil::execProgramOutput(
                "findmnt", {"-n", "-o", "FSTYPE", "--target", d.path}, 5000).trimmed();
    }
    d.kind = classify(d.fsType);
    d.sameDisk = dst.isValid() && hm.isValid() && dst.device() == hm.device();

    QDir dir(d.path);
    d.hasMarker = QFileInfo::exists(dir.filePath(markerName()));
    const QStringList entries = dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                                              | QDir::NoDotAndDotDot);
    d.nonEmpty = !(entries.isEmpty() || (entries.size() == 1 && entries.first() == markerName()));
    return d;
}

QStringList BackupTool::rsyncArgs(const Destination &dest,
                                  const QStringList &excludes,
                                  bool mirror)
{
    QStringList a;
    switch (dest.kind) {
    case FsKind::Posix:
        a << "-a";                                        // perms, owner, times, links, devices
        break;
    case FsKind::Ntfs:
        a << "-rlt" << "--modify-window=1";               // no Unix perms/owners on NTFS
        break;
    case FsKind::Fat32:
        a << "-rt" << "--modify-window=2" << "--max-size=4095M";  // 2 s times, <4 GiB files, no links
        break;
    case FsKind::ExFat:
    case FsKind::Smb:
        a << "-rt" << "--modify-window=2";                // no perms, no symlinks
        break;
    }
    // --info=progress2 gives an overall % the page can parse; --human-readable
    // for the summary; --delete for a true mirror.
    a << "--info=progress2" << "--human-readable";
    if (mirror) a << "--delete";
    // Never delete our own marker at the destination root.
    a << "--filter" << QString("P /%1").arg(markerName());
    for (const QString &e : excludes) a << "--exclude" << e;
    // A destination INSIDE home must not be copied into itself — otherwise
    // every run nests another full copy (Backups/Backups/Backups/…).
    if (dest.insideHome) {
        QString rel = dest.path.mid(QDir::cleanPath(homePath()).size());   // "/Backups"
        // rsync treats * ? [ as wildcards and then honours backslash escapes.
        rel.replace('\\', "\\\\").replace('*', "\\*").replace('?', "\\?").replace('[', "\\[");
        a << "--exclude" << rel + '/';
    }
    // Trailing slash on the source copies its *contents* into dest.
    a << homePath() + "/" << dest.path + "/";
    return a;
}

QStringList BackupTool::restoreArgs(const QString &backupDir,
                                    const QStringList &excludes)
{
    // Restore is NOT a mirror (never --delete into the live home).
    QStringList a = {"-a", "--info=progress2", "--human-readable"};
    a << "--exclude" << "/" + markerName();
    for (const QString &e : excludes) a << "--exclude" << e;
    a << backupDir + "/" << homePath() + "/";
    return a;
}

void BackupTool::writeMarker(const QString &dest)
{
    QFile f(QDir(dest).filePath(markerName()));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QString("GT-STACER home backup of %1\nlast run: %2\n")
                    .arg(homePath(), QDateTime::currentDateTime().toString(Qt::ISODate)).toUtf8());
}
