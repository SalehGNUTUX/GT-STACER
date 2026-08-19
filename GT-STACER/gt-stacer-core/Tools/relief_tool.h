#pragma once
#include <QString>
#include <QVector>

// A process that is safe to suspend temporarily to relieve memory/CPU pressure.
struct ReliefCandidate {
    int     pid        = 0;
    QString name;
    QString user;
    qint64  memoryKB   = 0;
    double  cpuPercent = 0.0;
    double  ioKBps     = 0.0;   // real block-layer I/O rate (read+write) since last sample
    QString status;     // single-letter /proc state (R,S,D,T,Z…)
};

// Smart, reversible pressure relief. The idea: under heavy load, temporarily
// freeze (SIGSTOP) idle user processes so the foreground workload gets the RAM
// and CPU it needs, then thaw them (SIGCONT) once things settle. Freezing is
// fully reversible and does NOT lose the process's state, unlike killing.
//
// Safety is layered:
//  · Only the current user's own processes are ever eligible.
//  · ProcessInfo::isCriticalProcess() filters init/systemd/dbus/compositor/…
//  · An extra desktop-shell denylist covers user-owned WMs/panels/portals whose
//    suspension would freeze the whole session.
//  · GT-STACER never suspends itself.
// The caller is responsible for resuming — the Relief page resumes everything
// it froze on exit so nothing is left stopped.
class ReliefTool {
public:
    // Candidates worth suspending, sorted by memory (desc). `minMemoryKB` drops
    // trivially small processes that wouldn't move the needle.
    static QVector<ReliefCandidate> candidates(qint64 minMemoryKB = 30 * 1024);

    static int  suspendPids(const QVector<int> &pids);  // SIGSTOP; returns # frozen
    static int  resumePids(const QVector<int> &pids);   // SIGCONT; returns # thawed

    // ── Disk-I/O relief (26.10) ────────────────────────────────────────────────
    // The "frozen but CPU/RAM are fine" case: the disk is the bottleneck. On old
    // spinning drives one process thrashing the disk stalls the whole desktop.

    // System-wide I/O pressure from PSI (/proc/pressure/io, "some avg10"): the %
    // of the last 10 s that tasks were stalled waiting on I/O. -1 if PSI is
    // unavailable (kernel < 4.20 or disabled).
    static double ioPressure();

    // Lower the I/O priority of these processes to the idle class (ionice -c3):
    // the foreground keeps a responsive disk while they keep running (just slower
    // on disk) — a gentler alternative to freezing. Our own processes, no root.
    // Most effective under the BFQ scheduler. Returns how many were re-niced.
    static int  easeIoPids(const QVector<int> &pids);

    // I/O scheduler of the disk backing "/". Switching an old HDD to **BFQ** is
    // the single biggest win for desktop responsiveness under load (and it makes
    // easeIoPids() actually bite). Applies until reboot.
    static QString     rootDisk();                       // "sda" / "nvme0n1" / ""
    static QString     ioScheduler(const QString &disk); // active one, e.g. "mq-deadline"
    static QStringList schedulers(const QString &disk);  // all available
    static bool        setScheduler(const QString &disk, const QString &sched); // pkexec

    // Ask the kernel to drop clean page/dentry/inode caches (vm.drop_caches=3)
    // after a sync. Root-only, so it goes through pkexec. Harmless — the kernel
    // simply re-reads from disk on demand — but frees "cached" RAM immediately.
    static bool dropCaches();
};
