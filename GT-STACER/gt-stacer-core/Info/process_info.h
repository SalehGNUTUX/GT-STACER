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
    static int                  count();

    // Signals — true on success. `kill` is SIGTERM (graceful), `forceKill`
    // is SIGKILL. `suspend`/`resume` map to SIGSTOP/SIGCONT.
    static bool kill(int pid);
    static bool forceKill(int pid);
    static bool suspend(int pid);
    static bool resume(int pid);
    // niceness ∈ [-20, 19]. Returns true if `renice` succeeded.
    static bool setPriority(int pid, int niceness);

    // Heuristic: returns true when killing this PID risks destabilising the
    // session — init/PID 1, kernel threads, systemd, dbus, the Wayland/X11
    // compositor, the audio daemon, etc. The UI uses this to escalate the
    // confirmation prompt instead of refusing the action outright.
    static bool isCriticalProcess(int pid, const QString &name);
};
