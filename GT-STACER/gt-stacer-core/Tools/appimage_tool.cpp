#include "appimage_tool.h"
#include "../Utils/command_util.h"
#include "../Utils/desktop_entry.h"
#include <QCryptographicHash>
#include <QHash>
#include <QSaveFile>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

QString home() { return QDir::homePath(); }
QString appsDir() { return home() + "/.local/share/applications"; }

// Read GearLever's configured AppImages folder. GearLever is usually a Flatpak,
// so its gsettings live in the app's own keyfile, not host dconf — try both,
// then fall back to the documented default (~/AppImages).
QString expandTilde(QString v)
{
    v.remove('\'').remove('"');
    if (v.startsWith('~')) v = home() + v.mid(1);
    return v;
}

// GearLever's Flatpak keyfile, if present (its gsettings live here, not host dconf).
QString gearleverKeyfile() { return home() + "/.var/app/it.mijorus.gearlever/config/glib-2.0/settings/keyfile"; }

// Should the AppImage be MOVED into the folder (true) or CLONED/copied (false)?
// We honour GearLever's `move-appimage-on-integration` preference when it is
// available, so GT-STACER behaves exactly as the user configured GearLever.
// Without GearLever we default to clone (keep the user's original file — safest).
bool moveOnIntegration()
{
    const QString kf = gearleverKeyfile();
    if (QFileInfo::exists(kf)) {
        QSettings s(kf, QSettings::IniFormat);
        s.beginGroup("it/mijorus/gearlever");
        if (s.contains("move-appimage-on-integration")) {
            const bool v = s.value("move-appimage-on-integration").toBool();
            s.endGroup();
            return v;
        }
        s.endGroup();
        return true;   // key absent but GearLever present → its gschema default (move)
    }
    const QString g = CommandUtil::execProgramOutput(
        "gsettings", {"get", "it.mijorus.gearlever", "move-appimage-on-integration"}, 5000).trimmed();
    if (g == "true")  return true;
    if (g == "false") return false;
    return false;      // no GearLever → clone, keeping the original
}

// Byte offset of the squashfs filesystem inside a type-2 AppImage. A type-2
// AppImage is a static ELF runtime with the squashfs appended immediately after
// it, so the offset is the end of the ELF section-header table
// (e_shoff + e_shnum*e_shentsize) — exactly how the AppImage runtime itself
// computes it. Reading it this way lets us extract the icon/desktop with
// unsquashfs WITHOUT executing the AppImage (running an untrusted AppImage's
// --appimage-extract would run its code). Returns -1 if this is not an ELF.
qint64 squashfsOffset(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return -1;
    const QByteArray h = f.read(64);
    if (h.size() < 64 || !h.startsWith(QByteArray("\x7f""ELF", 4))) return -1;
    auto le = [&](int off, int len) -> quint64 {
        quint64 v = 0;
        for (int i = len - 1; i >= 0; --i) v = (v << 8) | quint8(h[off + i]);
        return v;
    };
    if (quint8(h[4]) == 2)   // ELF64
        return qint64(le(0x28, 8) + le(0x3a, 2) * le(0x3c, 2));
    return qint64(le(0x20, 4) + le(0x2e, 2) * le(0x30, 2));   // ELF32
}

// Extract selected files from the AppImage's squashfs into `destDir`, without
// running the AppImage. Needs unsquashfs (squashfs-tools). Returns false if
// unavailable or extraction failed.
bool safeExtract(const QString &appimage, qint64 offset, const QString &destDir,
                 const QStringList &patterns)
{
    if (!CommandUtil::commandExists("unsquashfs")) return false;
    // Shell-wildcard matching of the extract patterns is unsquashfs's default
    // (the flag is -no-wildcards to DISABLE it), so we pass no wildcard flag.
    QStringList args = {"-no-progress", "-f", "-o", QString::number(offset),
                        "-d", destDir, appimage};
    args += patterns;
    return CommandUtil::execProgram("unsquashfs", args, 60000) == 0;
}

} // namespace

