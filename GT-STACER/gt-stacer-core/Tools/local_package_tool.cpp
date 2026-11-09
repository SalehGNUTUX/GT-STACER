#include "local_package_tool.h"
#include "appimage_tool.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>

namespace {

// ── helpers ────────────────────────────────────────────────────────────────

QString run(const QString &prog, const QStringList &args, int *code = nullptr, int timeoutMs = 30000)
{
    return CommandUtil::execProgramC(prog, args, timeoutMs, code);
}

// Debian-control / RFC-822 style "Key: value" with indented continuation lines.
QHash<QString, QString> parseControl(const QString &text)
{
    QHash<QString, QString> f;
    QString key;
    for (const QString &line : text.split('\n')) {
        if (line.isEmpty()) continue;
        if ((line.startsWith(' ') || line.startsWith('\t')) && !key.isEmpty()) {
            f[key] += '\n' + line.mid(1);
            continue;
        }
        const int c = line.indexOf(':');
        if (c <= 0) continue;
        key = line.left(c).trimmed();
        f[key] = line.mid(c + 1).trimmed();
    }
    return f;
}

// pacman -Qi style "Key            : value" (continuations are deeply indented).
QHash<QString, QString> parsePacmanInfo(const QString &text)
{
    QHash<QString, QString> f;
    QString key;
    static const QRegularExpression kv("^([A-Za-z][A-Za-z ]*[A-Za-z])\\s+:\\s?(.*)$");
    for (const QString &line : text.split('\n')) {
        if (line.trimmed().isEmpty()) continue;
        const auto m = kv.match(line);
        if (m.hasMatch() && !line.startsWith(' ')) {
            key = m.captured(1).trimmed();
            f[key] = m.captured(2).trimmed();
        } else if (!key.isEmpty()) {
            f[key] += '\n' + line.trimmed();
        }
    }
    return f;
}

// Debian "Description:" → (summary, long description), with " ." paragraph marks.
QPair<QString, QString> splitDebDescription(const QString &d)
{
    const int nl = d.indexOf('\n');
    const QString summary = nl < 0 ? d : d.left(nl);
    QStringList body;
    if (nl >= 0)
        for (QString l : d.mid(nl + 1).split('\n')) {
            l = l.trimmed();
            body << (l == "." ? QString() : l);
        }
    return {summary.trimmed(), body.join('\n').trimmed()};
}

QString machineArch() { return QSysInfo::currentCpuArchitecture(); }   // x86_64, arm64, …

// Inside Flatpak the file chooser hands us a document-portal path that the
// host-side package manager can't open; stage a copy under ~/.cache, which the
// host sees at the same path (the manifest grants --filesystem=home).
QString stageForHost(const QString &path, QString *warning)
{
    if (!CommandUtil::inFlatpak()) return path;
    const QString home = QDir::homePath();
    if (QDir::cleanPath(path).startsWith(home + '/') && !path.startsWith("/run/")) return path;
    const QString dir = home + "/.cache/gt-stacer-tmp";
    QDir().mkpath(dir);
    const QString dst = dir + '/' + QFileInfo(path).fileName();
    QFile::remove(dst);
    if (QFile::copy(path, dst)) {
        if (warning) *warning = QStringLiteral("A copy was staged in ~/.cache/gt-stacer-tmp for the host package manager.");
        return dst;
    }
    return path;
}

// A name we may hand to a package manager: plain, no option-looking prefix.
bool safeName(const QString &n) { return CommandUtil::isSafeIdentifier(n) && !n.contains('/'); }

// ── Debian ─────────────────────────────────────────────────────────────────

struct DebAlt { QString name; QString op; QString ver; };

QVector<QVector<DebAlt>> parseDebRelations(const QString &field)
{
    QVector<QVector<DebAlt>> groups;
    static const QRegularExpression rx(
        "^\\s*([A-Za-z0-9][A-Za-z0-9+.-]*)(?::[A-Za-z0-9-]+)?\\s*"
        "(?:\\(\\s*(<<|<=|=|>=|>>|<|>)\\s*([^)\\s]+)\\s*\\))?");
    for (const QString &grp : field.split(',', Qt::SkipEmptyParts)) {
        QVector<DebAlt> alts;
        for (const QString &alt : grp.split('|', Qt::SkipEmptyParts)) {
            const auto m = rx.match(alt.simplified());
            if (m.hasMatch()) alts.append({m.captured(1), m.captured(2), m.captured(3)});
        }
        if (!alts.isEmpty()) groups << alts;
    }
    return groups;
}

void inspectDeb(LocalPackageInfo &info)
{
    int code = 0;
    const QString ctl = run("dpkg-deb", {"-f", info.path}, &code);
    if (code != 0 || ctl.trimmed().isEmpty()) {
        info.problem = QStringLiteral("This file is not a valid Debian package (dpkg-deb could not read it).");
        info.installable = false;
        return;
    }
    const auto f = parseControl(ctl);
    info.name        = f.value("Package");
    info.version     = f.value("Version");
    info.arch        = f.value("Architecture");
    info.maintainer  = f.value("Maintainer");
    info.homepage    = f.value("Homepage");
    const auto d     = splitDebDescription(f.value("Description"));
    info.summary     = d.first;
    info.description = d.second;
    bool ok = false;
    const qint64 kib = f.value("Installed-Size").toLongLong(&ok);
    if (ok) info.installedSize = kib * 1024;

    // Architecture: "all", the native one, or an enabled foreign one.
    const QString native = run("dpkg", {"--print-architecture"}).trimmed();
    const QStringList foreign = run("dpkg", {"--print-foreign-architectures"}).split('\n', Qt::SkipEmptyParts);
    if (!info.arch.isEmpty() && info.arch != "all" && info.arch != native && !foreign.contains(info.arch)) {
        info.problem = QStringLiteral("This package is built for %1, but this system is %2.")
                           .arg(info.arch, native);
        info.installable = false;
    }

    if (safeName(info.name)) {
        const QString st = run("dpkg-query", {"-W", "-f=${db:Status-Abbrev}${Version}", info.name});
        if (st.startsWith("ii")) info.installedVersion = st.mid(3).trimmed();
    }

    // Installed set + virtual packages they provide — ONE query for everything.
    QHash<QString, QString> installed;   // name → version
    QSet<QString> provided;
    const QString db = run("dpkg-query", {"-W", "-f=${db:Status-Abbrev}\t${Package}\t${Version}\t${Provides}\n"});
    for (const QString &line : db.split('\n', Qt::SkipEmptyParts)) {
        const QStringList c = line.split('\t');
        if (c.size() < 3 || !c[0].startsWith("ii")) continue;
        installed.insert(c[1], c[2]);
        if (c.size() > 3)
            for (const QString &p : c[3].split(',', Qt::SkipEmptyParts))
                provided.insert(p.trimmed().section(' ', 0, 0).section(':', 0, 0));
    }
    auto versionOk = [](const QString &have, const DebAlt &a) {
        if (a.op.isEmpty()) return true;
        static const QHash<QString, QString> ops = {
            {"<<","lt"},{"<","lt"},{"<=","le"},{"=","eq"},{">=","ge"},{">>","gt"},{">","gt"}};
        int rc = -1;
        run("dpkg", {"--compare-versions", have, ops.value(a.op, "ge"), a.ver}, &rc);
        return rc == 0;
    };
    auto satisfied = [&](const DebAlt &a) {
        if (installed.contains(a.name)) return versionOk(installed.value(a.name), a);
        return a.op.isEmpty() && provided.contains(a.name);
    };

    struct Rel { QVector<DebAlt> alts; QString declared; bool required; QString note; };
    QVector<Rel> rels;
    auto add = [&](const QString &field, bool required, const QString &note) {
        const QString raw = f.value(field);
        const QStringList declared = raw.split(',', Qt::SkipEmptyParts);
        const auto groups = parseDebRelations(raw);
        for (int i = 0; i < groups.size(); ++i)
            rels.append({groups[i], i < declared.size() ? declared[i].simplified() : QString(),
                         required, note});
    };
    add("Pre-Depends", true,  QString());
    add("Depends",     true,  QString());
    add("Recommends",  false, QStringLiteral("recommended"));
    add("Suggests",    false, QStringLiteral("suggested"));

    // Which missing names do the repositories have? One apt-cache call.
    QStringList ask;
    for (const Rel &r : rels) {
        bool ok = false;
        for (const DebAlt &a : r.alts) if (satisfied(a)) { ok = true; break; }
        if (!ok) for (const DebAlt &a : r.alts) if (safeName(a.name) && !ask.contains(a.name)) ask << a.name;
    }
    QHash<QString, QString> candidate;   // name → repository candidate version
    QSet<QString> virtualOk;             // purely virtual, provided by a real package
    if (!ask.isEmpty() && PackageTool::has(PkgMgr::APT)) {
        const QString pol = run("apt-cache", QStringList{"policy"} + ask, nullptr, 60000);
        QString cur;
        QSet<QString> seen;
        for (const QString &line : pol.split('\n')) {
            if (!line.startsWith(' ') && line.endsWith(':')) { cur = line.chopped(1).section(':', 0, 0); seen << cur; continue; }
            const QString t = line.trimmed();
            if (t.startsWith("Candidate:") && !cur.isEmpty() && !t.contains("(none)"))
                candidate.insert(cur, t.mid(10).trimmed());
        }
        // Purely virtual names have no candidate but are provided by real packages.
        for (const QString &n : ask)
            if (!candidate.contains(n)) {
                const QString sp = run("apt-cache", {"showpkg", n});
                const int rp = sp.indexOf("Reverse Provides:");
                if (rp >= 0 && !sp.mid(rp + 17).trimmed().isEmpty()) virtualOk << n;
            }
    }

    for (const Rel &r : rels) {
        LocalDependency dep;
        dep.declared = r.declared;
        dep.required = r.required;
        dep.note     = r.note;
        dep.name     = r.alts.first().name;
        dep.state    = LocalDependency::State::Unavailable;
        bool done = false;
        for (const DebAlt &a : r.alts)
            if (satisfied(a)) { dep.name = a.name; dep.state = LocalDependency::State::Satisfied; done = true; break; }
        if (!done)
            for (const DebAlt &a : r.alts)
                // Available = the repository version also meets the constraint.
                if ((candidate.contains(a.name) && versionOk(candidate.value(a.name), a))
                    || (a.op.isEmpty() && virtualOk.contains(a.name))) {
                    dep.name = a.name; dep.state = LocalDependency::State::Installable; break;
                }
        if (!safeName(dep.name)) dep.name.clear();
        info.deps << dep;
    }
    info.depsChecked = true;
    info.installer = PackageTool::has(PkgMgr::APT) ? PkgMgr::APT : PkgMgr::Unknown;
}

// ── RPM ────────────────────────────────────────────────────────────────────

PkgMgr rpmInstaller()
{
    for (PkgMgr m : {PkgMgr::DNF5, PkgMgr::DNF, PkgMgr::YUM, PkgMgr::TDNF, PkgMgr::Zypper, PkgMgr::RpmOstree})
        if (PackageTool::has(m)) return m;
    return PkgMgr::Unknown;
}

void inspectRpm(LocalPackageInfo &info)
{
    if (!CommandUtil::commandExists("rpm")) {
        info.problem = QStringLiteral("Reading .rpm packages needs the rpm tool, which is not installed.");
        return;
    }
    const QStringList q = {"-qp", "--nosignature", "--nodigest"};
    int code = 0;
    const QString out = run("rpm", q + QStringList{"--qf",
        "%{NAME}\n%{VERSION}-%{RELEASE}\n%{ARCH}\n%{SIZE}\n%{URL}\n%{LICENSE}\n%{PACKAGER}\n%{SUMMARY}\n",
        info.path}, &code);
    const QStringList v = out.split('\n');
    if (code != 0 || v.size() < 8) {
        info.problem = QStringLiteral("This file is not a valid RPM package (rpm could not read it).");
        info.installable = false;
        return;
    }
    auto none = [](const QString &s) { return s == "(none)" ? QString() : s; };
    info.name          = v[0];
    info.version       = v[1];
    info.arch          = v[2];
    info.installedSize = v[3].toLongLong();
    info.homepage      = none(v[4]);
    info.license       = none(v[5]);
    info.maintainer    = none(v[6]);
    info.summary       = v[7];
    info.description   = run("rpm", q + QStringList{"--qf", "%{DESCRIPTION}", info.path}).trimmed();

    const QString m = machineArch();
    // QSysInfo says "arm64" where RPM says "aarch64".
    if (info.arch != "noarch" && info.arch != m && !(m == "arm64" && info.arch == "aarch64")) {
        info.problem = QStringLiteral("This package is built for %1, but this system is %2.").arg(info.arch, m);
        info.installable = false;
    }

    const PkgMgr inst = rpmInstaller();
    if (inst != PkgMgr::Unknown && safeName(info.name)) {
        int rc = -1;
        const QString iv = run("rpm", {"-q", "--qf", "%{VERSION}-%{RELEASE}", info.name}, &rc);
        if (rc == 0) info.installedVersion = iv.trimmed();
    }

    auto caps = [&](const QString &flag) {
        QStringList out;
        for (QString l : run("rpm", q + QStringList{flag, info.path}).split('\n', Qt::SkipEmptyParts)) {
            l = l.trimmed();
            if (l.startsWith("rpmlib(") || l.startsWith("config(") || out.contains(l)) continue;
            out << l;
        }
        return out;
    };
    const QStringList requires_ = caps("--requires");
    const QStringList recommends = caps("--recommends");
    const QStringList suggests   = caps("--suggests");

    // Satisfied? `rpm -q --whatprovides` per capability name (version part dropped).
    // Only meaningful on an rpm-based system — elsewhere the rpm db is empty.
    QSet<QString> missing;
    const bool rpmSystem = inst != PkgMgr::Unknown;
    if (rpmSystem) {
        QStringList names;
        for (const QString &c : requires_ + recommends + suggests) {
            const QString n = c.section(' ', 0, 0);
            if (!n.startsWith('(') && !names.contains(n)) names << n;
        }
        if (!names.isEmpty()) {
            const QString wp = run("rpm", QStringList{"-q", "--whatprovides"} + names, nullptr, 60000);
            for (const QString &l : wp.split('\n', Qt::SkipEmptyParts))
                if (l.startsWith("no package provides ")) missing << l.mid(20).trimmed();
        }
    }
    auto add = [&](const QStringList &list, bool required, const QString &note) {
        for (const QString &c : list) {
            LocalDependency dep;
            dep.declared = c;
            dep.required = required;
            dep.note     = note;
            const QString n = c.section(' ', 0, 0);
            // Only plain package names are selectable — not sonames, paths or rich deps.
            if (!n.startsWith('(') && !n.startsWith('/') && !n.contains('(') && safeName(n)) dep.name = n;
            if (!rpmSystem)               dep.state = LocalDependency::State::Missing;
            else if (missing.contains(n)) dep.state = LocalDependency::State::Missing;
            else                          dep.state = LocalDependency::State::Satisfied;
            info.deps << dep;
        }
    };
    add(requires_,  true,  QString());
    add(recommends, false, QStringLiteral("recommended"));
    add(suggests,   false, QStringLiteral("suggested"));
    info.depsChecked = rpmSystem;
    info.installer = inst;
    if (rpmSystem)
        info.warnings << QStringLiteral("Missing required dependencies are downloaded from your repositories during installation.");
}

// ── Arch (pacman) ──────────────────────────────────────────────────────────

void inspectPacman(LocalPackageInfo &info)
{
    int code = 0;
    const auto f = parsePacmanInfo(run("pacman", {"-Qip", info.path}, &code));
    if (code != 0 || f.value("Name").isEmpty()) {
        info.problem = QStringLiteral("This file is not a valid pacman package (pacman could not read it).");
        info.installable = false;
        return;
    }
    info.name       = f.value("Name");
    info.version    = f.value("Version");
    info.arch       = f.value("Architecture");
    info.summary    = f.value("Description");
    info.homepage   = f.value("URL");
    info.license    = f.value("Licenses");
    info.maintainer = f.value("Packager");
    if (info.arch != "any" && !info.arch.isEmpty() && info.arch != machineArch()) {
        info.problem = QStringLiteral("This package is built for %1, but this system is %2.").arg(info.arch, machineArch());
        info.installable = false;
    }
    if (safeName(info.name)) {
        int rc = -1;
        const QString iv = run("pacman", {"-Q", info.name}, &rc);
        if (rc == 0) info.installedVersion = iv.trimmed().section(' ', 1);
    }

    QStringList depends;
    const QString dep = f.value("Depends On");
    if (dep != "None") depends = dep.simplified().split(' ', Qt::SkipEmptyParts);
    QVector<QPair<QString, QString>> optional;   // name, description
    const QString opt = f.value("Optional Deps");
    if (opt != "None")
        for (const QString &l : opt.split('\n', Qt::SkipEmptyParts)) {
            const QString n = l.section(':', 0, 0).trimmed();
            if (!n.isEmpty()) optional.append({n, l.section(':', 1).trimmed()});
        }

    // `pacman -T` prints every dependency that is NOT satisfied.
    QStringList all = depends;
    for (const auto &o : optional) all << o.first;
    QSet<QString> unsatisfied;
    if (!all.isEmpty())
        for (const QString &l : run("pacman", QStringList{"-T"} + all).split('\n', Qt::SkipEmptyParts))
            unsatisfied << l.trimmed();
    auto stripVer = [](const QString &d) { return d.section(QRegularExpression("[<>=]"), 0, 0); };
    auto stateOf = [&](const QString &d) {
        if (!unsatisfied.contains(d)) return LocalDependency::State::Satisfied;
        int rc = -1;
        run("pacman", {"-Sp", "--print-format", "%n", stripVer(d)}, &rc);
        return rc == 0 ? LocalDependency::State::Installable : LocalDependency::State::Unavailable;
    };
    for (const QString &d : depends) {
        LocalDependency x;
        x.declared = d; x.required = true; x.state = stateOf(d);
        x.name = safeName(stripVer(d)) ? stripVer(d) : QString();
        info.deps << x;
    }
    for (const auto &o : optional) {
        LocalDependency x;
        x.declared = o.first; x.required = false; x.note = o.second; x.state = stateOf(o.first);
        x.name = safeName(stripVer(o.first)) ? stripVer(o.first) : QString();
        info.deps << x;
    }
    info.depsChecked = true;
    info.installer = PkgMgr::Pacman;
}

// ── Others (metadata only) ─────────────────────────────────────────────────

void inspectSnap(LocalPackageInfo &info)
{
    const QString yaml = run("unsquashfs", {"-cat", info.path, "meta/snap.yaml"});
    for (const QString &l : yaml.split('\n')) {
        if (l.startsWith(' ')) continue;                 // top-level keys only
        const QString k = l.section(':', 0, 0).trimmed();
        QString v = l.section(':', 1).trimmed();
        if (v.startsWith('\'') || v.startsWith('"')) v = v.mid(1).chopped(1);
        if      (k == "name")    info.name = v;
        else if (k == "version") info.version = v;
        else if (k == "summary") info.summary = v;
        else if (k == "architectures" && !v.isEmpty()) info.arch = v;
    }
    info.installer = PkgMgr::Snap;
    info.warnings << QStringLiteral("A local snap is not signed by the Snap Store: it is installed with --dangerous and will not update automatically.");
}

void inspectFlatpakRef(LocalPackageInfo &info)
{
    QFile f(info.path);
    if (f.open(QIODevice::ReadOnly))
        for (const QString &l : QString::fromUtf8(f.read(64 * 1024)).split('\n')) {
            const QString k = l.section('=', 0, 0).trimmed();
            const QString v = l.section('=', 1).trimmed();
            if      (k == "Name")        info.name = v;
            else if (k == "Title")       info.summary = v;
            else if (k == "Branch")      info.version = v;
            else if (k == "Homepage" || (k == "Url" && info.homepage.isEmpty())) info.homepage = v;
            else if (k == "Description") info.description = v;
        }
    info.installer = PkgMgr::Flatpak;
    info.warnings << QStringLiteral("The application and any runtime it needs are downloaded from its Flatpak remote.");
}

// "name-1.2.3-r0.apk" → name / version.
void nameVersionFromFile(LocalPackageInfo &info)
{
    QString b = QFileInfo(info.path).fileName();
    b.remove(QRegularExpression("\\.(apk|eopkg|flatpak)$"));
    const auto m = QRegularExpression("^(.+?)-(\\d[^-]*(?:-r?\\d+)?)(?:-\\d+-[a-z0-9_]+)?$").match(b);
    if (m.hasMatch()) { info.name = m.captured(1); info.version = m.captured(2); }
    else info.name = b;
}

} // namespace

