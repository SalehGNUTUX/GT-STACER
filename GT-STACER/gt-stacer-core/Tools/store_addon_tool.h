#pragma once
#include <QString>
#include <QVector>

// Manages desktop "store" add-ons — the OCS / KNewStuff content from
// opendesktop.org / store.kde.org (Plasma themes, icon packs, cursors,
// plasmoids, window decorations, wallpapers, colour schemes, GTK themes,
// fonts…). There is no unified CLI package manager for this content: each type
// installs into a well-known directory under the user's home. This tool
// enumerates what is installed there, removes an item (a guarded directory
// delete — never a shared root), and installs new content by handing an
// `ocs-url://` link to the system handler, or by opening the store in a browser.
struct StoreAddon {
    QString name;        // the item (directory) name
    QString category;    // human category: "Plasma Theme", "Icons", …
    QString path;        // absolute path of the item directory
    qint64  sizeBytes = 0;
};

class StoreAddonTool {
public:
    // All OCS/KNewStuff items installed under the user's home, across categories.
    static QVector<StoreAddon> installed();

    // Delete an installed item. Hard-guarded: the path must sit at least one
    // level *inside* a known content directory (never the content dir itself,
    // never a shared/system root). Returns false rather than delete anything
    // unsafe. No shell, no root — this is the user's own ~/.local/share.
    static bool remove(const StoreAddon &addon);

    // Is a handler for `ocs-url://` / `ocs-userpackage://` links present?
    static bool ocsHandlerAvailable();
    // Hand an ocs-url link to the installed handler. Validated + no shell.
    static bool installFromOcs(const QString &ocsUrl);

    // The store website, for the "open store" fallback.
    static QString storeUrl();
};
