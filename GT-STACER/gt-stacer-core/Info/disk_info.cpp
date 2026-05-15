#include "disk_info.h"
#include "../Utils/file_util.h"
#include <QStorageInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

QVector<DiskPartition> DiskInfo::partitions()
{
    QVector<DiskPartition> result;
    const QStringList skip = {"tmpfs", "devtmpfs", "sysfs", "proc", "devpts",
                               "cgroup", "cgroup2", "pstore", "bpf", "tracefs",
                               "debugfs", "hugetlbfs", "mqueue", "fusectl",
                               "overlay", "squashfs"};

    for (const auto &vol : QStorageInfo::mountedVolumes()) {
        if (!vol.isValid()) continue;
        QString fsType = vol.fileSystemType();
        if (skip.contains(fsType)) continue;
        if (vol.rootPath().startsWith("/sys") || vol.rootPath().startsWith("/proc")
            || vol.rootPath().startsWith("/dev/pts")) continue;

        DiskPartition p;
        p.device     = vol.device();
        p.mountPoint = vol.rootPath();
        p.fsType     = fsType;
        p.total      = vol.bytesTotal();
        p.free       = vol.bytesFree();
        p.used       = p.total - p.free;
        result << p;
    }
    return result;
}

namespace {
struct IoSample { qint64 readBytes; qint64 writeBytes; qint64 atMs; };
QHash<QString, IoSample> &lastIo()       { static QHash<QString, IoSample> h; return h; }
QMutex &lastIoMutex()                     { static QMutex m; return m; }
}

QVector<DiskIoStats> DiskInfo::ioStats()
{
    QVector<DiskIoStats> result;
    auto lines = FileUtil::readFile("/proc/diskstats").split('\n', Qt::SkipEmptyParts);

    // We need monotonic-ish ms to scale byte deltas to bytes/sec.
    static QElapsedTimer clock;
    if (!clock.isValid()) clock.start();
    const qint64 nowMs = clock.elapsed();

    QHash<QString, IoSample> previous;
    {
        QMutexLocker lk(&lastIoMutex());
        previous = lastIo();
    }
    QHash<QString, IoSample> current;

    for (const auto &line : lines) {
        auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 14) continue;
        QString dev = parts[2];
        // Skip loop and ram devices
        if (dev.startsWith("loop") || dev.startsWith("ram")) continue;
        // Skip partitions (e.g. sda1) but keep whole disks (sda) and nvme namespaces (nvme0n1).
        if (dev.contains(QRegularExpression("\\d$")) && !dev.startsWith("nvme")) continue;

        DiskIoStats s;
        s.device     = dev;
        s.readBytes  = parts[5].toLongLong() * 512;
        s.writeBytes = parts[9].toLongLong() * 512;

        IoSample cur{ s.readBytes, s.writeBytes, nowMs };
        current.insert(dev, cur);
        if (auto it = previous.find(dev); it != previous.end()) {
            qint64 dMs = nowMs - it->atMs;
            if (dMs > 0) {
                s.readSpeed  = ((s.readBytes  - it->readBytes)  * 1000) / dMs;
                s.writeSpeed = ((s.writeBytes - it->writeBytes) * 1000) / dMs;
                if (s.readSpeed  < 0) s.readSpeed  = 0; // counter wrap
                if (s.writeSpeed < 0) s.writeSpeed = 0;
            }
        }
        result << s;
    }

    {
        QMutexLocker lk(&lastIoMutex());
        lastIo() = current;
    }
    return result;
}

qint64 DiskInfo::totalUsed()
{
    qint64 used = 0;
    for (const auto &p : partitions()) {
        if (p.mountPoint == "/") used += p.used;
    }
    return used;
}

qint64 DiskInfo::totalSize()
{
    qint64 total = 0;
    for (const auto &p : partitions()) {
        if (p.mountPoint == "/") total += p.total;
    }
    return total;
}
