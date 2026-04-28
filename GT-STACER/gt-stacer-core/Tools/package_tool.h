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
    static QVector<PackageInfo> allPackages();

    // ── الإزالة ──
    static bool remove(const QString &name, PkgMgr mgr);

    // ── الصيانة ──
    static bool cleanCache(PkgMgr mgr = PkgMgr::Unknown); // Unknown = primary
    static bool update(PkgMgr mgr = PkgMgr::Unknown);

    // ── البحث ──
    static QVector<PackageInfo> search(const QString &query, PkgMgr mgr);

private:
    static QVector<PkgMgr> s_available;
    static bool             s_detected;
    static void             detectManagers();
};
