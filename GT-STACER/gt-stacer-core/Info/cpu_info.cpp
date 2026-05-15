#include "cpu_info.h"
#include "../Utils/file_util.h"
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInteger>

// ─── Parsing ────────────────────────────────────────────────────────────
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
    QString freqStr = FileUtil::readFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (!freqStr.isEmpty()) return freqStr.toLongLong() / 1000.0;
    return 0.0;
}

// ─── Background sampler ─────────────────────────────────────────────────
// One worker thread polls /proc/stat once per second, computes deltas, and
// caches the latest CpuUsage under a mutex. CpuInfo::usage() now returns the
// cached value instantly — the UI thread no longer blocks for 200 ms per call.
namespace {

class CpuSamplerThread : public QThread {
public:
    CpuSamplerThread()
    {
        setObjectName("gt-stacer-cpu");
        // Prime the cache synchronously so the very first caller doesn't see
        // cores=0 / empty model while the thread is still spinning up.
        m_cores       = CpuInfo::coreCount();
        m_cachedModel = CpuInfo::model();
        m_prevAgg     = CpuInfo::readStat();
        m_prevCore.resize(m_cores);
        for (int i = 0; i < m_cores; ++i) m_prevCore[i] = CpuInfo::readStat(i);

        {
            QMutexLocker lk(&m_mtx);
            m_current.cores   = m_cores;
            m_current.model   = m_cachedModel;
            m_current.freqMHz = CpuInfo::frequencyMHz();
            // total/perCore stay 0 for the very first read — they get filled in
            // after the worker takes its second sample (≈ 200 ms later).
        }

        m_running.storeRelease(1);
        start();
    }
    ~CpuSamplerThread() override
    {
        m_running.storeRelease(0);
        wait(2000);
    }

    CpuUsage current()
    {
        QMutexLocker lk(&m_mtx);
        return m_current;
    }

protected:
    void run() override
    {
        msleep(200); // Let stat counters tick before computing the first delta.

        while (m_running.loadAcquire()) {
            CpuUsage u;
            u.cores   = m_cores;
            u.model   = m_cachedModel;
            u.freqMHz = CpuInfo::frequencyMHz();

            auto agg = CpuInfo::readStat();
            u.total  = CpuInfo::calcUsage(m_prevAgg, agg);
            m_prevAgg = agg;

            for (int i = 0; i < m_cores; ++i) {
                auto cur = CpuInfo::readStat(i);
                u.perCore << CpuInfo::calcUsage(m_prevCore[i], cur);
                m_prevCore[i] = cur;
            }

            {
                QMutexLocker lk(&m_mtx);
                m_current = u;
            }

            // Sample once per second; check shutdown flag every 100 ms.
            for (int i = 0; i < 10 && m_running.loadAcquire(); ++i) msleep(100);
        }
    }

private:
    QMutex m_mtx;
    CpuUsage m_current;
    QAtomicInteger<int> m_running{0};
    int m_cores = 0;
    QString m_cachedModel;
    CpuStat m_prevAgg{};
    QVector<CpuStat> m_prevCore;
};

CpuSamplerThread &sampler()
{
    static CpuSamplerThread s;
    return s;
}

} // namespace

CpuUsage CpuInfo::usage()
{
    return sampler().current();
}
