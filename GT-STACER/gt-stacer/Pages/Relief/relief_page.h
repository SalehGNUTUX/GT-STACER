#pragma once
#include <QWidget>
#include <QSet>
#include "../../../gt-stacer-core/Info/cpu_info.h"

class QTableWidget;
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QTimer;

// System Relief — temporarily freezes idle, non-critical user processes to
// relieve RAM/CPU pressure, then thaws them. Manual (a button) plus an optional
// automatic mode that watches thresholds. Everything frozen is resumed on exit.
class ReliefPage : public QWidget {
    Q_OBJECT
public:
    explicit ReliefPage(QWidget *parent = nullptr);
    ~ReliefPage() override;

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void refreshCandidates();
    void suspendSelected();
    void resumeAll();
    void easeSelected();                     // ionice idle — gentle disk relief
    void switchToBfq();                      // change the disk I/O scheduler to BFQ
    void showBfqInfo();                      // "i": what BFQ is and when it helps
    void onFocusChanged(int pid);            // thaw a frozen app the user switched to
    void tickMonitor();
    void onAutoToggled(bool on);
    void toggleSelectAll();

private:
    void loadSettings();
    void saveSettings();
    void applyMonitorCadence();             // interval + keepAlive from state/visibility
    void updateLiveLabels();
    void updateStatusBadge();
    void updateBanner();
    void updateSchedulerUi();               // reflect the disk's active I/O scheduler
    QVector<int> checkedPids() const;       // rows the user has ticked

    QTableWidget *m_table        = nullptr;
    QLabel       *m_ramLabel     = nullptr;
    QLabel       *m_cpuLabel     = nullptr;
    QLabel       *m_ioLabel      = nullptr;  // live disk-pressure (PSI) readout
    QLabel       *m_badge        = nullptr;  // colored status pill
    QLabel       *m_banner       = nullptr;
    QPushButton  *m_suspendBtn   = nullptr;
    QPushButton  *m_resumeBtn    = nullptr;
    QPushButton  *m_easeBtn      = nullptr;  // ionice idle (gentle disk relief)
    QPushButton  *m_refreshBtn   = nullptr;
    QPushButton  *m_selectAllBtn = nullptr;
    QCheckBox    *m_dropCaches   = nullptr;
    QCheckBox    *m_autoRefresh  = nullptr;
    QSpinBox     *m_refreshSecs  = nullptr;

    QLabel       *m_schedLabel   = nullptr;  // "Disk scheduler: mq-deadline"
    QPushButton  *m_bfqBtn       = nullptr;  // switch to BFQ

    QCheckBox    *m_autoCheck    = nullptr;
    QSpinBox     *m_cpuThresh    = nullptr;
    QSpinBox     *m_ramThresh    = nullptr;
    QSpinBox     *m_ioThresh     = nullptr;
    QSpinBox     *m_holdSecs     = nullptr;
    QCheckBox    *m_prioritize   = nullptr;  // smart: keep only the heaviest app busy
    QString       m_disk;                    // disk backing "/", for scheduler control
    double        m_lastIo       = 0.0;      // last PSI I/O pressure reading

    QTimer       *m_monitor      = nullptr;  // live labels + auto-mode watchdog

    QSet<int>     m_suspended;               // PIDs we have frozen
    // Auto relief escalates in stages (26.12): 1 = idle apps frozen, 2 = also
    // the busy apps that are not the main workload. Thawing goes back down.
    int           m_stage        = 0;
    QSet<int>     m_frozenBusy;              // stage-2 PIDs (thawed first)
    CpuStat       m_prevStat{};
    bool          m_haveStat     = false;
    bool          m_allSelected  = true;     // select-all toggle state
    bool          m_loading      = false;    // suppress save while loading settings
    int           m_overSeconds  = 0;        // consecutive seconds over threshold
    int           m_underSeconds = 0;        // consecutive seconds back under threshold
    int           m_refreshAccum = 0;        // seconds accumulated toward auto-refresh
    double        m_lastCpu      = 0.0;
    int           m_sampleAccum  = 0;        // background activity-sampling cadence
    QSet<int>     m_userTicked;              // rows whose checkbox the USER changed
    bool          m_populating   = false;    // ignore our own setCheckState calls
};
