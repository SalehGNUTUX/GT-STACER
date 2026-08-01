#pragma once
#include <QString>
#include <QVector>

// GUI front-end for PhotoRec (the testdisk suite) — recovers lost/deleted files
// by carving raw devices, partitions or disk images by signature. PhotoRec has a
// scriptable /cmd mode we drive non-interactively. Reading raw devices needs
// root, so the actual run goes through pkexec (done by the page). This class
// enumerates recovery sources and builds the argv.
struct RecoverySource {
    QString path;        // /dev/sda2  (or an image file path)
    QString size;        // "332.8G"
    QString fstype;      // "ext4", "" …
    QString mountpoint;  // "" when not mounted
    QString model;       // disk model (for whole disks)
    bool    isDisk = false;   // whole disk vs partition
    bool    mounted() const { return !mountpoint.isEmpty(); }
    QString label() const;    // human one-liner for the combo
};

// A PhotoRec file family the user can pick (id is the token PhotoRec's fileopt
// menu uses — usually the extension).
struct FileFamily {
    QString id;      // "jpg", "pdf", …
    QString label;   // "JPEG image"
    QString group;   // "Images", "Documents", "Audio", "Video", "Archives", "Other"
};

class RecoveryTool {
public:
    static bool available();                       // photorec installed
    static QVector<RecoverySource> sources();      // partitions + whole disks (lsblk)
    static QVector<FileFamily> fileFamilies();     // curated common types for the picker

    // Where recovered files land under `destDir` (PhotoRec appends .1/.2/…).
    static QString recupBase(const QString &destDir);
    // Non-interactive PhotoRec argv. `enableIds` empty → recover every family;
    // otherwise recover only those families. Run as: pkexec photorec <args>.
    static QStringList photorecArgs(const QString &source, const QString &destDir,
                                    const QStringList &enableIds = {});
    // After recovery, move files from the recup dirs into destDir/by-type/<EXT>/.
    // Returns the number of files organised.
    static int organizeByType(const QString &destDir);
};
