#include "disk_info.h"
#include "../Utils/file_util.h"
#include <QStorageInfo>
#include <QDir>
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

QVector<DiskIoStats> DiskInfo::ioStats()
{
    QVector<DiskIoStats> result;
    auto lines = FileUtil::readFile("/proc/diskstats").split('\n', Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 14) continue;
        QString dev = parts[2];
        // Skip loop and ram devices
        if (dev.startsWith("loop") || dev.startsWith("ram")) continue;
        // Only physical disks and NVMe (not partitions)
        if (dev.contains(QRegularExpression("\\d$")) && !dev.startsWith("nvme")) continue;

        DiskIoStats s;
        s.device     = dev;
        s.readBytes  = parts[5].toLongLong() * 512;
        s.writeBytes = parts[9].toLongLong() * 512;
        result << s;
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