// ── public API ─────────────────────────────────────────────────────────────

LocalPkgKind LocalPackageTool::kindOf(const QString &path)
{
    const QString n = QFileInfo(path).fileName().toLower();
    if (n.endsWith(".deb"))        return LocalPkgKind::Deb;
    if (n.endsWith(".rpm"))        return LocalPkgKind::Rpm;
    if (QRegularExpression("\\.pkg\\.tar(\\.(zst|xz|gz|bz2))?$").match(n).hasMatch()) return LocalPkgKind::Pacman;
    if (n.endsWith(".apk"))        return LocalPkgKind::Apk;
    if (n.endsWith(".eopkg"))      return LocalPkgKind::Eopkg;
    if (n.endsWith(".flatpakref")) return LocalPkgKind::FlatpakRef;
    if (n.endsWith(".flatpak"))    return LocalPkgKind::FlatpakBundle;
    if (n.endsWith(".snap"))       return LocalPkgKind::Snap;
    if (n.endsWith(".appimage"))   return LocalPkgKind::AppImage;
    return LocalPkgKind::Unknown;
}

QString LocalPackageTool::kindLabel(LocalPkgKind k)
{
    switch (k) {
    case LocalPkgKind::Deb:           return "Debian package (.deb)";
    case LocalPkgKind::Rpm:           return "RPM package (.rpm)";
    case LocalPkgKind::Pacman:        return "Arch package (.pkg.tar.zst)";
    case LocalPkgKind::Apk:           return "Alpine package (.apk)";
    case LocalPkgKind::Eopkg:         return "Solus package (.eopkg)";
    case LocalPkgKind::FlatpakBundle: return "Flatpak bundle (.flatpak)";
    case LocalPkgKind::FlatpakRef:    return "Flatpak reference (.flatpakref)";
    case LocalPkgKind::Snap:          return "Snap package (.snap)";
    case LocalPkgKind::AppImage:      return "AppImage";
    default:                          return "Unknown";
    }
}

