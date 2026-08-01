#pragma once
#include <QWidget>
#include <QVector>
#include "../../../gt-stacer-core/Tools/recovery_tool.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QProcess;
class QTimer;
class QCheckBox;
class QListWidget;

// File-recovery page — a graphical front-end for PhotoRec. Pick a source
// (partition / disk / image) and a destination on a DIFFERENT disk, then carve.
// PhotoRec needs root (raw device reads), so the run goes through pkexec.
class RecoveryPage : public QWidget {
    Q_OBJECT
public:
    explicit RecoveryPage(QWidget *parent = nullptr);
    ~RecoveryPage() override;

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void refreshSources();
    void browseDest();
    void startRecovery();
    void cancelRecovery();
    void openFolder();
    void tickCount();

private:
    void setRunning(bool running);
    int  recoveredCount() const;   // files under the current recup dirs

    QComboBox    *m_source   = nullptr;
    QPushButton  *m_refresh  = nullptr;
    QLineEdit    *m_dest     = nullptr;
    QPushButton  *m_browse   = nullptr;
    QCheckBox    *m_allTypes = nullptr;
    QCheckBox    *m_organize = nullptr;
    QListWidget  *m_typeList = nullptr;
    QPushButton  *m_start    = nullptr;
    QPushButton  *m_cancel   = nullptr;
    QPushButton  *m_open     = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel       *m_status   = nullptr;

    QProcess     *m_proc     = nullptr;
    QTimer       *m_timer    = nullptr;
    QString       m_destUsed;    // destination of the active/last run

    QVector<RecoverySource> m_sources;
};