QString AppImageTool::managedFolder()
{
    // 1. Flatpak-installed GearLever keeps its gsettings in a keyfile.
    const QString keyfile = home() + "/.var/app/it.mijorus.gearlever/config/glib-2.0/settings/keyfile";
    if (QFileInfo::exists(keyfile)) {
        QSettings s(keyfile, QSettings::IniFormat);
        s.beginGroup("it/mijorus/gearlever");
        const QString v = s.value("appimages-default-folder").toString();
        s.endGroup();
        if (!v.isEmpty()) return expandTilde(v);
    }
    // 2. Native install → host gsettings.
    const QString g = CommandUtil::execProgramOutput(
        "gsettings", {"get", "it.mijorus.gearlever", "appimages-default-folder"}, 5000).trimmed();
    if (!g.isEmpty() && !g.contains("No such")) return expandTilde(g);
    // 3. GearLever's documented default.
    return home() + "/AppImages";
}

bool AppImageTool::gearLeverInstalled()
{
    if (CommandUtil::commandExists("gearlever")) return true;
    const QString out = CommandUtil::execProgramOutput(
        "flatpak", {"list", "--app", "--columns=application"}, 6000);
    return out.contains("it.mijorus.gearlever");
}


// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Follow a symlink chain INSIDE an extraction root. AppImages ship their root
// .desktop/.DirIcon as symlinks (often absolute, e.g. "/usr/share/…"), which must
// be re-rooted under `root` — never followed onto the host filesystem.
QString resolveInside(const QString &root, const QString &path)
{
    const QString r = QDir::cleanPath(root);
    QString cur = QDir::cleanPath(path);
    for (int hop = 0; hop < 16; ++hop) {
        if (!cur.startsWith(r + '/')) return {};
        const QFileInfo fi(cur);
        if (!fi.isSymLink()) return fi.isFile() ? cur : QString();
        const QString raw = fi.readSymLink();
        if (raw.isEmpty()) return {};
        cur = QDir::cleanPath(QDir::isAbsolutePath(raw) ? r + raw : fi.absolutePath() + '/' + raw);
    }
    return {};
}

// The embedded desktop entry: the root-level *.desktop the AppImage spec
// requires (usually a symlink), else the first one in usr/share/applications.
// Returns the resolved file; `id` receives the desktop-file id (basename).
QString findEmbeddedDesktop(const QString &ex, QString *id)
{
    for (const QString &dir : {ex, ex + "/usr/share/applications"}) {
        const auto list = QDir(dir).entryInfoList({"*.desktop"}, QDir::Files | QDir::System);
        for (const QFileInfo &fi : list) {
            const QString real = resolveInside(ex, fi.absoluteFilePath());
            if (real.isEmpty()) continue;
            if (id) { *id = fi.fileName(); id->chop(8); }   // strip ".desktop"
            return real;
        }
    }
    return {};
}

// Desktop-file-id safe form of a string ([A-Za-z0-9._-]).
QString sanitizeId(QString s)
{
    s.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_");
    while (s.startsWith('.') || s.startsWith('-') || s.startsWith('_')) s.remove(0, 1);
    return s.left(96);
}

// The file name minus its .AppImage extension, version and arch:
// "photocraft-0.19.0-linux-x86_64.AppImage" → "photocraft".
QString stemFromFileName(const QString &fileName)
{
    QString b = fileName;
    b.remove(QRegularExpression("\\.appimage$", QRegularExpression::CaseInsensitiveOption));
    const QRegularExpression cut("[-_ ](v?\\d|x86_64|amd64|aarch64|arm64|i[3-6]86|armhf|linux)",
                                 QRegularExpression::CaseInsensitiveOption);
    const auto m = cut.match(b);
    if (m.hasMatch() && m.capturedStart() > 0) b = b.left(m.capturedStart());
    return b;
}

// The AppImage an existing launcher points at (TryExec, else the Exec program).
QString launcherTarget(const DesktopEntry &de)
{
    const QString tryExec = de.value("TryExec");
    if (tryExec.endsWith(".appimage", Qt::CaseInsensitive)) return tryExec;
    const QStringList argv = DesktopEntry::splitExec(de.value("Exec"));
    const int pi = DesktopEntry::programIndex(argv);
    if (pi >= 0 && argv[pi].endsWith(".appimage", Qt::CaseInsensitive)) return argv[pi];
    return {};
}

