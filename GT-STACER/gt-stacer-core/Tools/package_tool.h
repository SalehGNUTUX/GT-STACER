#pragma once
#include <QString>
#include <QVector>
#include <QStringList>

// ─── مدراء الحزم المدعومون ────────────────────────────────────────────────
enum class PkgMgr {
    // System — Debian family
    APT,        // Debian, Ubuntu, Mint, Kali, Pop!_OS, elementaryOS, Zorin
    // System — Red Hat / RPM family
    DNF,        // Fedora 22+, RHEL 8+, CentOS Stream 8+, AlmaLinux, Rocky
    DNF5,       // Fedora 38+
    YUM,        // RHEL 7, CentOS 7, Amazon Linux 2
    TDNF,       // VMware Photon OS
    // System — Arch family
    Pacman,     // Arch Linux, Manjaro, EndeavourOS, Garuda, BlackArch, Artix
    Yay,        // AUR helper (Arch)
    Paru,       // AUR helper (Arch)
    // System — openSUSE / SUSE family
    Zypper,     // openSUSE Leap/Tumbleweed, SLES, SLED
    // System — Other Linux
    XBPS,       // Void Linux
    APK,        // Alpine Linux, Chimera Linux, postmarketOS
    Portage,    // Gentoo Linux, Funtoo, Calculate Linux
    Nix,        // NixOS, Nix package manager (any distro)
    RpmOstree,  // Fedora Silverblue, Kinoite, Endless OS
    Pkgtool,    // Slackware Linux
    Slackpkg,   // Slackware Linux (network)
    Slapt,      // Slackware (slapt-get)
    Opkg,       // OpenWrt, Yocto, OpenEmbedded
    Ipkg,       // embedded Linux (old)
    Tazpkg,     // SliTaz Linux
    Equo,       // Sabayon / Entropy Linux
    Eopkg,      // Solus Linux
    Swupd,      // Clear Linux (Intel)
    Guix,       // GNU Guix System
    Apx,        // Vanilla OS (Apx)
    Pisi,       // Pardus Linux
    // Universal / Cross-distro
    Flatpak,    // All distributions
    Snap,       // Ubuntu, others with snapd
    AppImage,   // All distributions (no manager, tracked via ~/.local/share/applications)
    Brew,       // Homebrew on Linux
    // Conda / Data science
    Conda,      // Anaconda / Miniconda
    Mamba,      // Mamba (fast conda)
    // Language / user-level
    Pip3,       // Python pip
    Cargo,      // Rust
    Npm,        // Node.js
    // Manually / externally installed (install scripts, tarballs, AppImages) —
    // not tracked by any package manager. Detected by scanning .desktop launchers
    // whose Exec lives under /opt, /usr/local, ~/.local, ~, or is an AppImage.
    Manual,
    // Unknown / Fallback
    Unknown
};

// ─── معلومات الحزمة ───────────────────────────────────────────────────────
struct PackageInfo {
    QString name;
    QString version;
    QString description;
    QString size;
    QString arch;
    PkgMgr  manager = PkgMgr::Unknown;

    QString managerName() const;
    QString managerLabel() const;  // Arabic label
};

// ─── PackageTool API ──────────────────────────────────────────────────────
class PackageTool {
public:
    // ── الكشف ──
    static QVector<PkgMgr> availableManagers();
    static bool             has(PkgMgr mgr);
    static PkgMgr           primaryManager();        // المدير الرئيسي للتوزيعة
    static QString          managerName(PkgMgr mgr);

    // ── جلب الحزم ──
    static QVector<PackageInfo> packages(PkgMgr mgr);
    static QVector<PackageInfo> aptPackages();
    static QVector<PackageInfo> rpmPackages(PkgMgr mgr);   // dnf/yum/zypper (rpm-based)
    static QVector<PackageInfo> pacmanPackages();
    static QVector<PackageInfo> xbpsPackages();
    static QVector<PackageInfo> apkPackages();
    static QVector<PackageInfo> portagePackages();
    static QVector<PackageInfo> nixPackages();
    static QVector<PackageInfo> eopkgPackages();
    static QVector<PackageInfo> equoPackages();
    static QVector<PackageInfo> swupdPackages();
    static QVector<PackageInfo> flatpakPackages();
    static QVector<PackageInfo> snapPackages();
    static QVector<PackageInfo> brewPackages();
    // Externally / manually installed apps (install scripts, /opt, AppImages) that
    // no package manager knows about — e.g. Megacubo via `wget | bash`.
    static QVector<PackageInfo> manualPackages();
    static QVector<PackageInfo> allPackages();

    // ── التثبيت والترقية ──
    // Install a package by name from the given manager. Same security model as
    // remove(): the name is validated with isSafeIdentifier() and passed as a
    // separate argv element (no shell), root ops go through pkexec.
    static bool install(const QString &name, PkgMgr mgr);
    // Upgrade a single named package (APT: install --only-upgrade, etc.).
    static bool upgrade(const QString &name, PkgMgr mgr);
    // Upgrade everything the manager has updates for.
    static bool upgradeAll(PkgMgr mgr);
    // Packages that have a newer version available (name/version = the NEW one).
    static QVector<PackageInfo> upgradable(PkgMgr mgr);

    // ── Command builders ──
    // Return the exact argv ({program, arg…}, pkexec-prepended when root is
    // needed) for an operation, WITHOUT running it — empty on an invalid name or
    // unsupported manager. Exposed so the UI can run the command with live output
    // and a cancel button (see CommandLogDialog). remove of a Manual app has no
    // single command, so removeCommand() returns empty for PkgMgr::Manual.
    static QStringList installCommand(const QString &name, PkgMgr mgr);
    static QStringList upgradeCommand(const QString &name, PkgMgr mgr);
    static QStringList upgradeAllCommand(PkgMgr mgr);
    static QStringList removeCommand(const QString &name, PkgMgr mgr);

    // ── الإزالة ──
    static bool remove(const QString &name, PkgMgr mgr);
    // Removes a manually-installed app end-to-end: its .desktop launcher, its
    // payload under /opt (or the AppImage/binary), and per-user leftovers in
    // ~/.config, ~/.cache, ~/.local/{share,state}. Matches `name` against a
    // fresh scan, so no shell-unsafe identifier ever reaches a command.
    static bool removeManual(const QString &name);

    // ── الصيانة ──
    static bool cleanCache(PkgMgr mgr = PkgMgr::Unknown); // Unknown = primary
    static bool update(PkgMgr mgr = PkgMgr::Unknown);

    // Filesystem path of the manager's downloaded-packages cache, e.g.
    //   APT      → /var/cache/apt/archives
    //   DNF      → /var/cache/dnf
    //   Pacman   → /var/cache/pacman/pkg
    // Returns an empty string when there is no on-disk cache.
    static QString cacheDir(PkgMgr mgr);

    // Lightweight per-app description for Flatpak / Snap so the cleaner
    // dialog can render a checkbox list without paying for a full
    // PackageInfo. `appId` is what `remove()` must be called with.
    struct UniversalApp {
        QString  name;          // human label
        QString  appId;         // unique id (Flatpak: org.foo.Bar; Snap: foo)
        QString  version;
        qint64   sizeBytes = 0; // 0 when the manager doesn't report a size
        PkgMgr   manager   = PkgMgr::Unknown;
    };
    static QVector<UniversalApp> flatpakApps();
    static QVector<UniversalApp> snapApps();

    // ── البحث ──
    static QVector<PackageInfo> search(const QString &query, PkgMgr mgr);

private:
    static QVector<PkgMgr> s_available;
    static bool             s_detected;
    static void             detectManagers();
};