QVector<LocalPkgKind> LocalPackageTool::supportedKinds()
{
    QVector<LocalPkgKind> k;
    if (PackageTool::has(PkgMgr::APT))       k << LocalPkgKind::Deb;
    if (rpmInstaller() != PkgMgr::Unknown)   k << LocalPkgKind::Rpm;
    if (PackageTool::has(PkgMgr::Pacman))    k << LocalPkgKind::Pacman;
    if (PackageTool::has(PkgMgr::APK))       k << LocalPkgKind::Apk;
    if (PackageTool::has(PkgMgr::Eopkg))     k << LocalPkgKind::Eopkg;
    if (PackageTool::has(PkgMgr::Flatpak))   k << LocalPkgKind::FlatpakBundle << LocalPkgKind::FlatpakRef;
    if (PackageTool::has(PkgMgr::Snap))      k << LocalPkgKind::Snap;
    k << LocalPkgKind::AppImage;             // integrated per-user, on any distro
    return k;
}

QStringList LocalPackageTool::patternsFor(LocalPkgKind k)
{
    switch (k) {
    case LocalPkgKind::Deb:           return {"*.deb"};
    case LocalPkgKind::Rpm:           return {"*.rpm"};
    case LocalPkgKind::Pacman:        return {"*.pkg.tar.zst", "*.pkg.tar.xz", "*.pkg.tar.gz", "*.pkg.tar"};
    case LocalPkgKind::Apk:           return {"*.apk"};
    case LocalPkgKind::Eopkg:         return {"*.eopkg"};
    case LocalPkgKind::FlatpakBundle: return {"*.flatpak"};
    case LocalPkgKind::FlatpakRef:    return {"*.flatpakref"};
    case LocalPkgKind::Snap:          return {"*.snap"};
    case LocalPkgKind::AppImage:      return {"*.AppImage", "*.appimage"};
    default:                          return {};
    }
}

