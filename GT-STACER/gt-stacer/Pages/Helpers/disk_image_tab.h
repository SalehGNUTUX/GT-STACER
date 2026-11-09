#pragma once
#include <QWidget>
#include <QStringList>
#include <QVector>
#include "../../../gt-stacer-core/Tools/disk_image_tool.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QCheckBox;
class QProgressBar;
class QProcess;
class QComboBox;
class QTimer;
class QFrame;

// Helpers → "Disk images" (26.12): the native replacement for GT-IMT.
// Mount ISO / IMG images (no password, via udisks), browse, unmount, and
// extract them to a folder — with progress, Stop, and a plain-language reason
// for every failure. See TOOLS-REVIEW/gt-imt.md for what it fixes.
//
// Nothing here ever deletes user files: "overwrite" replaces same-name files
// only, "keep" skips them, and the extraction always goes into a NEW subfolder
// named after the image unless the user chose to merge into an existing one.
class DiskImageTab : public QWidget {
    Q_OBJECT
public:
    explicit DiskImageTab(QWidget *parent = nullptr);
    ~DiskImageTab() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;   // drag & drop onto the list
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void retranslate();
    void refresh();                       // re-read the attached images
    void populate();                      // rebuild the list from m_images
    void updateButtons();
    void chooseAndMount();
    void mountImage(const QString &path);
    void unmountSelected();
    void openSelected();
    void chooseAndExtract();
    void extractImage(const QString &path);

    // The attached image the current row belongs to (-1 = none).
    int  selectedImage() const;
    QString selectedMountPoint() const;

    // Explain why an image can't be mounted/extracted (compressed, VM disk …).
    // Returns true when it explained (= the caller must stop).
    bool explainUnsupported(const DiskImageTool::Info &info, const QString &path);

    // Extraction runs as a queue of guarded commands (one per partition).
    struct Step { QString label; QStringList argv; };
    void runNextStep();
    void finishExtraction(bool ok, const QString &message);
    void stopExtraction();
    void releaseExtractMounts();          // unmount what WE mounted for the extraction
    void setBusy(bool busy);
    void showWait(const QString &title, const QString &detail);

    // Optical discs (CD/DVD/Blu-ray in a drive): detect, open, eject, and save
    // the disc as a local .iso file (dd, exact size, Stop, never a half file).
    void pollDiscs();                     // re-read the drives in the background
    void showDiscs();                     // update the disc row from m_drives
    int  currentDrive() const;            // index into m_drives (-1 = none)
    bool explainDisc(const DiskImageTool::OpticalDrive &d);   // true = can't be copied (explained)
    void saveDisc();
    void startDiscCopy(const DiskImageTool::OpticalDrive &d, qint64 bytes, const QString &dest);
    void finishDiscCopy(bool ok, const QString &message);
    void openDisc();
    void ejectDisc();

    // Burning an ISO onto a blank (or erasable) disc, then verifying it.
    void burnIso();
    void startBurn(const QString &device, const QString &image, qint64 bytes, bool blankFirst, bool verify);
    void startVerify();
    void finishBurn(bool ok, const QString &message, const QString &details = {});

    QLabel       *m_intro      = nullptr;
    QTreeWidget  *m_tree       = nullptr;
    QLabel       *m_status     = nullptr;
    QPushButton  *m_mountBtn   = nullptr;
    QPushButton  *m_extractBtn = nullptr;
    QPushButton  *m_openBtn    = nullptr;
    QPushButton  *m_unmountBtn = nullptr;
    QPushButton  *m_refreshBtn = nullptr;
    QCheckBox    *m_readOnly   = nullptr;
    QCheckBox    *m_overwrite  = nullptr;
    QProgressBar *m_progress   = nullptr;
    QPushButton  *m_stopBtn    = nullptr;

    QFrame       *m_discRow    = nullptr;
    QLabel       *m_discLabel  = nullptr;
    QComboBox    *m_discCombo  = nullptr;   // shown only with more than one drive
    QPushButton  *m_discSaveBtn  = nullptr;
    QPushButton  *m_discOpenBtn  = nullptr;
    QPushButton  *m_discEjectBtn = nullptr;
    QPushButton  *m_discBurnBtn  = nullptr;
    QTimer       *m_discTimer  = nullptr;
    QVector<DiskImageTool::OpticalDrive> m_drives;
    bool          m_discPolling = false;

    QVector<DiskImageTool::Attached> m_images;   // what the list shows
    bool          m_busy = false;                // a mount/unmount/extraction is running

    // Current extraction
    QProcess     *m_proc = nullptr;
    QVector<Step> m_steps;
    int           m_stepIdx = 0;
    bool          m_stopRequested = false;
    QString       m_output;            // current step's output (for explaining a failure)
    QString       m_program;           // current step's real program (7z, rsync …)
    QString       m_extractDest;
    QString       m_extractImage;
    QVector<DiskImageTool::Attached> m_mountedForExtract;   // unmount these afterwards
    int           m_warnings = 0;

    // Current disc copy (m_proc is shared with extraction — only one runs)
    bool          m_discCopy = false;
    QString       m_discPart;          // "<dest>.part" being written
    QString       m_discDest;          // final .iso path
    qint64        m_discBytes = 0;
    QString       m_discName;          // "DVD-R “LABEL”" for messages

    // Current burn (also on m_proc)
    enum class Burn { None, Writing, Verifying };
    Burn          m_burn = Burn::None;
    QString       m_burnDevice;
    QString       m_burnImage;
    qint64        m_burnBytes = 0;
    bool          m_burnVerify = true;
};
