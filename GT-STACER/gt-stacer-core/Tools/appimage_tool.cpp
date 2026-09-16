#include "appimage_tool.h"
#include "../Utils/command_util.h"
#include <QDir>
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

// The Exec target of a launcher, minus field codes / quotes / interpreter prefix.
QString execTarget(const QString &exec)
{
    for (QString t : exec.trimmed().split(' ', Qt::SkipEmptyParts)) {
        if (t.startsWith('"') || t.startsWith('\'')) t = t.mid(1);
        if (t.endsWith('"')   || t.endsWith('\''))   t.chop(1);
        if (t.isEmpty() || t.startsWith('%')) continue;
        if (t.contains('=') && !t.startsWith('/')) continue;
        if (t == "env" || t == "sh" || t == "bash" || t == "/usr/bin/env") continue;
        return t;
    }
    return {};
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

QVector<AppImageEntry> AppImageTool::installed()
{
    QVector<AppImageEntry> out;
    const QString folder = QDir::cleanPath(managedFolder());
    QDir apps(appsDir());
    if (!apps.exists()) return out;

    for (const QFileInfo &fi : apps.entryInfoList({"*.desktop"}, QDir::Files)) {
        QSettings d(fi.absoluteFilePath(), QSettings::IniFormat);
        d.beginGroup("Desktop Entry");
        const QString exec = d.value("Exec").toString();
        const QString tryExec = d.value("TryExec").toString();
        QString target = tryExec.isEmpty() ? execTarget(exec) : tryExec;
        // Only launchers that point at an actual .AppImage file.
        if (!target.endsWith(".appimage", Qt::CaseInsensitive)) { d.endGroup(); continue; }
        if (!QFileInfo::exists(target))                          { d.endGroup(); continue; }

        AppImageEntry e;
        e.name         = d.value("Name").toString();
        e.version      = d.value("X-AppImage-Version").toString();
        e.appImagePath = target;
        e.desktopPath  = fi.absoluteFilePath();
        e.iconPath     = d.value("Icon").toString();
        e.sizeBytes    = QFileInfo(target).size();
        e.byGearLever  = QDir::cleanPath(target).startsWith(folder + '/');
        d.endGroup();
        if (e.name.isEmpty()) e.name = QFileInfo(target).completeBaseName();
        out << e;
    }
    return out;
}

bool AppImageTool::integrate(const QString &appImagePath)
{
    const QFileInfo src(appImagePath);
    if (!src.exists() || !src.isFile()) return false;

    const qint64 offset = squashfsOffset(appImagePath);
    if (offset < 0) return false;   // not a type-2 AppImage we can read safely

    QTemporaryDir tmp;
    if (!tmp.isValid()) return false;
    const QString ex = tmp.path() + "/x";
    // Extract just the metadata files — the root desktop/icon (often symlinks),
    // their real targets under usr/share, and any pixmaps. No AppImage code runs.
    safeExtract(appImagePath, offset, ex, {
        "*.desktop", ".DirIcon", "*.png", "*.svg",
        "usr/share/applications/*", "usr/share/icons/*", "usr/share/pixmaps/*"});

    QDir exd(ex);
    // Find the real .desktop: prefer usr/share/applications, else any *.desktop
    // that is a regular file with a [Desktop Entry]. (Root entries are symlinks.)
    QString desktopFile;
    for (const QString &cand : QStringList{ex + "/usr/share/applications", ex}) {
        for (const QFileInfo &fi : QDir(cand).entryInfoList({"*.desktop"}, QDir::Files)) {
            const QString real = fi.isSymLink() ? fi.symLinkTarget() : fi.absoluteFilePath();
            if (QFileInfo(real).isFile()) { desktopFile = real; break; }
        }
        if (!desktopFile.isEmpty()) break;
    }

    QString desktopName, iconRef, categories, execArgs;
    bool terminal = false;
    if (!desktopFile.isEmpty()) {
        QSettings d(desktopFile, QSettings::IniFormat);
        d.beginGroup("Desktop Entry");
        desktopName = d.value("Name").toString();
        iconRef     = d.value("Icon").toString();
        categories  = d.value("Categories").toString();
        terminal    = d.value("Terminal").toString().compare("true", Qt::CaseInsensitive) == 0;
        d.endGroup();
    }
    if (desktopName.isEmpty()) desktopName = src.completeBaseName();

    // GearLever-compatible filenames: lowercased, spaces→underscores.
    QString base = desktopName.toLower();
    base.replace(QRegularExpression("[^a-z0-9._+-]+"), "_");
    if (base.isEmpty()) base = "appimage";

    const QString folder = managedFolder();
    QDir().mkpath(folder);
    QDir().mkpath(folder + "/.icons");
    QDir().mkpath(appsDir());

    // Move or clone into the folder, honouring GearLever's configured preference.
    const QString destAppImage = folder + '/' + base + ".AppImage";
    QFile::remove(destAppImage);
    bool placed;
    if (moveOnIntegration()) {
        placed = QFile::rename(appImagePath, destAppImage);   // move (rename)
        if (!placed && QFile::copy(appImagePath, destAppImage)) {   // cross-device: copy+delete
            QFile::remove(appImagePath);
            placed = true;
        }
    } else {
        placed = QFile::copy(appImagePath, destAppImage);     // clone (keep original)
    }
    if (!placed) return false;
    QFile::setPermissions(destAppImage, QFileInfo(destAppImage).permissions()
                          | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    // Icon: prefer the embedded .DirIcon, else the icon the .desktop names, else
    // the largest extracted image. Best-effort — a missing icon is not fatal.
    // The image type is detected from content (the .DirIcon has no extension).
    QString destIcon;
    auto pickIcon = [&](const QString &candidate) -> QString {
        if (candidate.isEmpty() || !QFileInfo::exists(candidate)) return {};
        QString extn = "png";
        QFile probe(candidate);
        if (probe.open(QIODevice::ReadOnly)) {
            const QByteArray head = probe.read(64);
            if      (head.startsWith(QByteArray("\x89PNG", 4))) extn = "png";
            else if (head.contains("<svg") || head.startsWith("<?xml")) extn = "svg";
            else if (head.startsWith(QByteArray("\xff\xd8", 2))) extn = "jpg";
            else {
                const QString s = QFileInfo(candidate).suffix().toLower();
                if (s == "png" || s == "svg" || s == "jpg" || s == "jpeg" || s == "xpm") extn = s;
            }
        }
        const QString dst = folder + "/.icons/" + base + '.' + extn;
        QFile::remove(dst);
        return QFile::copy(candidate, dst) ? dst : QString();
    };
    destIcon = pickIcon(ex + "/.DirIcon");
    if (destIcon.isEmpty() && !iconRef.isEmpty()) {
        for (const QString &e : {ex + "/" + iconRef + ".png", ex + "/" + iconRef + ".svg", ex + "/" + iconRef}) {
            destIcon = pickIcon(e); if (!destIcon.isEmpty()) break;
        }
    }
    if (destIcon.isEmpty()) {  // largest png/svg anywhere in the extraction
        qint64 best = 0;
        for (const QFileInfo &fi : exd.entryInfoList({"*.png","*.svg"}, QDir::Files))
            if (fi.size() > best) { best = fi.size(); destIcon = pickIcon(fi.absoluteFilePath()); }
    }

    // Write the launcher as a proper freedesktop .desktop file. (QSettings is NOT
    // used here — its IniFormat percent-encodes the "[Desktop Entry]" group name
    // and quotes values, producing an invalid launcher.) Same fields GearLever
    // writes, so GearLever recognises the app too.
    const QString exec = categories.isEmpty()
        ? QString("\"%1\"").arg(destAppImage)
        : QString("\"%1\"").arg(destAppImage);
    QString cats = categories;
    if (!cats.isEmpty() && !cats.endsWith(';')) cats += ';';

    QString content = "[Desktop Entry]\n";
    content += "Type=Application\n";
    content += "Name=" + desktopName + "\n";
    content += "Exec=" + exec + "\n";
    content += "TryExec=" + destAppImage + "\n";
    content += "Icon=" + (destIcon.isEmpty() ? QStringLiteral("application-x-executable") : destIcon) + "\n";
    content += QString("Terminal=") + (terminal ? "true" : "false") + "\n";
    if (!cats.isEmpty()) content += "Categories=" + cats + "\n";
    content += "X-AppImage-Version=" + QString() + "\n";   // GearLever marker key
    content += "X-GT-STACER-Integrated=true\n";

    const QString desktopPath = appsDir() + '/' + base + ".desktop";
    QFile df(desktopPath);
    if (!df.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    df.write(content.toUtf8());
    df.close();
    QFile::setPermissions(desktopPath, QFileInfo(desktopPath).permissions() | QFileDevice::ExeOwner);

    CommandUtil::execProgram("update-desktop-database", {appsDir()}, 15000);
    return true;
}

bool AppImageTool::remove(const AppImageEntry &entry)
{
    const QString h = QDir::cleanPath(home());
    bool any = false;

    // AppImage file — must be a .AppImage under the user's home.
    const QString ai = QDir::cleanPath(entry.appImagePath);
    if (ai.endsWith(".appimage", Qt::CaseInsensitive) && ai.startsWith(h + '/')
        && QFileInfo(ai).isFile())
        any |= QFile::remove(ai);

    // Launcher — must live in the applications dir.
    const QString dt = QDir::cleanPath(entry.desktopPath);
    if (dt.startsWith(QDir::cleanPath(appsDir()) + '/') && dt.endsWith(".desktop")
        && QFileInfo(dt).isFile())
        QFile::remove(dt);

    // Icon — must be under the managed folder's .icons (or home) and a real file.
    const QString ic = QDir::cleanPath(entry.iconPath);
    if (ic.startsWith(h + '/') && ic.contains("/.icons/") && QFileInfo(ic).isFile())
        QFile::remove(ic);

    CommandUtil::execProgram("update-desktop-database", {appsDir()}, 15000);
    return any;
}