// Rewrite an Exec line so its program is `appImage`, keeping any env prefix and
// every argument / field code after the program (%U, %F, --flags …).
QString rewriteExec(const QString &exec, const QString &appImage)
{
    QStringList argv = DesktopEntry::splitExec(exec);
    const int pi = DesktopEntry::programIndex(argv);
    if (pi < 0) argv = {appImage};
    else        argv[pi] = appImage;
    QStringList q;
    for (const QString &a : argv) q << DesktopEntry::quoteExecArg(a);
    return q.join(' ');
}

// Pick the best icon in the extraction for `iconRef` (an icon name or path).
QString findIcon(const QString &ex, const QString &iconRef)
{
    // "Icon=" is normally a theme name ("krita"), sometimes a path or "name.png".
    QString name = iconRef.contains('/') ? QFileInfo(iconRef).fileName() : iconRef;
    name.remove(QRegularExpression("\\.(png|svg|xpm|jpg)$", QRegularExpression::CaseInsensitiveOption));
    // 1. Scalable SVG from the hicolor theme.
    if (!name.isEmpty()) {
        const QString svg = resolveInside(ex, ex + "/usr/share/icons/hicolor/scalable/apps/" + name + ".svg");
        if (!svg.isEmpty()) return svg;
    }
    // 2. The AppImage's declared icon.
    const QString dirIcon = resolveInside(ex, ex + "/.DirIcon");
    if (!dirIcon.isEmpty()) return dirIcon;
    // 3. The largest hicolor PNG of that name (folders are named NxN).
    QString best; int bestSize = 0;
    if (!name.isEmpty()) {
        QDirIterator it(ex + "/usr/share/icons", {name + ".png"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString f = it.next();
            const auto m = QRegularExpression("/(\\d+)x\\d+/").match(f);
            const int sz = m.hasMatch() ? m.captured(1).toInt() : 1;
            if (sz > bestSize && !resolveInside(ex, f).isEmpty()) { bestSize = sz; best = resolveInside(ex, f); }
        }
        if (!best.isEmpty()) return best;
        // 4. Root-level or pixmaps file of that name.
        for (const QString &c : {ex + '/' + name + ".png", ex + '/' + name + ".svg",
                                 ex + "/usr/share/pixmaps/" + name + ".png",
                                 ex + "/usr/share/pixmaps/" + name + ".svg"}) {
            const QString r = resolveInside(ex, c);
            if (!r.isEmpty()) return r;
        }
    }
    // 5. Anything image-like at the root.
    qint64 bigger = 0;
    for (const QFileInfo &fi : QDir(ex).entryInfoList({"*.png", "*.svg"}, QDir::Files)) {
        const QString r = resolveInside(ex, fi.absoluteFilePath());
        if (!r.isEmpty() && QFileInfo(r).size() > bigger) { bigger = QFileInfo(r).size(); best = r; }
    }
    return best;
}

// Image extension from content (a .DirIcon has none).
QString imageExtension(const QString &file)
{
    QFile probe(file);
    if (probe.open(QIODevice::ReadOnly)) {
        const QByteArray head = probe.read(256);
        if (head.startsWith(QByteArray("\x89PNG", 4))) return "png";
        if (head.startsWith(QByteArray("\xff\xd8", 2))) return "jpg";
        if (head.contains("<svg") || head.startsWith("<?xml")) return "svg";
    }
    const QString s = QFileInfo(file).suffix().toLower();
    return (s == "svg" || s == "jpg" || s == "jpeg" || s == "xpm") ? s : QStringLiteral("png");
}

// Extract the metadata files of an AppImage into `ex` without running it.
bool extractMetadata(const QString &appImage, const QString &ex, bool withIcons)
{
    const qint64 offset = squashfsOffset(appImage);
    if (offset < 0) return false;
    QStringList patterns = {"*.desktop", "usr/share/applications/*"};
    if (withIcons) patterns += {".DirIcon", "*.png", "*.svg",
                                "usr/share/icons/hicolor/*", "usr/share/pixmaps/*"};
    safeExtract(appImage, offset, ex, patterns);
    return QDir(ex).exists();
}

} // namespace

QString AppImageTool::versionFromFileName(const QString &fileName)
{
    static const QRegularExpression rx(
        "(?:^|[-_ ])v?(\\d+(?:\\.\\d+)+(?:[-+~]?(?:alpha|beta|rc|pre)[.-]?\\d*)?)",
        QRegularExpression::CaseInsensitiveOption);
    const auto m = rx.match(fileName);
    return m.hasMatch() ? m.captured(1) : QString();
}

