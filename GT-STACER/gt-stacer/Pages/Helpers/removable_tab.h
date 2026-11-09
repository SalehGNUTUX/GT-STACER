#pragma once
#include <QWidget>
#include <QString>
#include <QVector>
#include "../../../gt-stacer-core/Tools/removable_tool.h"

class QTreeWidget;
class QLabel;
class QPushButton;
class QProgressBar;
class QProcess;
class QTimer;

// Helpers → "USB & memory cards" (26.12): the native replacement for gt-usdr.
// Check / repair / format USB sticks and SD cards, read their health, scan
// their surface and back them up to an image — through UDisks2 (no password
// for removable media in a normal session), with plain-language results.
// Internal disks are never listed; a removable disk holding the system or
// active swap is shown locked. See TOOLS-REVIEW/gt-usdr.md.
class RemovableTab : public QWidget {
    Q_OBJECT
public:
    explicit RemovableTab(QWidget *parent = nullptr);
    ~RemovableTab() override;

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onUdisksChanged();               // hot-plug (UDisks2 ObjectManager signal)

private:
    // What the current row stands for.
    struct Target {
        int     disk = -1;                // index in m_disks
        QString device;                   // disk or partition device
        bool    isDisk = false;
        RemovableTool::Part part;         // when a partition (or whole-device fs) is the target
        bool    hasPart = false;
    };

    void retranslate();
    void refresh();
    void populate();
    Target target() const;
    void updateButtons();
    void setBusy(bool busy, bool stoppable = false);
    QString lockedText(const QString &reason) const;
    // "" if the device may be changed now; else a message (and it was shown).
    bool verifyOrExplain(const Target &t);

    void toggleMount();
    void openFolder();
    void checkOrRepair(bool repair);
    void showHealth();
    void surfaceScan();
    void backupImage();
    void formatTarget();

    // Long root jobs (surface scan, image backup) — guarded, stoppable.
    void startJob(const QStringList &argv, const QString &label, const QString &outFile);
    void stopJob();

    QLabel       *m_intro      = nullptr;
    QLabel       *m_empty      = nullptr;
    QTreeWidget  *m_tree       = nullptr;
    QPushButton  *m_mountBtn   = nullptr;
    QPushButton  *m_openBtn    = nullptr;
    QPushButton  *m_checkBtn   = nullptr;
    QPushButton  *m_repairBtn  = nullptr;
    QPushButton  *m_healthBtn  = nullptr;
    QPushButton  *m_scanBtn    = nullptr;
    QPushButton  *m_backupBtn  = nullptr;
    QPushButton  *m_formatBtn  = nullptr;
    QPushButton  *m_refreshBtn = nullptr;
    QProgressBar *m_progress   = nullptr;
    QPushButton  *m_stopBtn    = nullptr;
    QLabel       *m_status     = nullptr;
    QTimer       *m_debounce   = nullptr;

    QVector<RemovableTool::Disk> m_disks;
    bool          m_busy = false;
    bool          m_dirty = false;        // devices changed while hidden

    // Current long job
    QProcess     *m_proc = nullptr;
    enum class Job { None, Scan, Backup } m_job = Job::None;
    bool          m_stopRequested = false;
    QString       m_jobLabel;
    QString       m_jobOutFile;           // image file being written (deleted if not complete)
    qint64        m_jobSize = 0;          // bytes expected (backup)
    QString       m_jobOutput;
    int           m_badBlocks = 0;
    QStringList   m_jobUnmounted;         // partitions the job unmounted → mounted back afterwards
};
