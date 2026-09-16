#include "store_addon_tool.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QStandardPaths>
#include <QUrl>
#include <QSet>

namespace {

struct ContentDir { QString rel; QString category; };

// Known OCS/KNewStuff install locations, relative to the user's home. Each
// directory holds one sub-directory per installed item.
QVector<ContentDir> contentDirs()
{
    return {
        {".local/share/plasma/desktoptheme",   QStringLiteral("Plasma Theme")},
        {".local/share/plasma/plasmoids",       QStringLiteral("Plasmoid")},
        {".local/share/plasma/look-and-feel",   QStringLiteral("Global Theme")},
        {".local/share/plasma/wallpapers",      QStringLiteral("Wallpaper Plugin")},
        {".local/share/aurorae/themes",         QStringLiteral("Window Decoration")},
        {".local/share/color-schemes",          QStringLiteral("Colour Scheme")},
        {".local/share/wallpapers",             QStringLiteral("Wallpaper")},
        {".local/share/icons",                  QStringLiteral("Icons")},
        {".icons",                              QStringLiteral("Icons")},
        {".local/share/themes",                 QStringLiteral("Desktop Theme")},
        {".themes",                             QStringLiteral("Desktop Theme")},
        {".local/share/fonts",                  QStringLiteral("Font")},
        {".fonts",                              QStringLiteral("Font")},
        {".local/share/konsole",                QStringLiteral("Konsole Profile")},
        {".local/share/kwin/effects",           QStringLiteral("KWin Effect")},
        {".local/share/kwin/scripts",           QStringLiteral("KWin Script")},
        {".local/share/kwin/tabbox",            QStringLiteral("KWin Switcher")},
        {".local/share/sddm/themes",            QStringLiteral("SDDM Theme")},
    };
}

qint64 dirSize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); total += it.fileInfo().size(); }
    return total;
}

} // namespace

QVector<StoreAddon> StoreAddonTool::installed()
{
    QVector<StoreAddon> out;
    const QString home = QDir::homePath();
    QSet<QString> seen;   // a path can be reachable via two rel entries (.icons)

    for (const ContentDir &cd : contentDirs()) {
        const QString base = home + '/' + cd.rel;
        QDir d(base);
        if (!d.exists()) continue;
        for (const QFileInfo &fi : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString path = fi.absoluteFilePath();
            if (seen.contains(path)) continue;
            seen.insert(path);
            StoreAddon a;
            a.name      = fi.fileName();
            a.category  = cd.category;
            a.path      = path;
            a.sizeBytes = dirSize(path);
            out << a;
        }
    }
    return out;
}

bool StoreAddonTool::remove(const StoreAddon &addon)
{
    const QString path = QDir::cleanPath(addon.path);
    if (path.isEmpty()) return false;

    const QString home = QDir::cleanPath(QDir::homePath());
    // The item must live strictly *inside* one of the known content dirs — never
    // be the content dir itself, home, or any shared root. This mirrors the
    // hard guard used for manual-app removal in PackageTool.
    bool inside = false;
    for (const ContentDir &cd : contentDirs()) {
        const QString base = QDir::cleanPath(home + '/' + cd.rel);
        if (path.startsWith(base + '/') && path != base) { inside = true; break; }
    }
    if (!inside) return false;
    if (!path.startsWith(home + '/'))          return false;  // must be under home
    if (path.count('/') < 4)                    return false;  // too shallow to be an item
    if (!QFileInfo(path).isDir())               return false;

    return QDir(path).removeRecursively();
}

bool StoreAddonTool::ocsHandlerAvailable()
{
    // ocs-url is the common handler for ocs-url:// links; Discover/plasma-discover
    // also registers one. We treat either as "installable".
    return CommandUtil::commandExists("ocs-url")
        || CommandUtil::commandExists("plasma-discover");
}

bool StoreAddonTool::installFromOcs(const QString &ocsUrl)
{
    // Accept only well-formed ocs-url / ocs-userpackage links, then pass the URL
    // as a single argv element (no shell). This never interpolates into a string.
    const QUrl u(ocsUrl);
    const QString scheme = u.scheme().toLower();
    if (!u.isValid() || (scheme != "ocs-url" && scheme != "ocs-userpackage"
                         && scheme != "ocss-url"))
        return false;

    if (CommandUtil::commandExists("ocs-url"))
        return CommandUtil::execProgram("ocs-url", {ocsUrl}, 300000) == 0;
    // Fall back to the desktop's URL handler.
    return CommandUtil::execProgram("xdg-open", {ocsUrl}, 15000) == 0;
}

QString StoreAddonTool::storeUrl()
{
    return QStringLiteral("https://www.opendesktop.org/");
}
