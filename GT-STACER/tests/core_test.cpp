#include "../gt-stacer-core/Info/cpu_info.h"
#include "../gt-stacer-core/Info/memory_info.h"
#include "../gt-stacer-core/Info/process_info.h"
#include "../gt-stacer-core/Info/system_info.h"
#include "../gt-stacer-core/Info/network_info.h"
#include "../gt-stacer-core/Info/disk_info.h"
#include "../gt-stacer-core/Info/temperature_info.h"
#include "../gt-stacer-core/Info/gpu_info.h"
#include "../gt-stacer-core/Info/battery_info.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <unistd.h>

static int passed = 0;
static int failed = 0;

#define CHECK(expr, msg) do { \
    if (expr) { qInfo() << "  PASS:" << msg; ++passed; } \
    else      { qWarning() << "  FAIL:" << msg; ++failed; } \
} while (0)

static void testCpuSampler()
{
    qInfo() << "\n[CpuInfo::usage] background sampler";
    QElapsedTimer t; t.start();
    auto first = CpuInfo::usage();
    qint64 elapsed1 = t.elapsed();
    qInfo() << "  First call:" << elapsed1 << "ms — total =" << first.total
            << "cores =" << first.cores;

    CHECK(first.cores > 0,                       "core count > 0");
    CHECK(!first.model.isEmpty(),                "model not empty");
    CHECK(first.total >= 0 && first.total <= 100, "total in [0,100]");

    // Subsequent calls should be NEAR-INSTANT (cached read of QMutex-protected value).
    t.restart();
    for (int i = 0; i < 100; ++i) (void)CpuInfo::usage();
    qint64 elapsed100 = t.elapsed();
    qInfo() << "  100 consecutive calls:" << elapsed100 << "ms";
    CHECK(elapsed100 < 50, "100 calls take < 50ms (non-blocking)");

    // After 1.5s the sampler should have updated the value.
    QThread::msleep(1500);
    auto later = CpuInfo::usage();
    qInfo() << "  After 1.5s — total =" << later.total
            << "perCore size =" << later.perCore.size();
    CHECK(later.perCore.size() == later.cores, "perCore has cores entries");
}

static void testMemory()
{
    qInfo() << "\n[MemoryInfo::memory]";
    auto m = MemoryInfo::memory();
    qInfo() << "  totalRam =" << m.totalRam << "  usedRam =" << m.usedRam
            << "  ramPercent =" << m.ramPercent();
    CHECK(m.totalRam > 0,                                "totalRam > 0");
    CHECK(m.usedRam >= 0 && m.usedRam <= m.totalRam,     "usedRam in [0,total]");
    CHECK(m.ramPercent() >= 0 && m.ramPercent() <= 100,  "ramPercent in [0,100]");
}

static void testProcesses()
{
    qInfo() << "\n[ProcessInfo::processes] /proc reader";
    QElapsedTimer t; t.start();
    auto procs = ProcessInfo::processes();
    qInfo() << "  First call:" << t.elapsed() << "ms — found" << procs.size() << "processes";
    CHECK(procs.size() > 5, "at least a few processes found");

    // Find our own PID
    int self = getpid();
    bool foundSelf = false;
    for (const auto &p : procs) {
        if (p.pid == self) { foundSelf = true;
            qInfo() << "  Self: pid =" << p.pid << " name =" << p.name
                    << " user =" << p.user << " mem KB =" << p.memoryKB;
            CHECK(!p.name.isEmpty(),      "self.name not empty");
            CHECK(!p.user.isEmpty(),      "self.user not empty");
            CHECK(p.memoryKB > 0,         "self.memoryKB > 0");
            break;
        }
    }
    CHECK(foundSelf, "self process found in /proc enumeration");

    // Second call after sleep should have computed CPU% deltas
    QThread::msleep(300);
    auto procs2 = ProcessInfo::processes();
    double totalCpu = 0;
    for (const auto &p : procs2) totalCpu += p.cpuPercent;
    qInfo() << "  Second call: sum of cpuPercent =" << totalCpu;
    CHECK(procs2.size() > 5,         "second call still returns processes");
    CHECK(totalCpu >= 0,             "cpuPercent non-negative");

    // No process should have cpuPercent > 100*ncpu (very loose sanity check)
    int ncpu = QThread::idealThreadCount();
    bool inRange = true;
    for (const auto &p : procs2)
        if (p.cpuPercent < 0 || p.cpuPercent > 100.0 * ncpu) inRange = false;
    CHECK(inRange, "all cpuPercent values in plausible range");
}

static void testSystemInfo()
{
    qInfo() << "\n[SystemInfo]";
    auto info = SystemInfo::info();
    qInfo() << "  hostname =" << info.hostname
            << "kernel =" << info.kernelVersion;
    CHECK(!info.hostname.isEmpty(),      "hostname not empty");
    CHECK(!info.kernelVersion.isEmpty(), "kernel version not empty");
    CHECK(info.uptimeSeconds > 0,        "uptime > 0");
}

static void testNetwork()
{
    qInfo() << "\n[NetworkInfo::interfaces]";
    auto ifaces = NetworkInfo::interfaces();
    qInfo() << "  Found" << ifaces.size() << "interfaces";
    CHECK(ifaces.size() > 0, "at least one interface (lo)");
}

static void testDisk()
{
    qInfo() << "\n[DiskInfo::partitions]";
    auto parts = DiskInfo::partitions();
    qInfo() << "  Found" << parts.size() << "mounted partitions";
    bool hasRoot = false;
    for (const auto &p : parts) if (p.mountPoint == "/") hasRoot = true;
    CHECK(hasRoot, "/ partition present");
}

static void testTemperature()
{
    qInfo() << "\n[TemperatureInfo]";
    auto sensors = TemperatureInfo::hwmonSensors();
    qInfo() << "  hwmon sensors:" << sensors.size();
    // No assertion — system may not have sensors.
}

static void testGpu()
{
    qInfo() << "\n[GpuInfo::gpus]";
    auto gpus = GpuInfo::gpus();
    qInfo() << "  GPUs:" << gpus.size();
    for (const auto &g : gpus)
        qInfo() << "    name =" << g.name
                << "usage =" << (g.usagePercent.has_value() ? *g.usagePercent : -1);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testCpuSampler();
    testMemory();
    testProcesses();
    testSystemInfo();
    testNetwork();
    testDisk();
    testTemperature();
    testGpu();

    qInfo().noquote() << QString("\n────── %1 passed · %2 failed ──────")
                            .arg(passed).arg(failed);
    return failed == 0 ? 0 : 1;
}