QVector<AppImageEntry> AppImageTool::installed(const QString &locale)
{
    QVector<AppImageEntry> out;
    const QString folder = QDir::cleanPath(managedFolder());
    QDir apps(appsDir());
    if (!apps.exists()) return out;

    QHash<QString, int> byTarget;   // canonical AppImage path → index in out
    for (const QFileInfo &fi : apps.entryInfoList({"*.desktop"}, QDir::Files)) {
        const DesktopEntry de = DesktopEntry::read(fi.absoluteFilePath());
        if (!de.isValid()) continue;
        const QString target = launcherTarget(de);
        if (target.isEmpty()) continue;
        const QFileInfo ti(target);
        if (!ti.isFile()) continue;                    // stale launcher (file gone)

        AppImageEntry e;
        e.name         = de.localized("Name", locale);
        e.version      = de.value("X-AppImage-Version");
        if (e.version.isEmpty()) e.version = versionFromFileName(ti.fileName());
        e.comment      = de.localized("Comment", locale);
        if (e.comment.isEmpty()) e.comment = de.localized("GenericName", locale);
        e.appImagePath = target;
        e.desktopPath  = fi.absoluteFilePath();
        e.iconPath     = de.value("Icon");
        e.sizeBytes    = ti.size();
        e.byGearLever  = QDir::cleanPath(target).startsWith(folder + '/');
        if (e.name.isEmpty()) e.name = stemFromFileName(ti.fileName());

        // Several launchers can point at one AppImage (GearLever + the app's own
        // AppRun, or a leftover) — list the file once, preferring our launcher.
        const QString key = ti.canonicalFilePath();
        if (byTarget.contains(key)) {
            if (de.value("X-GT-STACER-Integrated") == "true") out[byTarget[key]] = e;
            continue;
        }
        byTarget.insert(key, out.size());
        out << e;
    }
    return out;
}

AppImageInfo AppImageTool::inspect(const QString &appImagePath)
{
    AppImageInfo info;
    const QFileInfo src(appImagePath);
    if (!src.isFile()) { info.error = "File not found."; return info; }
    if (squashfsOffset(appImagePath) < 0) { info.error = "Not a type-2 AppImage (no ELF runtime)."; return info; }
    if (!CommandUtil::commandExists("unsquashfs")) {
        info.error = "unsquashfs (squashfs-tools) is required to read AppImages.";
        return info;
    }
    QTemporaryDir tmp;
    if (!tmp.isValid()) { info.error = "Could not create a temporary folder."; return info; }
    const QString ex = tmp.path() + "/x";
    extractMetadata(appImagePath, ex, false);
    const QString desktop = findEmbeddedDesktop(ex, &info.desktopId);
    const DesktopEntry de = DesktopEntry::read(desktop);
    info.valid      = true;
    info.name       = de.value("Name");
    info.comment    = de.value("Comment");
    if (info.comment.isEmpty()) info.comment = de.value("GenericName");
    info.categories = de.value("Categories");
    info.version    = de.value("X-AppImage-Version");
    if (info.version.isEmpty()) info.version = versionFromFileName(src.fileName());
    if (info.name.isEmpty()) info.name = stemFromFileName(src.fileName());
    return info;
}

