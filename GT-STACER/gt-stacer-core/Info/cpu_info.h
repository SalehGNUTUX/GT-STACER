#pragma once
#include <QString>
#include <QVector>

struct CpuStat {
    qint64 user, nice, system, idle, iowait, irq, softirq, steal;
};

struct CpuUsage {
    double total = 0.0;
    double user  = 0.0;
    double sys   = 0.0;
    double idle  = 0.0;
    int    cores = 0;
    QString model;
    double freqMHz = 0.0;
    QVector<double> perCore;
};

class CpuInfo {
public:
    static CpuUsage usage();
    static QString  model();
    static int      coreCount();
    static double   frequencyMHz();

private:
    static CpuStat  readStat(int cpuIndex = -1);
    static double   calcUsage(const CpuStat &a, const CpuStat &b);
};
