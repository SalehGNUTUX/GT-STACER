#pragma once
#include <QWidget>
#include <QVector>
#include "../../../gt-stacer-core/Tools/snapshot_tool.h"

class QGroupBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QLineEdit;
class QProgressBar;
class QCheckBox;
class QComboBox;
class QProcess;
class LoadingOverlay;

// Backup & Snapshots (26.09). Two independent sections, each shown only when its
// backend exists:
//   • Snapshots — system restore points via Timeshift (or ZFS). List/create/
//     delete/restore run through pkexec (long ones on a worker thread + overlay).
//   • Home backup — an rsync mirror of the home directory to a chosen folder,
//     with live progress and a Cancel button (no root — it's your own files).
class BackupPage : public QWidget {
    Q_OBJECT
public:
    explicit BackupPage(QWidget *parent = nullptr);
    ~BackupPage() override;

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onBackendChanged();
    void loadSnapshots();
    void createSnapshot();
    void deleteSnapshot();
    void restoreSnapshot();
    void browseDest();
    void startBackup();
    void cancelBackup();

private:
    void buildSnapshotSection();
    void buildBackupSection();
    void setBackupRunning(bool running);
    void updateSnapStatus();

    // Snapshots
    QGroupBox    *m_snapBox     = nullptr;
    QComboBox    *m_engineCombo = nullptr;
    QLabel       *m_snapBadge   = nullptr;
    QLabel       *m_snapStatus  = nullptr;
    QTableWidget *m_snapTable   = nullptr;
    QLineEdit    *m_snapComment = nullptr;
    QPushButton  *m_snapLoad    = nullptr;
    QPushButton  *m_snapCreate  = nullptr;
    QPushButton  *m_snapDelete  = nullptr;
    QPushButton  *m_snapRestore = nullptr;
    LoadingOverlay *m_overlay   = nullptr;
    QVector<Snapshot> m_snaps;

    // Home backup
    QGroupBox    *m_backupBox   = nullptr;
    QLineEdit    *m_dest        = nullptr;
    QCheckBox    *m_mirror      = nullptr;
    QPushButton  *m_backupBtn   = nullptr;
    QPushButton  *m_cancelBtn   = nullptr;
    QProgressBar *m_progress    = nullptr;
    QLabel       *m_backupStatus= nullptr;
    QProcess     *m_proc        = nullptr;
};
