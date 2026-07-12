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

    // Ask the kernel to drop clean page/dentry/inode caches (vm.drop_caches=3)
    // after a sync. Root-only, so it goes through pkexec. Harmless — the kernel
    // simply re-reads from disk on demand — but frees "cached" RAM immediately.
    static bool dropCaches();
};