QString LocalPackageTool::distroName()
{
    // /run/host/os-release is the HOST's when we run inside Flatpak.
    for (const QString &p : {QStringLiteral("/run/host/os-release"), QStringLiteral("/etc/os-release")}) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) continue;
        for (const QString &l : QString::fromUtf8(f.readAll()).split('\n'))
            if (l.startsWith("PRETTY_NAME=")) {
                QString v = l.mid(12).trimmed();
                if (v.startsWith('"')) v = v.mid(1).chopped(1);
                return v;
            }
    }
    return QSysInfo::prettyProductName();
}

LocalPackageInfo LocalPackageTool::inspect(const QString &pathIn)
{
    LocalPackageInfo info;
    const QFileInfo fi(pathIn);
    info.kind = kindOf(pathIn);
    if (!fi.isFile() || !fi.isReadable()) {
        info.problem = QStringLiteral("The file can't be read.");
        return info;
    }
    info.fileSize = fi.size();
    QString staged;
    info.path = stageForHost(fi.absoluteFilePath(), &staged);
    if (!staged.isEmpty()) info.warnings << staged;

    if (info.kind == LocalPkgKind::Unknown) {
        info.problem = QStringLiteral("This file type is not a package GT-STACER can install.");
        return info;
    }
    // Can THIS system install this kind at all? (Still read the metadata when
    // we can, so the user sees what the file is.)
    info.installable = LocalPackageTool::supportedKinds().contains(info.kind);
    if (!info.installable)
        info.problem = QStringLiteral("%1 can't be installed on %2: it is meant for a different "
                                      "family of distributions (no matching package manager here).")
                           .arg(kindLabel(info.kind), distroName());

    switch (info.kind) {
    case LocalPkgKind::Deb:
        if (CommandUtil::commandExists("dpkg-deb")) inspectDeb(info);
        else nameVersionFromFile(info);
        break;
    case LocalPkgKind::Rpm:    inspectRpm(info); break;
    case LocalPkgKind::Pacman:
        if (CommandUtil::commandExists("pacman")) inspectPacman(info);
        else nameVersionFromFile(info);
        break;
    case LocalPkgKind::Snap:
        if (CommandUtil::commandExists("unsquashfs")) inspectSnap(info);
        if (info.name.isEmpty()) nameVersionFromFile(info);
        break;
    case LocalPkgKind::FlatpakRef: inspectFlatpakRef(info); break;
    case LocalPkgKind::AppImage: {
        const AppImageInfo a = AppImageTool::inspect(info.path);
        if (!a.valid) { info.installable = false; info.problem = a.error; break; }
        info.name    = a.name;
        info.version = a.version;
        info.summary = a.comment;
        info.arch    = machineArch();
        info.warnings << QStringLiteral("AppImages are integrated for your user only (no password): "
                                        "placed in %1 and added to the application menu.")
                             .arg(AppImageTool::managedFolder());
        break;
    }
    case LocalPkgKind::FlatpakBundle:
        nameVersionFromFile(info);
        info.installer = PkgMgr::Flatpak;
        info.warnings << QStringLiteral("If the bundle needs a runtime you don't have, Flatpak downloads it from a configured remote (e.g. Flathub).");
        break;
    case LocalPkgKind::Apk:
        nameVersionFromFile(info);
        info.installer = PkgMgr::APK;
        info.warnings << QStringLiteral("A local .apk is installed with --allow-untrusted (its signature is not checked).");
        break;
    case LocalPkgKind::Eopkg:
        nameVersionFromFile(info);
        info.installer = PkgMgr::Eopkg;
        break;
    default: break;
    }
    if (info.name.isEmpty()) info.name = fi.completeBaseName();

    if (info.installable && info.depsChecked) {
        int unavailable = 0;
        for (const auto &d : info.deps)
            if (d.required && d.state == LocalDependency::State::Unavailable) ++unavailable;
        if (unavailable)
            info.warnings << QStringLiteral("%1 required dependenc(ies) were not found in your repositories — "
                                            "the installation will probably fail.").arg(unavailable);
    }
    return info;
}

