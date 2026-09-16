#include "package_tool.h"
#include "../Utils/command_util.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

// ─── Static members ───────────────────────────────────────────────────────
QVector<PkgMgr> PackageTool::s_available;
bool            PackageTool::s_detected = false;

// ─── Name helpers ─────────────────────────────────────────────────────────
QString PackageTool::managerName(PkgMgr mgr)
{
    switch (mgr) {
    case PkgMgr::APT:       return "APT";
    case PkgMgr::DNF:       return "DNF";
    case PkgMgr::DNF5:      return "DNF5";
    case PkgMgr::YUM:       return "YUM";
    case PkgMgr::TDNF:      return "TDNF";
    case PkgMgr::Pacman:    return "Pacman";
    case PkgMgr::Yay:       return "Yay (AUR)";
    case PkgMgr::Paru:      return "Paru (AUR)";
    case PkgMgr::Zypper:    return "Zypper";
    case PkgMgr::XBPS:      return "XBPS";
    case PkgMgr::APK:       return "APK";
    case PkgMgr::Portage:   return "Portage";
    case PkgMgr::Nix:       return "Nix";
    case PkgMgr::RpmOstree: return "rpm-ostree";
    case PkgMgr::Pkgtool:   return "Pkgtool";
    case PkgMgr::Slackpkg:  return "Slackpkg";
    case PkgMgr::Slapt:     return "slapt-get";
    case PkgMgr::Opkg:      return "Opkg";
    case PkgMgr::Tazpkg:    return "Tazpkg";
    case PkgMgr::Equo:      return "Equo";
    case PkgMgr::Eopkg:     return "Eopkg";
    case PkgMgr::Swupd:     return "swupd";
    case PkgMgr::Guix:      return "Guix";
    case PkgMgr::Apx:       return "Apx";
    case PkgMgr::Pisi:      return "PISI";
    case PkgMgr::Flatpak:   return "Flatpak";
    case PkgMgr::Snap:      return "Snap";
    case PkgMgr::AppImage:  return "AppImage";
    case PkgMgr::Brew:      return "Homebrew";
    case PkgMgr::Conda:     return "Conda";
    case PkgMgr::Mamba:     return "Mamba";
    case PkgMgr::Pip3:      return "pip3";
    case PkgMgr::Cargo:     return "Cargo";
    case PkgMgr::Npm:       return "npm";
    case PkgMgr::Manual:    return "External";
    default:                return "Unknown";
    }
}

QString PackageInfo::managerName() const  { return PackageTool::managerName(manager); }
QString PackageInfo::managerLabel() const { return PackageTool::managerName(manager); }

// ─── Detection ────────────────────────────────────────────────────────────
void PackageTool::detectManagers()
{
    if (s_detected) return;
    s_detected = true;
    s_available.clear();

    struct Probe { PkgMgr mgr; QString cmd; };
    static const QVector<Probe> probes = {
        // System managers
        {PkgMgr::APT,       "apt-get"},
        {PkgMgr::DNF5,      "dnf5"},
        {PkgMgr::DNF,       "dnf"},
        {PkgMgr::YUM,       "yum"},
        {PkgMgr::TDNF,      "tdnf"},
        {PkgMgr::Pacman,    "pacman"},
        {PkgMgr::Yay,       "yay"},
        {PkgMgr::Paru,      "paru"},
        {PkgMgr::Zypper,    "zypper"},
        {PkgMgr::XBPS,      "xbps-query"},
        {PkgMgr::APK,       "apk"},
        {PkgMgr::Portage,   "emerge"},
        {PkgMgr::Nix,       "nix-env"},
        {PkgMgr::RpmOstree, "rpm-ostree"},
        {PkgMgr::Pkgtool,   "installpkg"},
        {PkgMgr::Slackpkg,  "slackpkg"},
        {PkgMgr::Slapt,     "slapt-get"},
        {PkgMgr::Opkg,      "opkg"},
        {PkgMgr::Tazpkg,    "tazpkg"},
        {PkgMgr::Equo,      "equo"},
        {PkgMgr::Eopkg,     "eopkg"},
        {PkgMgr::Swupd,     "swupd"},
        {PkgMgr::Guix,      "guix"},
        {PkgMgr::Apx,       "apx"},
        {PkgMgr::Pisi,      "pisi"},
        // Universal
        {PkgMgr::Flatpak,   "flatpak"},
        {PkgMgr::Snap,      "snap"},
        {PkgMgr::Brew,      "brew"},
        {PkgMgr::Conda,     "conda"},
        {PkgMgr::Mamba,     "mamba"},
        // Language
        {PkgMgr::Pip3,      "pip3"},
        {PkgMgr::Cargo,     "cargo"},
        {PkgMgr::Npm,       "npm"},
    };

    for (const auto &p : probes) {
        if (CommandUtil::commandExists(p.cmd))
            s_available << p.mgr;
    }

    // AppImage: check ~/.local/share/applications for *.desktop with Exec=*.AppImage
    QString appimageDir = QDir::homePath() + "/.local/share/applications";
    if (QDir(appimageDir).exists()) {
        auto files = QDir(appimageDir).entryList({"*.desktop"}, QDir::Files);
        for (const auto &f : files) {
            QString content = FileUtil::readFile(appimageDir + "/" + f);
            if (content.contains(".AppImage")) {
                s_available << PkgMgr::AppImage;
                break;
            }
        }
    }
}

bool PackageTool::has(PkgMgr mgr)
{
    detectManagers();
    return s_available.contains(mgr);
}

QVector<PkgMgr> PackageTool::availableManagers()
{
    detectManagers();
    return s_available;
}

PkgMgr PackageTool::primaryManager()
{
    detectManagers();
    // Priority order: system managers first
    for (PkgMgr m : {PkgMgr::APT, PkgMgr::DNF5, PkgMgr::DNF, PkgMgr::Pacman,
                     PkgMgr::Zypper, PkgMgr::YUM, PkgMgr::XBPS, PkgMgr::APK,
                     PkgMgr::Portage, PkgMgr::Eopkg, PkgMgr::Equo, PkgMgr::Swupd,
                     PkgMgr::Nix, PkgMgr::RpmOstree, PkgMgr::Pkgtool, PkgMgr::Tazpkg,
                     PkgMgr::Opkg, PkgMgr::Guix, PkgMgr::Apx}) {
        if (s_available.contains(m)) return m;
    }
    return PkgMgr::Unknown;
}

