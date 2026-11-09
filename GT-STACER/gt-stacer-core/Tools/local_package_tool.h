#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "package_tool.h"

// Install a package FILE the user picked (26.12): .deb, .rpm, Arch .pkg.tar.*,
// Alpine .apk, Solus .eopkg, Flatpak bundle/ref, Snap, AppImage.
//
// The tool detects which of these the running system can install (from the
// detected package managers), reads the file's metadata and — for traditional
// packages — its dependencies, and checks each one against the installed
// system and the configured repositories, so the UI can show the user what
// will happen before anything runs.
//
// Security model (same as PackageTool): every command is argv (no shell); the
// file is passed as an absolute path; dependency names come from an UNTRUSTED
// file, so a name is only ever passed to a package manager after
// isSafeIdentifier() (which also rejects a leading '-', i.e. option injection).

enum class LocalPkgKind {
    Unknown, Deb, Rpm, Pacman, Apk, Eopkg, FlatpakBundle, FlatpakRef, Snap, AppImage
};

struct LocalDependency {
    enum class State {
        Satisfied,      // already installed (and the version constraint holds)
        Installable,    // missing, but available from the configured repositories
        Unavailable,    // missing and NOT found in the repositories
        Missing         // missing; availability couldn't be determined
    };
    QString declared;     // as written in the package, e.g. "libc6 (>= 2.34) | libc6-udeb"
    QString name;         // what we would pass to the manager (first resolvable alternative)
    QString note;         // "recommended", "suggested", or the optional-dep description
    bool    required = true;
    State   state = State::Missing;
};

struct LocalPackageInfo {
    QString      path;               // absolute (staged under ~/.cache inside Flatpak)
    LocalPkgKind kind = LocalPkgKind::Unknown;
    QString      name, version, arch, summary, description;
    QString      maintainer, homepage, license;
    qint64       fileSize = 0;
    qint64       installedSize = -1; // bytes, -1 = unknown
    QString      installedVersion;   // already-installed version of this package, if any

    bool         installable = false;  // this system can install it
    QString      problem;             // why not (shown instead of the Install button)
    QStringList  warnings;            // non-blocking notes

    PkgMgr       installer = PkgMgr::Unknown;
    bool         depsChecked = false; // dependency info was available for this kind
    QVector<LocalDependency> deps;
};

class LocalPackageTool {
public:
    static LocalPkgKind kindOf(const QString &path);
    static QString      kindLabel(LocalPkgKind kind);       // English, e.g. "Debian package (.deb)"

    // Kinds this system can install, from the detected package managers.
    static QVector<LocalPkgKind> supportedKinds();
    // Glob patterns for a file dialog, e.g. {"*.deb", "*.flatpak", "*.AppImage"}.
    static QStringList patternsFor(LocalPkgKind kind);
    // "Debian GNU/Linux 13 (trixie)" — the host's, also from inside Flatpak.
    static QString      distroName();

    // Read metadata + check dependencies. BLOCKING (runs several commands) —
    // call it off the UI thread.
    static LocalPackageInfo inspect(const QString &path);

    // The install steps for `info`, with the user's chosen optional dependencies.
    // Each step is a full argv (pkexec-prefixed when root is needed). Empty if the
    // kind can't be installed via a command (AppImage → AppImageTool::integrate).
    static QVector<QStringList> installSteps(const LocalPackageInfo &info,
                                             const QStringList &optionalDeps);
};
