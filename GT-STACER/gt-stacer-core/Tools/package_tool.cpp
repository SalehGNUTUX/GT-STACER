#include "package_tool.h"
#include "../Utils/command_util.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QRegularExpression>

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
    return result;
}

// ─── Removal ─────────────────────────────────────────────────────────────
// Each entry builds an argv list (program + args) and is passed to QProcess
// without going through a shell, so package names with metacharacters cannot
// be turned into command injection. We additionally validate the name.
bool PackageTool::remove(const QString &name, PkgMgr mgr)
{
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

// ─── Cache cleaning ───────────────────────────────────────────────────────
bool PackageTool::cleanCache(PkgMgr mgr)
{
    if (mgr == PkgMgr::Unknown) mgr = primaryManager();
    switch (mgr) {
    case PkgMgr::APT:    return CommandUtil::execStatus("pkexec apt-get clean") == 0;
    case PkgMgr::DNF:
    case PkgMgr::DNF5:   return CommandUtil::execStatus("pkexec dnf clean all") == 0;
    case PkgMgr::YUM:    return CommandUtil::execStatus("pkexec yum clean all") == 0;
    case PkgMgr::Pacman: return CommandUtil::execStatus("pkexec pacman -Sc --noconfirm") == 0;
    case PkgMgr::Zypper: return CommandUtil::execStatus("pkexec zypper clean") == 0;
    case PkgMgr::APK:    return CommandUtil::execStatus("pkexec apk cache clean") == 0;
    default:             return false;
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
    QString cmd;

    switch (mgr) {
    case PkgMgr::APT:
        cmd = "apt-cache search " + query + " 2>/dev/null";
        for (const auto &line : CommandUtil::execLines(cmd)) {
            int sep = line.indexOf(" - ");
            if (sep < 0) continue;
            PackageInfo p; p.manager = mgr;
            p.name        = line.left(sep).trimmed();
            p.description = line.mid(sep + 3).trimmed();
            result << p;
        }
        break;
    case PkgMgr::DNF:
    case PkgMgr::DNF5:
        cmd = "dnf search " + query + " 2>/dev/null";
        for (const auto &line : CommandUtil::execLines(cmd)) {
            auto p2 = line.split(':');
            if (p2.size() < 2) continue;
            PackageInfo p; p.manager = mgr;
            p.name        = p2[0].trimmed().split('.').first();
            p.description = p2[1].trimmed();
            result << p;
        }
        break;
    case PkgMgr::Pacman:
        cmd = "pacman -Ss " + query + " 2>/dev/null";
        for (const auto &line : CommandUtil::execLines(cmd)) {
            if (!line.startsWith(' ') && line.contains('/')) {
                PackageInfo p; p.manager = mgr;
                auto nameVer = line.split('/').last().split(' ');
                p.name    = nameVer.value(0);
                p.version = nameVer.value(1);
                result << p;
            }
        }
        break;
    default:
        break;
    }
    return result;
}
