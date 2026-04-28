#include "cpu_info.h"
#include "../Utils/file_util.h"
#include <QThread>

static CpuStat parseStat(const QString &line)
{
    auto parts = line.split(' ', Qt::SkipEmptyParts);
    CpuStat s{};
    if (parts.size() >= 8) {
        s.user    = parts[1].toLongLong();
        s.nice    = parts[2].toLongLong();
        s.system  = parts[3].toLongLong();
        s.idle    = parts[4].toLongLong();
        s.iowait  = parts[5].toLongLong();
        s.irq     = parts[6].toLongLong();
        s.softirq = parts[7].toLongLong();
        s.steal   = parts.size() > 8 ? parts[8].toLongLong() : 0;
    }
    return s;
}

CpuStat CpuInfo::readStat(int cpuIndex)
{
    QString prefix = cpuIndex < 0 ? "cpu " : QString("cpu%1 ").arg(cpuIndex);
    auto lines = FileUtil::readFile("/proc/stat").split('\n');
    for (const auto &line : lines) {
        if (line.startsWith(prefix))
            return parseStat(line);
    }
    return {};
}

double CpuInfo::calcUsage(const CpuStat &a, const CpuStat &b)
{
    qint64 idleA  = a.idle + a.iowait;
    qint64 idleB  = b.idle + b.iowait;
    qint64 totalA = a.user + a.nice + a.system + idleA + a.irq + a.softirq + a.steal;
    qint64 totalB = b.user + b.nice + b.system + idleB + b.irq + b.softirq + b.steal;
    qint64 dTotal = totalB - totalA;
    qint64 dIdle  = idleB  - idleA;
    if (dTotal <= 0) return 0.0;
    return 100.0 * (1.0 - static_cast<double>(dIdle) / dTotal);
}

CpuUsage CpuInfo::usage()
{
    CpuUsage result;
    result.cores = coreCount();
    result.model = model();
    result.freqMHz = frequencyMHz();

    auto s1 = readStat();
    QThread::msleep(200);
    auto s2 = readStat();
    result.total = calcUsage(s1, s2);

    for (int i = 0; i < result.cores; ++i) {
        auto c1 = readStat(i);
        QThread::msleep(0);
        auto c2 = readStat(i);
        result.perCore << calcUsage(c1, c2);
    }
    return result;
}

QString CpuInfo::model()
{
    auto lines = FileUtil::readFile("/proc/cpuinfo").split('\n');
    for (const auto &line : lines) {
        if (line.startsWith("model name")) {
            auto parts = line.split(':');
            if (parts.size() >= 2) return parts[1].trimmed();
        }
    }
    return "Unknown CPU";
}

int CpuInfo::coreCount()
{
    return static_cast<int>(QThread::idealThreadCount());
}

double CpuInfo::frequencyMHz()
{
    auto lines = FileUtil::readFile("/proc/cpuinfo").split('\n');
    for (const auto &line : lines) {
        if (line.startsWith("cpu MHz")) {
            auto parts = line.split(':');
            if (parts.size() >= 2) return parts[1].trimmed().toDouble();
        }
    }
    // Try /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
    QString freqStr = FileUtil::readFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (!freqStr.isEmpty()) return freqStr.toLongLong() / 1000.0;
    return 0.0;
}
