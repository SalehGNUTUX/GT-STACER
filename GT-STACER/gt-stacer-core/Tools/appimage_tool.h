#pragma once
#include <QString>
#include <QVector>

// AppImage integration, GearLever-compatible. "Integrating" an AppImage means
// moving it into a managed folder, making it executable, extracting its icon and
// embedded desktop entry, and writing a launcher in ~/.local/share/applications
// so it shows up in the application menu — exactly what GearLever
// (it.mijorus.gearlever) does. We deliberately use the SAME managed folder and
// the SAME launcher format (Exec pointing at the AppImage in the folder, an
// X-AppImage-Version key), so that:
//   • apps integrated by GearLever are listed and manageable here, and
//   • apps we integrate are recognised by GearLever,
// with no duplication or conflict between the two tools.
struct AppImageEntry {
    QString name;          // display name (from the embedded .desktop)
    QString version;       // X-AppImage-Version, if present
    QString appImagePath;  // the .AppImage file in the managed folder
    QString desktopPath;   // its launcher in ~/.local/share/applications
    QString iconPath;      // extracted icon, if any
    qint64  sizeBytes = 0;
    bool    byGearLever = false;  // launcher lives in GearLever's default folder
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
    // Exec resolves to an existing .AppImage file. Catches GearLever's and ours.
    static QVector<AppImageEntry> installed();

    // Integrate an AppImage file (GearLever-style): move into the managed folder,
    // chmod +x, extract icon + desktop entry, write the launcher. Returns false on
    // failure (bad file, extraction failed…). No shell; the path is passed as argv.
    static bool integrate(const QString &appImagePath);

    // Remove an integrated AppImage: its .AppImage file, its launcher and its icon.
    // Hard-guarded to the managed folder / applications dir under the user's home.
    static bool remove(const AppImageEntry &entry);
};
