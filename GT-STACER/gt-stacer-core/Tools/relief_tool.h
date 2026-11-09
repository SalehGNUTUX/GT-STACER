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

    // Is this process part of an app the user is working with right now?
    // (26.12) Auto relief used to freeze EVERY candidate — including the job
    // that was doing the work (a compile, an export, the focused browser).
    // Activity is judged per APP (the whole process tree under the session/
    // terminal), so a busy renderer or compiler child protects its parent too.
    enum class Activity {
        None,
        Focused,      // owns the active window
        Audio,        // playing sound
        Inhibitor,    // blocking sleep/idle (download, video, presentation…)
        Cpu,          // using the CPU now
        Disk,         // reading/writing the disk now
        Recent        // was active a few seconds ago (a pause in a busy job)
    };
    Activity activity  = Activity::None;
    QString  activeVia;          // the process in the same app that is active ("" = this one)
    int      appRoot   = 0;      // top process of its app tree
    bool     active() const { return activity != Activity::None; }

    // The MAIN WORKLOAD (26.12): the one app using the most CPU + disk right
    // now — with everything it runs in the background (its app tree). Smart
    // auto relief keeps it running and may freeze the OTHER busy apps so it
    // gets the machine. The choice is sticky (see ReliefTool::candidates).
    bool     mainWorkload = false;
    double   appLoad   = 0.0;    // its app's load: % of one core + 10 per MB/s of disk

    // Never frozen automatically: what the user is looking at or listening
    // to, what keeps the system awake, and the main workload.
    bool     isProtected() const {
        return mainWorkload || activity == Activity::Focused
            || activity == Activity::Audio || activity == Activity::Inhibitor;
    }
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
    // `focusedPid` = the process owning the active window (0 = unknown); its
    // whole app is marked Activity::Focused. Activity is sampled on every call
    // (CPU/disk rates are deltas since the previous call), so call it on a
    // regular tick — the auto watchdog does — to keep the rates fresh.
    static QVector<ReliefCandidate> candidates(qint64 minMemoryKB = 30 * 1024,
                                               int focusedPid = 0);

    // Every process of the app `pid` belongs to (its app tree), including
    // stopped ones — used to thaw an app as soon as the user switches to it.
    static QVector<int> appTreePids(int pid);

    // Name of the main workload chosen by the last candidates() call — even
    // when it isn't freezable itself (e.g. a compile running in a terminal).
    // "" = no app is busy enough.
    static QString mainWorkloadName();

    static int  suspendPids(const QVector<int> &pids);  // SIGSTOP; returns # frozen

    // ── Never leave an app frozen behind us (26.12) ───────────────────────────
    // A frozen process stays frozen when GT-STACER dies — SIGKILL, a crash, or a
    // SIGTERM before the clean-up ran (found in testing: 8 apps, spectacle among
    // them, stayed stopped after the app was killed). So every change to the set
    // of frozen PIDs is written to a journal (one file per running instance,
    // PID + process start time, so a recycled PID is never touched), and the
    // next start thaws what an instance that is no longer running left frozen.
    static void saveFrozenJournal(const QVector<int> &pids);   // empty = remove
    static int  recoverFrozenJournal();                         // # thawed
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
