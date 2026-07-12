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

private slots:
    void refreshCandidates();
    void suspendSelected();
    void resumeAll();
    void tickMonitor();
    void onAutoToggled(bool on);

private:
    void updateLiveLabels();
    void updateBanner();

    QTableWidget *m_table       = nullptr;
    QLabel       *m_ramLabel    = nullptr;
    QLabel       *m_cpuLabel    = nullptr;
    QLabel       *m_banner      = nullptr;
    QPushButton  *m_suspendBtn  = nullptr;
    QPushButton  *m_resumeBtn   = nullptr;
    QPushButton  *m_refreshBtn  = nullptr;
    QCheckBox    *m_dropCaches  = nullptr;

    QCheckBox    *m_autoCheck   = nullptr;
    QSpinBox     *m_cpuThresh   = nullptr;
    QSpinBox     *m_ramThresh   = nullptr;
    QSpinBox     *m_holdSecs    = nullptr;

    QTimer       *m_monitor     = nullptr;  // live labels + auto-mode watchdog

    QSet<int>     m_suspended;              // PIDs we have frozen
    CpuStat       m_prevStat{};
    bool          m_haveStat    = false;
    int           m_overSeconds = 0;        // consecutive seconds over threshold
    int           m_underSeconds= 0;        // consecutive seconds back under threshold
    double        m_lastCpu     = 0.0;
};