QVector<QStringList> LocalPackageTool::installSteps(const LocalPackageInfo &info,
                                                    const QStringList &optionalIn)
{
    QVector<QStringList> steps;
    if (!info.installable) return steps;
    const QString p = info.path;
    if (!QDir::isAbsolutePath(p)) return steps;   // never let a path look like an option

    QStringList optional;
    for (const QString &n : optionalIn) if (safeName(n) && !optional.contains(n)) optional << n;

    switch (info.kind) {
    case LocalPkgKind::Deb:
        if (info.installer == PkgMgr::APT)
            // apt resolves the REQUIRED dependencies from the repositories; we add the
            // optional ones the user ticked and nothing else (--no-install-recommends).
            steps << (QStringList{"pkexec", "apt-get", "install", "-y", "--no-install-recommends", p} + optional);
        else
            steps << QStringList{"pkexec", "dpkg", "-i", p};
        break;
    case LocalPkgKind::Rpm:
        switch (info.installer) {
        case PkgMgr::DNF5:
        case PkgMgr::DNF:  steps << (QStringList{"pkexec", "dnf", "install", "-y", "--setopt=install_weak_deps=False", p} + optional); break;
        case PkgMgr::YUM:  steps << (QStringList{"pkexec", "yum", "install", "-y", p} + optional); break;
        case PkgMgr::TDNF: steps << (QStringList{"pkexec", "tdnf", "install", "-y", p} + optional); break;
        case PkgMgr::Zypper:
            steps << (QStringList{"pkexec", "zypper", "--non-interactive", "install", "--no-recommends",
                                  "--allow-unsigned-rpm", p} + optional);
            break;
        case PkgMgr::RpmOstree: steps << (QStringList{"pkexec", "rpm-ostree", "install", p} + optional); break;
        default: break;
        }
        break;
    case LocalPkgKind::Pacman:
        steps << QStringList{"pkexec", "pacman", "-U", "--noconfirm", p};
        if (!optional.isEmpty())
            steps << (QStringList{"pkexec", "pacman", "-S", "--noconfirm", "--needed", "--asdeps"} + optional);
        break;
    case LocalPkgKind::Apk:    steps << QStringList{"pkexec", "apk", "add", "--allow-untrusted", p}; break;
    case LocalPkgKind::Eopkg:  steps << QStringList{"pkexec", "eopkg", "install", "-y", p}; break;
    case LocalPkgKind::FlatpakBundle: steps << QStringList{"flatpak", "install", "--user", "-y", "--bundle", p}; break;
    case LocalPkgKind::FlatpakRef:    steps << QStringList{"flatpak", "install", "--user", "-y", "--from", p}; break;
    case LocalPkgKind::Snap:   steps << QStringList{"pkexec", "snap", "install", "--dangerous", p}; break;
    default: break;   // AppImage: AppImageTool::integrate (in-process, no root)
    }
    return steps;
}
