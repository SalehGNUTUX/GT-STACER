#pragma once
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QFrame>

class CircularGauge;

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void tickUptime();

private:
    void buildUi();
    QFrame *makeInfoRow(const QString &iconPath, const QString &label, QLabel **valueOut);
    QFrame *makeNetCard();
    QFrame *makeGpuCard();
    QFrame *makeBatCard();

    void refreshSystemInfo();
    void refreshCpu();
    void refreshMemory();
    void refreshDisk();
    void refreshNetwork();
    void refreshGpu();
    void refreshBattery();

    // Circular gauges
    CircularGauge *m_cpuGauge  = nullptr;
    CircularGauge *m_ramGauge  = nullptr;
    CircularGauge *m_diskGauge = nullptr;
    CircularGauge *m_swapGauge = nullptr;
    CircularGauge *m_batGauge  = nullptr;

    // System info labels
    QLabel *m_hostname  = nullptr;
    QLabel *m_os        = nullptr;
    QLabel *m_kernel    = nullptr;
    QLabel *m_arch      = nullptr;
    QLabel *m_desktop   = nullptr;
    QLabel *m_display   = nullptr;
    QLabel *m_initSys   = nullptr;
    QLabel *m_uptime    = nullptr;
    QLabel *m_cpuModel  = nullptr;
    QLabel *m_cpuFreq   = nullptr;
    QLabel *m_totalRam  = nullptr;

    // Network
    QLabel *m_netIface  = nullptr;
    QLabel *m_netIp     = nullptr;
    QLabel *m_netRx     = nullptr;
    QLabel *m_netTx     = nullptr;
    QLabel *m_diskRead  = nullptr;
    QLabel *m_diskWrite = nullptr;

    // GPU (optional)
    QFrame *m_gpuCard   = nullptr;
    QLabel *m_gpuName   = nullptr;
    QLabel *m_gpuUse    = nullptr;
    QLabel *m_gpuMem    = nullptr;
    QLabel *m_gpuTmp    = nullptr;

    // Battery (optional)
    QFrame *m_batCard   = nullptr;
    QLabel *m_batStatus = nullptr;
    QLabel *m_batTime   = nullptr;

    QTimer *m_refreshTimer = nullptr;
    QTimer *m_uptimeTimer  = nullptr;
    qint64  m_uptimeBase   = 0;
    int     m_uptimeTickCount = 0;
};
