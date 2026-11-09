#pragma once
#include <QString>
#include <QStringList>

// rsync-based backup of the user's home directory to a chosen destination.
// Backing up your OWN home to a user-writable location needs **no root**, so
// there is no pkexec here. This class builds the argv and checks the
// destination; the page runs `rsync` itself via a QProcess so it can show live
// progress (rsync --info=progress2) and offer a Stop button.
//
// 26.12 — the destination's FILESYSTEM decides the rsync options:
//   • POSIX (ext4, btrfs, xfs, zfs, f2fs, nfs, sshfs …): full archive (-a).
//   • NTFS (ntfs3 / ntfs-3g): no Unix permissions/owners → -rlt, 1 s mtime window.
//   • FAT32 / exFAT / SMB-CIFS: also no symlinks → -rt, 2 s mtime window
//     (FAT stores times in 2-second steps; without it every run recopies
//     everything); FAT32 additionally can't hold files ≥ 4 GiB → skipped.
// Using -a on those filesystems made every run end in "exit 23" with thousands
// of chmod/symlink errors and, on FAT, recopy the whole home each time.
class BackupTool {
public:
    enum class FsKind { Posix, Ntfs, Fat32, ExFat, Smb };

    struct Destination {
        QString path;              // cleaned absolute path
        QString fsType;            // as reported, e.g. "ext4", "vfat", "ntfs3"
        FsKind  kind = FsKind::Posix;
        bool    insideHome  = false;   // dest is under $HOME (it gets excluded)
        bool    sameDisk    = false;   // same filesystem as $HOME — not a real backup
        bool    containsHome = false;  // dest IS home or an ancestor of it → refused
        bool    hasMarker   = false;   // a previous GT-STACER backup lives here
        bool    nonEmpty    = false;   // has entries (besides our marker)
    };

    static QString     homePath();
    // Caches, trash and other churny/regenerable paths — skipped by default.
    static QStringList defaultExcludes();
    // The marker file written at the root of a destination after a backup.
    static QString     markerName();

    // Inspect a destination folder (it must exist). Blocking but quick.
    static Destination inspect(const QString &dest);
    static FsKind      classify(const QString &fsType);

    // Full argv for a backup of home -> dest (trailing slash on src so the
    // contents, not the "home" dir itself, land in dest). `mirror` adds --delete.
    static QStringList rsyncArgs(const Destination &dest,
                                 const QStringList &excludes,
                                 bool mirror = true);
    // Argv to restore FROM a previous backup dir back INTO home.
    static QStringList restoreArgs(const QString &backupDir,
                                   const QStringList &excludes);

    // Write the marker after a successful run.
    static void        writeMarker(const QString &dest);
};