bool AppImageTool::integrate(const QString &appImagePath, QString *error, QString *launcherOut)
{
    auto fail = [error](const QString &why) { if (error) *error = why; return false; };

    const QFileInfo src(appImagePath);
    if (!src.exists() || !src.isFile()) return fail("File not found.");
    if (squashfsOffset(appImagePath) < 0)
        return fail("Not a type-2 AppImage — it can't be read safely without running it.");
    if (!CommandUtil::commandExists("unsquashfs"))
        return fail("unsquashfs (package squashfs-tools) is required to integrate AppImages.");

    QTemporaryDir tmp;
    if (!tmp.isValid()) return fail("Could not create a temporary folder.");
    const QString ex = tmp.path() + "/x";
    extractMetadata(appImagePath, ex, true);       // no AppImage code runs

    QString embeddedId;
    const QString embeddedDesktop = findEmbeddedDesktop(ex, &embeddedId);
    DesktopEntry de = DesktopEntry::read(embeddedDesktop);
    if (!de.isValid()) de.set("Type", "Application");

    // ── Identity ── the embedded desktop id is the app's real identity (and on
    // Wayland usually its app_id). Fall back to the file name without version.
    QString id = sanitizeId(embeddedId);
    if (id.isEmpty()) id = sanitizeId(stemFromFileName(src.fileName()));
    if (id.isEmpty())
        id = "app-" + QString::fromLatin1(QCryptographicHash::hash(
                 src.fileName().toUtf8(), QCryptographicHash::Sha1).toHex().left(10));

    QString name = de.value("Name");
    if (name.isEmpty()) { name = stemFromFileName(src.fileName()); de.set("Name", name); }
    QString version = de.value("X-AppImage-Version");
    if (version.isEmpty()) version = versionFromFileName(src.fileName());

    const QString folder = managedFolder();
    QDir().mkpath(folder);
    QDir().mkpath(folder + "/.icons");
    QDir().mkpath(appsDir());

    // Launcher name: "appimage-<id>.desktop". The "appimage-" prefix matters: a
    // user launcher named exactly like a system one (e.g. gt-stacer.desktop)
    // would SHADOW the distro-installed app in the menu.
    const QString launcher = appsDir() + "/appimage-" + id + ".desktop";

    // What did a previous integration of this app leave? (update = replace)
    QStringList oldTargets, oldLaunchers;
    auto noteOld = [&](const QString &desktopPath) {
        const DesktopEntry old = DesktopEntry::read(desktopPath);
        if (!old.isValid() || old.value("X-GT-STACER-Integrated") != "true") return;
        oldLaunchers << desktopPath;
        const QString t = launcherTarget(old);
        if (!t.isEmpty()) oldTargets << QDir::cleanPath(t);
    };
    if (QFileInfo::exists(launcher)) noteOld(launcher);
    {   // 26.11 launchers were named after the lowercased display name.
        QString legacy = name.toLower();
        legacy.replace(QRegularExpression("[^a-z0-9._+-]+"), "_");
        if (!legacy.isEmpty() && QFileInfo::exists(appsDir() + '/' + legacy + ".desktop"))
            noteOld(appsDir() + '/' + legacy + ".desktop");
    }

    // ── Place the file ── keep its own name; only add the extension if missing.
    QString fileName = src.fileName();
    if (!fileName.endsWith(".appimage", Qt::CaseInsensitive)) fileName += ".AppImage";
    const QString destAppImage = QDir::cleanPath(folder + '/' + fileName);
    const bool alreadyThere = QFileInfo(destAppImage).exists()
        && QFileInfo(destAppImage).canonicalFilePath() == src.canonicalFilePath();
    if (!alreadyThere) {
        // Write to a temp name first so a failed copy never destroys the old file.
        const QString part = destAppImage + ".part";
        QFile::remove(part);
        bool placed = false;
        if (moveOnIntegration()) {
            placed = QFile::rename(appImagePath, part);
            if (!placed && QFile::copy(appImagePath, part)) placed = true;  // cross-device
        } else {
            placed = QFile::copy(appImagePath, part);       // clone (keep original)
        }
        if (!placed) { QFile::remove(part); return fail("Could not copy the AppImage into " + folder + "."); }
        QFile::remove(destAppImage);
        if (!QFile::rename(part, destAppImage)) return fail("Could not place the AppImage in " + folder + ".");
        if (moveOnIntegration() && QFileInfo::exists(appImagePath)
            && QFileInfo(appImagePath).canonicalFilePath() != QFileInfo(destAppImage).canonicalFilePath())
            QFile::remove(appImagePath);                    // finish a cross-device move
    }
    QFile::setPermissions(destAppImage, QFileInfo(destAppImage).permissions()
                          | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    // ── Icon ──
    QString destIcon;
    const QString iconSrc = findIcon(ex, de.value("Icon"));
    if (!iconSrc.isEmpty()) {
        const QString dst = folder + "/.icons/" + id + '.' + imageExtension(iconSrc);
        for (const char *ext : {"png", "svg", "jpg", "jpeg", "xpm"})
            QFile::remove(folder + "/.icons/" + id + '.' + ext);
        if (QFile::copy(iconSrc, dst)) destIcon = dst;
    }

    // ── Launcher ── the embedded entry carried over, with only the keys that
    // must point at the installed copy rewritten.
    de.set("Type", de.value("Type").isEmpty() ? QStringLiteral("Application") : de.value("Type"));
    de.set("Exec", rewriteExec(de.value("Exec"), destAppImage));
    de.set("TryExec", destAppImage);
    de.removeKey("Icon");
    de.set("Icon", destIcon.isEmpty() ? QStringLiteral("application-x-executable") : destIcon);
    // DBusActivatable would launch through a D-Bus service that isn't installed;
    // Hidden means "deleted"; X-AppImage-Integrate=false is advice to *automatic*
    // integrators — the user asked for this one explicitly.
    de.removeKey("DBusActivatable");
    de.removeKey("Hidden");
    de.removeKey("X-AppImage-Integrate");
    // Lets the taskbar/dock match the running window to this launcher (its icon
    // and pinning). Most apps report their desktop id as WM_CLASS / app_id.
    if (de.value("StartupWMClass").isEmpty() && !embeddedId.isEmpty())
        de.set("StartupWMClass", embeddedId);
    de.set("X-AppImage-Version", version);             // GearLever marker key
    de.set("X-GT-STACER-Integrated", "true");
    de.set("X-GT-STACER-Id", id);
    for (auto &g : de.groups())                         // [Desktop Action …] entries
        if (g.name.startsWith("Desktop Action ") && de.contains("Exec", g.name))
            de.set("Exec", rewriteExec(de.value("Exec", g.name), destAppImage), g.name);

    QSaveFile df(launcher);
    if (!df.open(QIODevice::WriteOnly)) return fail("Could not write the launcher " + launcher + ".");
    df.write(de.serialize());
    if (!df.commit()) return fail("Could not write the launcher " + launcher + ".");
    QFile::setPermissions(launcher, QFileInfo(launcher).permissions() | QFileDevice::ExeOwner);

    // ── Replace the previous version ── only files we integrated, in our folder.
    const QString cleanFolder = QDir::cleanPath(folder);
    for (const QString &t : oldTargets)
        if (t != destAppImage && t.startsWith(cleanFolder + '/') && QFileInfo(t).isFile())
            QFile::remove(t);
    for (const QString &l : oldLaunchers)
        if (QDir::cleanPath(l) != QDir::cleanPath(launcher)) QFile::remove(l);

    CommandUtil::execProgram("update-desktop-database", {appsDir()}, 15000);
    if (launcherOut) *launcherOut = launcher;
    return true;
}

bool AppImageTool::remove(const AppImageEntry &entry)
{
    const QString h = QDir::cleanPath(home());
    bool any = false;

    const QString ai = QDir::cleanPath(entry.appImagePath);
    const QString canonical = QFileInfo(ai).canonicalFilePath();

    // Every launcher pointing at this AppImage (ours, GearLever's, the app's own).
    QStringList launchers;
    const QString apps = QDir::cleanPath(appsDir());
    const QString dt = QDir::cleanPath(entry.desktopPath);
    if (dt.startsWith(apps + '/') && dt.endsWith(".desktop") && QFileInfo(dt).isFile())
        launchers << dt;
    if (!canonical.isEmpty())
        for (const QFileInfo &fi : QDir(apps).entryInfoList({"*.desktop"}, QDir::Files)) {
            const QString t = launcherTarget(DesktopEntry::read(fi.absoluteFilePath()));
            if (!t.isEmpty() && QFileInfo(t).canonicalFilePath() == canonical
                && !launchers.contains(fi.absoluteFilePath()))
                launchers << fi.absoluteFilePath();
        }

    // AppImage file — must be a .AppImage under the user's home.
    if (ai.endsWith(".appimage", Qt::CaseInsensitive) && ai.startsWith(h + '/')
        && QFileInfo(ai).isFile())
        any |= QFile::remove(ai);

    for (const QString &l : launchers) QFile::remove(l);

    // Icon — only an extracted one under the user's home (".icons" folder).
    const QString ic = QDir::cleanPath(entry.iconPath);
    if (ic.startsWith(h + '/') && ic.contains("/.icons/") && QFileInfo(ic).isFile())
        QFile::remove(ic);

    CommandUtil::execProgram("update-desktop-database", {appsDir()}, 15000);
    return any;
}
