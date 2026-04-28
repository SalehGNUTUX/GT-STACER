#pragma once
#include <QString>
#include <QVector>

struct ProcessData {
    int     pid    = 0;
    QString name;
    QString user;
    QString status;
    double  cpuPercent  = 0.0;
    qint64  memoryKB    = 0;
    QString command;
};

class ProcessInfo {
public:
    static QVector<ProcessData> processes();
    static bool                 kill(int pid);
    static int                  count();
};
