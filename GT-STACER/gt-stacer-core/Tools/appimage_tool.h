#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

// AppImage integration, GearLever-compatible. "Integrating" an AppImage means
// placing it in a managed folder, making it executable, extracting its icon and
// embedded desktop entry, and writing a launcher in ~/.local/share/applications
// so it shows up in the application menu — what GearLever (it.mijorus.gearlever)
// does. We use the SAME managed folder and an Exec pointing at the AppImage plus
// an X-AppImage-Version key, so that:
//   • apps integrated by GearLever (or by an AppImage's own AppRun) are listed
//     and manageable here, and
//   • apps we integrate are recognised by GearLever,
// with no duplication or conflict between the two tools.
//
// 26.12: the embedded .desktop is now carried over FAITHFULLY (localized names,
// GenericName, Comment, Keywords, MimeType, Categories, StartupWMClass, desktop
// actions, %U/%F field codes) instead of being reduced to Name/Icon/one category,
// and the AppImage keeps its own file name.
struct AppImageEntry {
    QString name;          // display name (localized when a locale is given)
    QString version;       // X-AppImage-Version, else parsed from the file name
    QString comment;       // Comment / GenericName, if any
    QString appImagePath;  // the .AppImage file
    QString desktopPath;   // its launcher in ~/.local/share/applications
    QString iconPath;      // extracted icon (absolute path) or an icon-theme name
    qint64  sizeBytes = 0;
    bool    byGearLever = false;  // the AppImage lives in the managed (GearLever) folder
};

// Metadata read from an AppImage WITHOUT executing it (see integrate()).
struct AppImageInfo {
    bool    valid = false;      // a type-2 AppImage we could read
    QString name;
    QString version;
    QString comment;
    QString categories;
    QString desktopId;          // embedded .desktop basename, e.g. "org.kde.krita"
    QString error;
};

class AppImageTool {
public:
    // The folder where AppImages are stored — GearLever's
    // `appimages-default-folder` gsetting if readable, else its default ~/AppImages.
    static QString managedFolder();
    // Is GearLever itself installed (Flatpak or native)? (Informational — we work
    // with or without it.)
    static bool gearLeverInstalled();

    // Every integrated AppImage: launchers under ~/.local/share/applications whose
    // Exec/TryExec resolves to an existing .AppImage file. Catches GearLever's,
    // self-integrating AppImages' and ours. One entry per AppImage file.
    // `locale` (e.g. "ar_MA") picks the localized Name when the launcher has one.
    static QVector<AppImageEntry> installed(const QString &locale = QString());

    // Read name/version/comment from an AppImage without running it.
    static AppImageInfo inspect(const QString &appImagePath);

    // Integrate an AppImage (GearLever-style): place it in the managed folder
    // (move or copy per GearLever's preference), chmod +x, extract the icon and
    // the embedded desktop entry, write the launcher. Re-integrating a newer
    // build of the same app replaces the previous one. On failure returns false
    // and, if given, sets *error to an English reason. No shell is involved.
    static bool integrate(const QString &appImagePath, QString *error = nullptr,
                          QString *launcherPath = nullptr);

    // Remove an integrated AppImage: its .AppImage file, every launcher pointing
    // at it, and its extracted icon. Hard-guarded to paths under the user's home.
    static bool remove(const AppImageEntry &entry);

    // Version-looking part of an AppImage file name ("app-1.2.3-x86_64" → "1.2.3").
    static QString versionFromFileName(const QString &fileName);
};