// ─── Listing ──────────────────────────────────────────────────────────────
QVector<PackageInfo> PackageTool::aptPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines(
        "dpkg-query -W -f='${Package}\\t${Version}\\t${Installed-Size}\\t${binary:Summary}\\n' 2>/dev/null");
    for (const auto &line : lines) {
        auto p = line.split('\t');
        if (p.size() < 2) continue;
        PackageInfo pkg;
        pkg.manager     = PkgMgr::APT;
        pkg.name        = p[0].trimmed();
        pkg.version     = p[1].trimmed();
        pkg.size        = p.size() > 2 ? p[2].trimmed() + " KB" : "";
        pkg.description = p.size() > 3 ? p[3].trimmed() : "";
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::rpmPackages(PkgMgr mgr)
{
    QVector<PackageInfo> result;
    // rpm -qa works for DNF, YUM, Zypper, rpm-ostree
    auto lines = CommandUtil::execLines(
        "rpm -qa --queryformat '%{NAME}\\t%{VERSION}-%{RELEASE}\\t%{SIZE}\\t%{SUMMARY}\\n' 2>/dev/null");
    for (const auto &line : lines) {
        auto p = line.split('\t');
        if (p.size() < 2) continue;
        PackageInfo pkg;
        pkg.manager     = mgr;
        pkg.name        = p[0].trimmed();
        pkg.version     = p[1].trimmed();
        pkg.size        = p.size() > 2 ? p[2].trimmed() : "";
        pkg.description = p.size() > 3 ? p[3].trimmed() : "";
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::pacmanPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("pacman -Q 2>/dev/null");
    for (const auto &line : lines) {
        auto p = line.split(' ');
        if (p.size() < 2) continue;
        PackageInfo pkg;
        pkg.manager = PkgMgr::Pacman;
        pkg.name    = p[0].trimmed();
        pkg.version = p[1].trimmed();
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::xbpsPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("xbps-query -l 2>/dev/null");
    for (const auto &line : lines) {
        // format: ii name-version description
        auto p = line.simplified().split(' ');
        if (p.size() < 2) continue;
        PackageInfo pkg;
        pkg.manager     = PkgMgr::XBPS;
        QString nameVer = p[1];
        int dash = nameVer.lastIndexOf('-');
        if (dash > 0) {
            pkg.name    = nameVer.left(dash);
            pkg.version = nameVer.mid(dash + 1);
        } else {
            pkg.name = nameVer;
        }
        pkg.description = p.mid(2).join(' ');
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::apkPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("apk list --installed 2>/dev/null");
    for (const auto &line : lines) {
        // format: name-version-arch {repo} [state]
        QString name = line.split(' ').first();
        PackageInfo pkg;
        pkg.manager = PkgMgr::APK;
        pkg.name    = name;
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::portagePackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("qlist -I 2>/dev/null");
    for (const auto &line : lines) {
        PackageInfo pkg;
        pkg.manager = PkgMgr::Portage;
        // format: category/name-version
        QString atom = line.trimmed();
        int slash = atom.lastIndexOf('/');
        pkg.name = slash >= 0 ? atom.mid(slash + 1) : atom;
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::nixPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("nix-env -q 2>/dev/null");
    for (const auto &line : lines) {
        PackageInfo pkg;
        pkg.manager = PkgMgr::Nix;
        pkg.name    = line.trimmed();
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::eopkgPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("eopkg list-installed 2>/dev/null");
    for (const auto &line : lines) {
        auto p = line.split(' ');
        if (p.isEmpty()) continue;
        PackageInfo pkg;
        pkg.manager = PkgMgr::Eopkg;
        pkg.name    = p[0].trimmed();
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::equoPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("equo query installed 2>/dev/null");
    for (const auto &line : lines) {
        auto p = line.trimmed().split(' ');
        if (p.isEmpty()) continue;
        PackageInfo pkg;
        pkg.manager = PkgMgr::Equo;
        pkg.name    = p[0];
        if (p.size() > 1) pkg.version = p[1];
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::swupdPackages()
{
    QVector<PackageInfo> result;
    auto lines = CommandUtil::execLines("swupd bundle-list --status 2>/dev/null");
    for (const auto &line : lines) {
        if (line.trimmed().isEmpty() || line.startsWith("Bundles")) continue;
        PackageInfo pkg;
        pkg.manager = PkgMgr::Swupd;
        pkg.name    = line.trimmed().split(' ').first();
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::flatpakPackages()
{
    QVector<PackageInfo> result;
    if (!has(PkgMgr::Flatpak)) return result;
    auto lines = CommandUtil::execLines(
        "flatpak list --app --columns=name,application,version,size 2>/dev/null");
    for (const auto &line : lines) {
        auto p = line.split('\t');
        if (p.size() < 2) continue;
        PackageInfo pkg;
        pkg.manager     = PkgMgr::Flatpak;
        pkg.description = p[0].trimmed();
        pkg.name        = p[1].trimmed();
        pkg.version     = p.size() > 2 ? p[2].trimmed() : "";
        pkg.size        = p.size() > 3 ? p[3].trimmed() : "";
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::snapPackages()
{
    QVector<PackageInfo> result;
    if (!has(PkgMgr::Snap)) return result;
    auto lines = CommandUtil::execLines("snap list 2>/dev/null");
    bool header = true;
    for (const auto &line : lines) {
        if (header) { header = false; continue; }
        auto p = line.simplified().split(' ');
        if (p.size() < 2) continue;
        PackageInfo pkg;
        pkg.manager = PkgMgr::Snap;
        pkg.name    = p[0];
        pkg.version = p[1];
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::brewPackages()
{
    QVector<PackageInfo> result;
    if (!has(PkgMgr::Brew)) return result;
    auto lines = CommandUtil::execLines("brew list --versions 2>/dev/null");
    for (const auto &line : lines) {
        auto p = line.simplified().split(' ');
        if (p.isEmpty()) continue;
        PackageInfo pkg;
        pkg.manager = PkgMgr::Brew;
        pkg.name    = p[0];
        pkg.version = p.size() > 1 ? p[1] : "";
        result << pkg;
    }
    return result;
}

QVector<PackageInfo> PackageTool::packages(PkgMgr mgr)
{
    switch (mgr) {
    case PkgMgr::APT:       return aptPackages();
    case PkgMgr::DNF:
    case PkgMgr::DNF5:
    case PkgMgr::YUM:
    case PkgMgr::TDNF:
    case PkgMgr::Zypper:
    case PkgMgr::RpmOstree: return rpmPackages(mgr);
    case PkgMgr::Pacman:
    case PkgMgr::Yay:
    case PkgMgr::Paru:      return pacmanPackages();
    case PkgMgr::XBPS:      return xbpsPackages();
    case PkgMgr::APK:       return apkPackages();
    case PkgMgr::Portage:   return portagePackages();
    case PkgMgr::Nix:       return nixPackages();
    case PkgMgr::Eopkg:     return eopkgPackages();
    case PkgMgr::Equo:      return equoPackages();
    case PkgMgr::Swupd:     return swupdPackages();
    case PkgMgr::Flatpak:   return flatpakPackages();
    case PkgMgr::Snap:      return snapPackages();
    case PkgMgr::Brew:      return brewPackages();
    default:                return {};
    }
}

QVector<PackageInfo> PackageTool::allPackages()
{
    detectManagers();
    QVector<PackageInfo> result;
    // Show each manager only once (no duplicates for rpm-family)
    bool rpmDone = false;
    for (PkgMgr mgr : s_available) {
        bool isRpm = (mgr == PkgMgr::DNF || mgr == PkgMgr::DNF5 ||
                      mgr == PkgMgr::YUM || mgr == PkgMgr::TDNF ||
                      mgr == PkgMgr::Zypper || mgr == PkgMgr::RpmOstree);
        if (isRpm) {
            if (rpmDone) continue;
            rpmDone = true;
        }
        result += packages(mgr);
    }
    // Always append externally/manually installed apps — these belong to no
    // manager in s_available but are exactly what users can't otherwise remove.
    result += manualPackages();
    return result;
}

// ─── Removal ─────────────────────────────────────────────────────────────
// Each entry builds an argv list (program + args) and is passed to QProcess
// without going through a shell, so package names with metacharacters cannot
// be turned into command injection. We additionally validate the name.
bool PackageTool::remove(const QString &name, PkgMgr mgr)
{
    // Manual apps are matched against a fresh filesystem scan (not shelled out
    // by name), so display names with spaces/uppercase are fine here.
    if (mgr == PkgMgr::Manual) return removeManual(name);

    if (!CommandUtil::isSafeIdentifier(name)) return false;

    QString prog;
    QStringList args;
    bool needsRoot = true;

    switch (mgr) {
    case PkgMgr::APT:       prog = "apt-get";       args = {"remove", "-y", name}; break;
    case PkgMgr::DNF:
    case PkgMgr::DNF5:      prog = "dnf";           args = {"remove", "-y", name}; break;
    case PkgMgr::YUM:       prog = "yum";           args = {"remove", "-y", name}; break;
    case PkgMgr::TDNF:      prog = "tdnf";          args = {"remove", "-y", name}; break;
    case PkgMgr::Pacman:
    case PkgMgr::Yay:
    case PkgMgr::Paru:      prog = "pacman";        args = {"-R", "--noconfirm", name}; break;
    case PkgMgr::Zypper:    prog = "zypper";        args = {"remove", "-y", name}; break;
    case PkgMgr::XBPS:      prog = "xbps-remove";   args = {"-R", name}; break;
    case PkgMgr::APK:       prog = "apk";           args = {"del", name}; break;
    case PkgMgr::Portage:   prog = "emerge";        args = {"--deselect", name}; break;
    case PkgMgr::Nix:       prog = "nix-env";       args = {"-e", name}; needsRoot = false; break;
    case PkgMgr::RpmOstree: prog = "rpm-ostree";    args = {"override", "remove", name}; break;
    case PkgMgr::Eopkg:     prog = "eopkg";         args = {"remove", name}; break;
    case PkgMgr::Equo:      prog = "equo";          args = {"remove", name}; break;
    case PkgMgr::Swupd:     prog = "swupd";         args = {"bundle-remove", name}; break;
    case PkgMgr::Guix:      prog = "guix";          args = {"remove", name}; needsRoot = false; break;
    case PkgMgr::Flatpak:   prog = "flatpak";       args = {"uninstall", "-y", name}; needsRoot = false; break;
    case PkgMgr::Snap:      prog = "snap";          args = {"remove", name}; break;
    case PkgMgr::Brew:      prog = "brew";          args = {"uninstall", name}; needsRoot = false; break;
    case PkgMgr::Conda:     prog = "conda";         args = {"remove", "-y", name}; needsRoot = false; break;
    case PkgMgr::Pip3:      prog = "pip3";          args = {"uninstall", "-y", name}; needsRoot = false; break;
    case PkgMgr::Cargo:     prog = "cargo";         args = {"uninstall", name}; needsRoot = false; break;
    case PkgMgr::Npm:       prog = "npm";           args = {"uninstall", "-g", name}; needsRoot = false; break;
    default:                return false;
    }

    if (needsRoot) {
        args.prepend(prog);
        prog = "pkexec";
    }
    return CommandUtil::execProgram(prog, args, 120000) == 0;
}

// ─── Install ──────────────────────────────────────────────────────────────
// Mirrors remove()'s security model exactly: the package name is validated with
// isSafeIdentifier() (so no shell metacharacter can reach a command) and passed
// as a distinct argv element via execProgram (no shell). Root managers go
// through pkexec; user-level managers (flatpak/brew/pip/…) do not.
bool PackageTool::install(const QString &name, PkgMgr mgr)
{
    if (!CommandUtil::isSafeIdentifier(name)) return false;

    QString prog;
    QStringList args;
    bool needsRoot = true;

    switch (mgr) {
    case PkgMgr::APT:       prog = "apt-get";       args = {"install", "-y", name}; break;
    case PkgMgr::DNF:
    case PkgMgr::DNF5:      prog = "dnf";           args = {"install", "-y", name}; break;
    case PkgMgr::YUM:       prog = "yum";           args = {"install", "-y", name}; break;
    case PkgMgr::TDNF:      prog = "tdnf";          args = {"install", "-y", name}; break;
    case PkgMgr::Pacman:    prog = "pacman";        args = {"-S", "--noconfirm", name}; break;
    case PkgMgr::Zypper:    prog = "zypper";        args = {"install", "-y", name}; break;
    case PkgMgr::XBPS:      prog = "xbps-install";  args = {"-y", name}; break;
    case PkgMgr::APK:       prog = "apk";           args = {"add", name}; break;
    case PkgMgr::Portage:   prog = "emerge";        args = {name}; break;
    case PkgMgr::Nix:       prog = "nix-env";       args = {"-iA", name}; needsRoot = false; break;
    case PkgMgr::RpmOstree: prog = "rpm-ostree";    args = {"install", name}; break;
    case PkgMgr::Eopkg:     prog = "eopkg";         args = {"install", "-y", name}; break;
    case PkgMgr::Equo:      prog = "equo";          args = {"install", name}; break;
    case PkgMgr::Swupd:     prog = "swupd";         args = {"bundle-add", name}; break;
    case PkgMgr::Guix:      prog = "guix";          args = {"install", name}; needsRoot = false; break;
    case PkgMgr::Flatpak:   prog = "flatpak";       args = {"install", "-y", name}; needsRoot = false; break;
    case PkgMgr::Snap:      prog = "snap";          args = {"install", name}; break;
    case PkgMgr::Brew:      prog = "brew";          args = {"install", name}; needsRoot = false; break;
    case PkgMgr::Conda:     prog = "conda";         args = {"install", "-y", name}; needsRoot = false; break;
    case PkgMgr::Pip3:      prog = "pip3";          args = {"install", name}; needsRoot = false; break;
    case PkgMgr::Cargo:     prog = "cargo";         args = {"install", name}; needsRoot = false; break;
    case PkgMgr::Npm:       prog = "npm";           args = {"install", "-g", name}; needsRoot = false; break;
    default:                return false;
    }

    if (needsRoot) { args.prepend(prog); prog = "pkexec"; }
    return CommandUtil::execProgram(prog, args, 600000) == 0;
}

// ─── Upgrade ──────────────────────────────────────────────────────────────
bool PackageTool::upgrade(const QString &name, PkgMgr mgr)
{
    if (!CommandUtil::isSafeIdentifier(name)) return false;

    QString prog;
    QStringList args;
    bool needsRoot = true;

    switch (mgr) {
    case PkgMgr::APT:       prog = "apt-get"; args = {"install", "--only-upgrade", "-y", name}; break;
    case PkgMgr::DNF:
    case PkgMgr::DNF5:      prog = "dnf";     args = {"upgrade", "-y", name}; break;
    case PkgMgr::YUM:       prog = "yum";     args = {"update", "-y", name}; break;
    case PkgMgr::Zypper:    prog = "zypper";  args = {"update", "-y", name}; break;
    case PkgMgr::Pacman:    prog = "pacman";  args = {"-S", "--noconfirm", name}; break;
    case PkgMgr::XBPS:      prog = "xbps-install"; args = {"-u", "-y", name}; break;
    case PkgMgr::APK:       prog = "apk";     args = {"upgrade", name}; break;
    case PkgMgr::Flatpak:   prog = "flatpak"; args = {"update", "-y", name}; needsRoot = false; break;
    case PkgMgr::Snap:      prog = "snap";    args = {"refresh", name}; break;
    case PkgMgr::Brew:      prog = "brew";    args = {"upgrade", name}; needsRoot = false; break;
    case PkgMgr::Pip3:      prog = "pip3";    args = {"install", "--upgrade", name}; needsRoot = false; break;
    default:                return false;
    }

    if (needsRoot) { args.prepend(prog); prog = "pkexec"; }
    return CommandUtil::execProgram(prog, args, 600000) == 0;
}

bool PackageTool::upgradeAll(PkgMgr mgr)
{
    QString prog;
    QStringList args;
    bool needsRoot = true;

    switch (mgr) {
    case PkgMgr::APT:       prog = "apt-get"; args = {"upgrade", "-y"}; break;
    case PkgMgr::DNF:
    case PkgMgr::DNF5:      prog = "dnf";     args = {"upgrade", "-y"}; break;
    case PkgMgr::YUM:       prog = "yum";     args = {"update", "-y"}; break;
    case PkgMgr::Zypper:    prog = "zypper";  args = {"update", "-y"}; break;
    case PkgMgr::Pacman:    prog = "pacman";  args = {"-Syu", "--noconfirm"}; break;
    case PkgMgr::XBPS:      prog = "xbps-install"; args = {"-Su", "-y"}; break;
    case PkgMgr::APK:       prog = "apk";     args = {"upgrade"}; break;
    case PkgMgr::Flatpak:   prog = "flatpak"; args = {"update", "-y"}; needsRoot = false; break;
    case PkgMgr::Snap:      prog = "snap";    args = {"refresh"}; break;
    case PkgMgr::Brew:      prog = "brew";    args = {"upgrade"}; needsRoot = false; break;
    default:                return false;
    }

    if (needsRoot) { args.prepend(prog); prog = "pkexec"; }
    return CommandUtil::execProgram(prog, args, 1800000) == 0;
}

// ─── Cache cleaning ───────────────────────────────────────────────────────
// Every branch uses execProgram (no shell), and falls through to false only
// when the manager genuinely doesn't expose a cache-clean operation
// (Flatpak/Snap manage their own LRU, language managers have no cache).
bool PackageTool::cleanCache(PkgMgr mgr)
{
    if (mgr == PkgMgr::Unknown) mgr = primaryManager();
    auto pk = [](const QStringList &args) {
        return CommandUtil::execProgram("pkexec", args, 300000) == 0;
    };
    auto user = [](const QString &prog, const QStringList &args) {
        return CommandUtil::execProgram(prog, args, 300000) == 0;
    };
    switch (mgr) {
    case PkgMgr::APT:       return pk({"apt-get", "clean"});
    case PkgMgr::DNF:
    case PkgMgr::DNF5:      return pk({"dnf", "clean", "all"});
    case PkgMgr::YUM:       return pk({"yum", "clean", "all"});
    case PkgMgr::TDNF:      return pk({"tdnf", "clean", "all"});
    case PkgMgr::Pacman:
    case PkgMgr::Yay:
    case PkgMgr::Paru:      return pk({"pacman", "-Sc", "--noconfirm"});
    case PkgMgr::Zypper:    return pk({"zypper", "clean", "--all"});
    case PkgMgr::APK:       return pk({"apk", "cache", "clean"});
    case PkgMgr::XBPS:      return pk({"xbps-remove", "-O"});  // -O = clean cache
    case PkgMgr::Portage:   return pk({"eclean-dist", "--deep"});
    case PkgMgr::Eopkg:     return pk({"eopkg", "delete-cache"});
    case PkgMgr::Equo:      return pk({"equo", "cleanup"});
    case PkgMgr::Swupd:     return pk({"swupd", "clean", "--all"});
    case PkgMgr::Nix:       return user("nix-collect-garbage", {});
    case PkgMgr::Flatpak:   return user("flatpak", {"uninstall", "--unused", "-y"});
    case PkgMgr::Snap:      return pk({"snap", "remove-disabled"});
    case PkgMgr::Brew:      return user("brew", {"cleanup"});
    default:                return false;
    }
}

// ─── Helpers for size strings reported by flatpak/snap ─────────────────
static qint64 parseHumanSize(const QString &s)
{
    // Inputs we accept: "3.4 MB", "12.0M", "1,2 GB", "?" (returns 0).
    QString t = s.trimmed();
    if (t.isEmpty() || t == "?" || t == "-") return 0;
    t.replace(',', '.');
    static const QRegularExpression rx(R"(([0-9]+(?:\.[0-9]+)?)\s*([KMGTP]i?B?|B)?)",
                                        QRegularExpression::CaseInsensitiveOption);
    auto m = rx.match(t);
    if (!m.hasMatch()) return 0;
    double value = m.captured(1).toDouble();
    QString unit = m.captured(2).toUpper();
    qint64 mult  = 1;
    if      (unit.startsWith('K')) mult = 1024;
    else if (unit.startsWith('M')) mult = 1024LL * 1024;
    else if (unit.startsWith('G')) mult = 1024LL * 1024 * 1024;
    else if (unit.startsWith('T')) mult = 1024LL * 1024 * 1024 * 1024;
    return static_cast<qint64>(value * mult);
}

QVector<PackageTool::UniversalApp> PackageTool::flatpakApps()
{
    QVector<UniversalApp> out;
    if (!has(PkgMgr::Flatpak)) return out;
    // Tab-separated for unambiguous parsing — Flatpak app IDs never contain tabs.
    auto lines = CommandUtil::execLines(
        "flatpak list --app --columns=name,application,version,size 2>/dev/null");
    for (const auto &raw : lines) {
        auto parts = raw.split('\t');
        if (parts.size() < 2) continue;
        UniversalApp a;
        a.manager   = PkgMgr::Flatpak;
        a.name      = parts.value(0).trimmed();
        a.appId     = parts.value(1).trimmed();
        a.version   = parts.size() > 2 ? parts.value(2).trimmed() : QString();
        a.sizeBytes = parseHumanSize(parts.value(3));
        if (a.name.isEmpty()) a.name = a.appId;
        out.append(a);
    }
    return out;
}

QVector<PackageTool::UniversalApp> PackageTool::snapApps()
{
    QVector<UniversalApp> out;
    if (!has(PkgMgr::Snap)) return out;
    // `snap list` doesn't report sizes inline; we fall back to /var/lib/snapd/snaps
    // sizes when available. Fields: Name Version Rev Tracking Publisher Notes.
    auto lines = CommandUtil::execLines("snap list 2>/dev/null");
    bool header = true;
    for (const auto &line : lines) {
        if (header) { header = false; continue; }
        auto parts = line.simplified().split(' ');
        if (parts.size() < 2) continue;
        UniversalApp a;
        a.manager = PkgMgr::Snap;
        a.name    = parts[0];
        a.appId   = parts[0];
        a.version = parts.value(1);
        // Best-effort size from the on-disk snap blob.
        QFileInfo fi(QString("/var/lib/snapd/snaps/%1_%2.snap")
                       .arg(parts[0], parts.value(2))); // (name)_(rev).snap
        if (fi.exists()) a.sizeBytes = fi.size();
        out.append(a);
    }
    return out;
}

QString PackageTool::cacheDir(PkgMgr mgr)
{
    // For managers with a known on-disk cache directory, return its path so
    // the System Cleaner can scan & enumerate the files. Empty string ⇒ no
    // user-visible cache (Flatpak/Snap manage their own; language managers
    // cache under $HOME and don't need root).
    switch (mgr) {
    case PkgMgr::APT:       return "/var/cache/apt/archives";
    case PkgMgr::DNF:
    case PkgMgr::DNF5:      return "/var/cache/dnf";
    case PkgMgr::YUM:       return "/var/cache/yum";
    case PkgMgr::Zypper:    return "/var/cache/zypp/packages";
    case PkgMgr::Pacman:
    case PkgMgr::Yay:
    case PkgMgr::Paru:      return "/var/cache/pacman/pkg";
    case PkgMgr::XBPS:      return "/var/cache/xbps";
    case PkgMgr::APK:       return "/var/cache/apk";
    case PkgMgr::Portage:   return "/var/cache/distfiles";
    case PkgMgr::Eopkg:     return "/var/cache/eopkg/packages";
    case PkgMgr::Equo:      return "/var/lib/entropy/client/packages";
    case PkgMgr::Swupd:     return "/var/lib/swupd/cache";
    default:                return {};
    }
}

bool PackageTool::update(PkgMgr mgr)
{
    if (mgr == PkgMgr::Unknown) mgr = primaryManager();
    switch (mgr) {
    case PkgMgr::APT:    return CommandUtil::execStatus("pkexec apt-get update") == 0;
    case PkgMgr::DNF:
    case PkgMgr::DNF5:   return CommandUtil::execStatus("pkexec dnf check-update") == 0;
    case PkgMgr::Pacman: return CommandUtil::execStatus("pkexec pacman -Sy") == 0;
    case PkgMgr::Zypper: return CommandUtil::execStatus("pkexec zypper refresh") == 0;
    case PkgMgr::APK:    return CommandUtil::execStatus("pkexec apk update") == 0;
    default:             return false;
    }
}

// ─── Search ───────────────────────────────────────────────────────────────
QVector<PackageInfo> PackageTool::search(const QString &query, PkgMgr mgr)
{
    QVector<PackageInfo> result;

    // SECURITY: the query used to be concatenated into a `/bin/sh -c` string —
    // a command-injection vector (e.g. "x; rm -rf ~"). It is now (1) validated to
    // a conservative character set and (2) passed as separate argv elements via
    // execProgramOutput, which never invokes a shell. Both together close the hole.
    const QString q = query.trimmed();
    static const QRegularExpression kSafe("^[A-Za-z0-9 ._+:@-]{1,64}$");
    if (q.isEmpty() || !kSafe.match(q).hasMatch()) return result;
    const QStringList terms = q.split(' ', Qt::SkipEmptyParts);

    switch (mgr) {
    case PkgMgr::APT: {
        const QString out = CommandUtil::execProgramOutput("apt-cache", QStringList{"search"} + terms, 20000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const int sep = line.indexOf(" - ");
            if (sep < 0) continue;
            PackageInfo p; p.manager = mgr;
            p.name        = line.left(sep).trimmed();
            p.description = line.mid(sep + 3).trimmed();
            result << p;
        }
        break;
    }
    case PkgMgr::DNF:
    case PkgMgr::DNF5: {
        const QString out = CommandUtil::execProgramOutput("dnf", QStringList{"search"} + terms, 30000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const int sep = line.indexOf(':');
            if (sep < 0 || line.startsWith('=') || line.startsWith(' ')) continue;
            PackageInfo p; p.manager = mgr;
            p.name        = line.left(sep).trimmed().split('.').first();
            p.description = line.mid(sep + 1).trimmed();
            if (!p.name.isEmpty()) result << p;
        }
        break;
    }
    case PkgMgr::Zypper: {
        const QString out = CommandUtil::execProgramOutput("zypper", QStringList{"--no-refresh", "search"} + terms, 30000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            if (!line.startsWith("i") && !line.startsWith("  ")) continue;
            const QStringList cols = line.split('|');
            if (cols.size() < 3) continue;
            PackageInfo p; p.manager = mgr;
            p.name        = cols[1].trimmed();
            p.description = cols[2].trimmed();
            if (!p.name.isEmpty() && p.name != "Name") result << p;
        }
        break;
    }
    case PkgMgr::Pacman: {
        const QString out = CommandUtil::execProgramOutput("pacman", QStringList{"-Ss"} + terms, 20000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            if (!line.startsWith(' ') && line.contains('/')) {
                PackageInfo p; p.manager = mgr;
                const auto nameVer = line.split('/').last().split(' ');
                p.name    = nameVer.value(0);
                p.version = nameVer.value(1);
                result << p;
            }
        }
        break;
    }
    case PkgMgr::Flatpak: {
        // Tab-separated: Name  Description  App ID  Version  Branch  Remotes
        const QString out = CommandUtil::execProgramOutput("flatpak", QStringList{"search", "--columns=name,description,application,version"} + terms, 20000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const QStringList c = line.split('\t');
            if (c.size() < 3) continue;
            PackageInfo p; p.manager = mgr;
            p.name        = c.value(2).trimmed();   // app-id is what install() needs
            p.description = c.value(0).trimmed() + " — " + c.value(1).trimmed();
            p.version     = c.value(3).trimmed();
            if (!p.name.isEmpty() && p.name != "Application ID") result << p;
        }
        break;
    }
    case PkgMgr::Snap: {
        const QString out = CommandUtil::execProgramOutput("snap", QStringList{"find"} + terms, 20000);
        const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        for (int i = 1; i < lines.size(); ++i) {           // skip the header row
            const QStringList c = lines[i].split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);
            if (c.size() < 2) continue;
            PackageInfo p; p.manager = mgr;
            p.name        = c.value(0).trimmed();
            p.version     = c.value(1).trimmed();
            p.description = c.size() > 4 ? c.value(4).trimmed() : QString();
            if (!p.name.isEmpty()) result << p;
        }
        break;
    }
    default:
        break;
    }
    return result;
}

// ─── Upgradable packages ──────────────────────────────────────────────────
// Packages with a newer version available. Read-only (no root for the query on
// most managers); all through execProgramOutput (no shell).
QVector<PackageInfo> PackageTool::upgradable(PkgMgr mgr)
{
    QVector<PackageInfo> result;
    switch (mgr) {
    case PkgMgr::APT: {
        // apt list --upgradable: "name/repo NEWVER arch [upgradable from: OLDVER]"
        const QString out = CommandUtil::execProgramOutput("apt", {"list", "--upgradable"}, 30000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const int slash = line.indexOf('/');
            if (slash <= 0 || line.startsWith("Listing")) continue;
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            PackageInfo p; p.manager = mgr;
            p.name    = line.left(slash);
            p.version = parts.value(1);
            result << p;
        }
        break;
    }
    case PkgMgr::DNF:
    case PkgMgr::DNF5: {
        const QString out = CommandUtil::execProgramOutput("dnf", {"--quiet", "check-update"}, 60000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const QStringList c = line.split(' ', Qt::SkipEmptyParts);
            if (c.size() < 3 || line.startsWith("Last metadata")) continue;
            PackageInfo p; p.manager = mgr;
            p.name    = c.value(0).split('.').first();
            p.version = c.value(1);
            if (!p.name.isEmpty()) result << p;
        }
        break;
    }
    case PkgMgr::Pacman: {
        const QString out = CommandUtil::execProgramOutput("pacman", {"-Qu"}, 30000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const QStringList c = line.split(' ', Qt::SkipEmptyParts);
            if (c.isEmpty()) continue;
            PackageInfo p; p.manager = mgr;
            p.name    = c.value(0);
            p.version = c.value(3);   // "name old -> new"
            result << p;
        }
        break;
    }
    case PkgMgr::Flatpak: {
        const QString out = CommandUtil::execProgramOutput("flatpak", {"remote-ls", "--updates", "--columns=application,version"}, 30000);
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            const QStringList c = line.split('\t');
            PackageInfo p; p.manager = mgr;
            p.name    = c.value(0).trimmed();
            p.version = c.value(1).trimmed();
            if (!p.name.isEmpty() && p.name != "Application ID") result << p;
        }
        break;
    }
    case PkgMgr::Snap: {
        const QString out = CommandUtil::execProgramOutput("snap", {"refresh", "--list"}, 30000);
        const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        for (int i = 1; i < lines.size(); ++i) {
            const QStringList c = lines[i].split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);
            if (c.size() < 2) continue;
            PackageInfo p; p.manager = mgr;
            p.name    = c.value(0).trimmed();
            p.version = c.value(1).trimmed();
            if (!p.name.isEmpty()) result << p;
        }
        break;
    }
    default:
        break;
    }
    return result;
}

// ─── Manually / externally installed apps ─────────────────────────────────
// Apps dropped in by install scripts (e.g. Megacubo's `wget | bash`), tarballs
// under /opt, or AppImages — none of which any package manager tracks. We find
// them through their .desktop launchers and remove the launcher, the payload,
// and per-user leftovers, guarding hard against ever deleting a shared root.
namespace {

QStringList manualDesktopDirs()
{
    const QString home = QDir::homePath();
    return {
        "/usr/share/applications",
        "/usr/local/share/applications",
        home + "/.local/share/applications",
    };
}

// First absolute-path token of a .desktop Exec= line, minus field codes,
// env-var assignments, quotes and interpreter/env prefixes.
QString execBinary(const QString &exec)
{
    const QStringList tokens = exec.trimmed().split(' ', Qt::SkipEmptyParts);
    for (QString t : tokens) {
        if (t.startsWith('"') || t.startsWith('\'')) t = t.mid(1);
        if (t.endsWith('"')   || t.endsWith('\''))   t.chop(1);
        if (t.isEmpty() || t.startsWith('%'))                 continue; // field code
        if (t.contains('=') && !t.startsWith('/'))            continue; // VAR=val
        if (t == "env" || t == "sh" || t == "bash" || t == "/usr/bin/env") continue;
        if (t.startsWith('/')) return t;
    }
    return {};
}

// True when this executable is an externally/manually installed app rather than
// something a system package manager owns (/usr/bin, /snap, flatpak exports …).
bool isManualExec(const QString &path)
{
    const QString home = QDir::homePath();
    if (path.endsWith(".AppImage", Qt::CaseInsensitive)) return true;
    if (path.startsWith("/opt/"))       return true;
    if (path.startsWith("/usr/local/")) return true;
    if (path.startsWith(home + "/") && !path.startsWith(home + "/.local/share/flatpak"))
        return true;
    return false;
}

// The filesystem payload to delete + a base name to match per-user leftovers.
void payloadFor(const QString &exec, QString &payload, QString &base)
{
    const QRegularExpression optRe("^(/opt/[^/]+)");
    const auto m = optRe.match(exec);
    if (m.hasMatch()) { payload = m.captured(1); base = QFileInfo(payload).fileName(); return; }
    // AppImage or standalone binary — remove just that file.
    payload = exec;
    base    = QFileInfo(exec).completeBaseName();
}

// Never delete a shared/system root: the payload must be a deep, app-specific
// path, never a top-level or shared bin/lib/share directory.
bool isSafeRemovalPath(const QString &p)
{
    if (p.isEmpty()) return false;
    const QString home = QDir::homePath();
    static const QSet<QString> forbidden = {
        "/", "/opt", "/usr", "/usr/local", "/usr/local/bin", "/usr/local/share",
        "/usr/local/lib", "/usr/bin", "/bin", "/sbin", "/usr/sbin", "/usr/share",
        "/etc", "/var", "/home", home, home + "/.local", home + "/.local/bin",
        home + "/.local/share", home + "/.local/state", home + "/.config",
        home + "/.cache", home + "/.local/share/applications",
    };
    const QString c = QDir::cleanPath(p);
    if (forbidden.contains(c)) return false;
    if (c.count('/') < 2)      return false; // too shallow to be app-specific
    return true;
}

qint64 duBytes(const QString &path)
{
    if (path.isEmpty()) return 0;
    const QString out = CommandUtil::execProgramOutput("du", {"-sb", path}, 8000);
    return out.section('\t', 0, 0).trimmed().toLongLong();
}

// Of the given files, return the subset a real package manager owns — i.e. NOT
// manual installs, and never ours to delete (their files may be shared, e.g. a
// browser backing several PWA launchers). Batched into a single query so it is
// fast and deterministic. Only system paths can be owned; ~/ ones never are.
QSet<QString> packageOwned(const QStringList &sysPaths)
{
    QSet<QString> owned;
    if (sysPaths.isEmpty()) return owned;
    static const QString tool =
        CommandUtil::commandExists("dpkg")   ? QStringLiteral("dpkg")   :
        CommandUtil::commandExists("rpm")    ? QStringLiteral("rpm")    :
        CommandUtil::commandExists("pacman") ? QStringLiteral("pacman") : QString();
    if (tool.isEmpty()) return owned;

    if (tool == "dpkg") {
        QStringList args = {"-S"}; args += sysPaths;
        const QString o = CommandUtil::execProgramOutput("dpkg", args, 30000);
        for (const QString &line : o.split('\n')) {          // "pkg: /opt/foo/bar"
            const int idx = line.lastIndexOf(": ");
            if (idx < 0) continue;
            const QString p = line.mid(idx + 2).trimmed();
            if (p.startsWith('/')) owned << p;
        }
    } else if (tool == "rpm") {
        QStringList args = {"-qf"}; args += sysPaths;         // one output line per input, in order
        const QStringList out = CommandUtil::execProgramOutput("rpm", args, 30000)
                                    .split('\n');
        for (int i = 0; i < sysPaths.size() && i < out.size(); ++i)
            if (!out[i].contains("not owned") && !out[i].trimmed().isEmpty())
                owned << sysPaths[i];
    } else { // pacman -Qo
        QStringList args = {"-Qo"}; args += sysPaths;
        const QString o = CommandUtil::execProgramOutput("pacman", args, 30000);
        for (const QString &line : o.split('\n'))             // "/opt/foo is owned by pkg 1.0"
            if (line.contains("owned by")) {
                const QString p = line.section(QStringLiteral(" is owned by"), 0, 0).trimmed();
                if (p.startsWith('/')) owned << p;
            }
    }
    return owned;
}

QString humanBytes(qint64 b)
{
    if (b <= 0) return {};
    double v = b; const char *u[] = {"B","KB","MB","GB","TB"}; int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return QString::number(v, 'f', v < 10 && i > 0 ? 1 : 0) + " " + u[i];
}

struct ManualApp {
    QString name, version, desktopPath, payload, base;
    qint64  sizeBytes = 0;
};

QVector<ManualApp> scanManualApps()
{
    // Pass 1 — gather candidates from every .desktop launcher, remembering the
    // executable so we can batch the (slow) package-ownership query afterwards.
    struct Cand { ManualApp app; QString bin; };
    QVector<Cand> cands;
    QSet<QString> seen;
    for (const QString &dir : manualDesktopDirs()) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &file : d.entryList({"*.desktop"}, QDir::Files)) {
            const QString path = d.absoluteFilePath(file);
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QString name, exec, type, hidden, version;
            const QList<QByteArray> lines = f.readAll().split('\n');
            for (const QByteArray &raw : lines) {
                const QString l = QString::fromUtf8(raw).trimmed();
                if      (l.startsWith("Name=")        && name.isEmpty()) name = l.mid(5);
                else if (l.startsWith("Exec=")        && exec.isEmpty()) exec = l.mid(5);
                else if (l.startsWith("Type="))                          type = l.mid(5);
                else if (l.startsWith("Hidden="))                        hidden = l.mid(7);
                else if (l.startsWith("X-AppVersion="))                  version = l.mid(13);
            }
            if (!type.isEmpty() && type != "Application") continue;
            if (hidden.compare("true", Qt::CaseInsensitive) == 0) continue;

            const QString bin = execBinary(exec);
            if (bin.isEmpty() || !isManualExec(bin)) continue;

            ManualApp a;
            a.name    = name.isEmpty() ? QFileInfo(bin).fileName() : name;
            a.version = version;
            a.desktopPath = path;
            payloadFor(bin, a.payload, a.base);
            if (!isSafeRemovalPath(a.payload)) continue;
            if (seen.contains(a.payload))      continue;
            seen.insert(a.payload);
            cands << Cand{a, bin};
        }
    }

    // Pass 2 — drop anything a package manager owns (shared/managed installs),
    // then compute on-disk size for the survivors.
    QStringList sysBins;
    for (const auto &c : cands)
        if (c.bin.startsWith("/opt") || c.bin.startsWith("/usr"))
            sysBins << c.bin;
    const QSet<QString> owned = packageOwned(sysBins);

    QVector<ManualApp> apps;
    for (const auto &c : cands) {
        if (owned.contains(c.bin)) continue;
        ManualApp a = c.app;
        a.sizeBytes = duBytes(a.payload);
        apps << a;
    }
    return apps;
}

} // namespace

QVector<PackageInfo> PackageTool::manualPackages()
{
    QVector<PackageInfo> out;
    for (const auto &a : scanManualApps()) {
        PackageInfo p;
        p.name        = a.name;
        p.version     = a.version;
        p.manager     = PkgMgr::Manual;
        p.description = a.payload;              // where it lives on disk
        p.size        = humanBytes(a.sizeBytes);
        out << p;
    }
    return out;
}

bool PackageTool::removeManual(const QString &name)
{
    const QString home = QDir::homePath();
    for (const auto &a : scanManualApps()) {
        if (a.name != name) continue;

        QStringList rootPaths, userPaths;
        auto add = [&](const QString &p) {
            if (p.isEmpty() || !isSafeRemovalPath(p)) return;
            if (p.startsWith("/opt") || p.startsWith("/usr"))
                rootPaths << p;
            else
                userPaths << p;
        };
        add(a.payload);
        // The .desktop launcher (delete even if it sits in a system dir).
        if (!a.desktopPath.isEmpty()) {
            if (a.desktopPath.startsWith("/usr") || a.desktopPath.startsWith("/opt"))
                rootPaths << a.desktopPath;
            else
                userPaths << a.desktopPath;
        }
        // Per-user leftovers keyed on the app's base name (exact, case-insensitive).
        if (!a.base.isEmpty()) {
            const QStringList roots = {
                home + "/.config", home + "/.cache",
                home + "/.local/share", home + "/.local/state",
            };
            for (const QString &r : roots) {
                QDir rd(r);
                if (!rd.exists()) continue;
                for (const QString &e : rd.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
                    if (e.compare(a.base, Qt::CaseInsensitive) != 0) continue;
                    const QString full = rd.absoluteFilePath(e);
                    if (isSafeRemovalPath(full)) userPaths << full;
                }
            }
        }

        bool ok = true;
        for (const QString &p : userPaths) {
            QFileInfo fi(p);
            if (fi.isDir())       { if (!QDir(p).removeRecursively()) ok = false; }
            else if (fi.exists()) { if (!QFile::remove(p))            ok = false; }
        }
        if (!rootPaths.isEmpty()) {
            QStringList args = {"rm", "-rf"};
            args += rootPaths;
            if (CommandUtil::execProgram("pkexec", args, 120000) != 0) ok = false;
        }
        // Best-effort: refresh the launcher database so the entry disappears.
        CommandUtil::execProgram("update-desktop-database",
            {home + "/.local/share/applications"}, 8000);
        return ok;
    }
    return false;
}
