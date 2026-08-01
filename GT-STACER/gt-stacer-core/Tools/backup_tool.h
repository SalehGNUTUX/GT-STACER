#pragma once
#include <QString>
#include <QStringList>

// rsync-based backup of the user's home directory to a chosen destination.
// Backing up your OWN home to a user-writable location needs **no root**, so
// there is no pkexec here. This class just builds the argv and the sensible
// default excludes; the page runs `rsync` itself via a QProcess so it can show
// live progress (rsync --info=progress2) and offer a Cancel button.
class BackupTool {
public:
    static QString     homePath();
    // Caches, trash and other churny/regenerable paths — skipped by default.
    static QStringList defaultExcludes();

    // Full argv for a mirror backup of home -> dest (trailing slash on src so the
    // contents, not the "home" dir itself, land in dest). `mirror` adds --delete.
    static QStringList rsyncArgs(const QString &dest,
                                 const QStringList &excludes,
                                 bool mirror = true);
    // Argv to restore FROM a previous backup dir back INTO home.
    static QStringList restoreArgs(const QString &backupDir,
                                   const QStringList &excludes);
};
