#pragma once
#include <QString>
#include <QVector>

struct DiskPartition {
    QString device;
    QString mountPoint;
    QString fsType;
    qint64  total = 0;
    qint64  used  = 0;
    qint64  free  = 0;
    double  percent() const { return total > 0 ? (used * 100.0) / total : 0.0; }
};

struct DiskIoStats {
    QString device;
    qint64  readBytes  = 0;
    qint64  writeBytes = 0;
    qint64  readSpeed  = 0;
    qint64  writeSpeed = 0;
};

class DiskInfo {
public:
    static QVector<DiskPartition> partitions();
    static QVector<DiskIoStats>   ioStats();
    static qint64 totalUsed();
    static qint64 totalSize();
};
