#include "relief_tool.h"
#include "../Info/process_info.h"
#include "../Utils/command_util.h"
#include <QSet>
#include <QFile>
#include <QByteArray>
#include <QList>
#include <QHash>
#include <QElapsedTimer>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <csignal>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

namespace {

QString currentUserName()
{
    if (struct passwd *pw = getpwuid(getuid()))
        return QString::fromLocal8Bit(pw->pw_name);
    return QString::fromLocal8Bit(qgetenv("USER"));
}

// User-owned processes that ProcessInfo::isCriticalProcess may not flag but
// whose suspension would freeze the desktop session. Matched case-insensitively
// against the process name (which may be a truncated comm, so we compare by
// prefix/contains for the compositor family).
bool isDesktopShell(const QString &nameLower)
{
    static const QStringList shells = {
        "gnome-shell", "plasmashell", "kwin_x11", "kwin_wayland", "kwin",
        "mutter", "xfwm4", "marco", "muffin", "cinnamon", "budgie-wm",
        "budgie-panel", "xfce4-panel", "mate-panel", "lxqt-panel", "wayfire",
        "sway", "hyprland", "labwc", "openbox", "i3", "gala", "weston",
        "xdg-desktop-por", "xdg-desktop-portal", "xembedsniproxy",
        "gnome-session-b", "gnome-session", "ksmserver", "plasma-session",
        "xdg-permission-", "polkit-gnome-au", "polkit-kde-auth",
        "gsd-", "org.gnome.Shell", "kded", "kded6", "kded5",
        "ibus-daemon", "fcitx", "fcitx5", "at-spi2-registr", "at-spi-bus-laun"
    };
    for (const QString &s : shells) {
        const QString sl = s.toLower();
        if (nameLower == sl || nameLower.startsWith(sl) || sl.startsWith(nameLower))
            return true;
    }
    return false;
}

// Parse the fields after the "(comm)" section of /proc/<pid>/stat.
// The comm field may contain spaces and parentheses, so we split on the LAST
// ')'. The remaining fields are: state ppid pgrp session tty_nr ...
// i.e. fields[0]=state, fields[1]=ppid, fields[3]=session.
QList<QByteArray> statFieldsAfterComm(int pid)
{
    QFile f(QString("/proc/%1/stat").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray raw = f.readAll();
    const int rparen = raw.lastIndexOf(')');
    if (rparen < 0)
        return {};
    return raw.mid(rparen + 1).simplified().split(' ');
}

int parentPid(int pid)
{
    const QList<QByteArray> fields = statFieldsAfterComm(pid);
    return fields.size() > 1 ? fields[1].toInt() : 0;
}

int sessionOf(int pid)
{
    const QList<QByteArray> fields = statFieldsAfterComm(pid);
    return fields.size() > 3 ? fields[3].toInt() : 0;
}

// Walk from `startPid` up the parent chain to init, collecting every ancestor.
// Suspending any of these would freeze the process tree that owns gt-stacer —
// most importantly the terminal emulator and shell (and any node/claude parent)
// that launched it. A guard caps the walk in case of a corrupt /proc reading.
QSet<int> ancestorPids(int startPid)
{
    QSet<int> anc;
    int pid = startPid;
    for (int guard = 0; pid > 1 && guard < 128; ++guard) {
        const int pp = parentPid(pid);
        if (pp <= 0)
            break;
        anc.insert(pp);
        pid = pp;
    }
    return anc;
}

// Interactive shells, terminal multiplexers and terminal emulators — plus the
// node/claude runtime — whose suspension would freeze the user's working
// session even when they are NOT in gt-stacer's ancestor chain (e.g. a separate
// terminal window, or gt-stacer launched from a desktop icon). The comm name in
// /proc is truncated to 15 chars, so entries are kept short and matched exactly.
// Cumulative bytes this process has actually pushed through the block layer
// (read_bytes + write_bytes from /proc/<pid>/io). Readable for our own
// processes without root. -1 if unavailable.
qint64 procIoBytes(int pid)
{
    QFile f(QString("/proc/%1/io").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    qint64 rd = 0, wr = 0;
    for (const QByteArray &line : f.readAll().split('\n')) {
        if      (line.startsWith("read_bytes:"))  rd = line.mid(11).trimmed().toLongLong();
        else if (line.startsWith("write_bytes:")) wr = line.mid(12).trimmed().toLongLong();
    }
    return rd + wr;
}

// Only [a-z0-9-] — safe to interpolate into a /sys path or a scheduler value.
bool isSafeSysToken(const QString &s)
{
    static const QRegularExpression re("^[a-z0-9][a-z0-9-]*$");
    return re.match(s).hasMatch();
}

bool isTerminalOrShell(const QString &nameLower)
{
    static const QStringList names = {
        // shells
        "bash", "zsh", "fish", "sh", "dash", "ksh", "tcsh", "csh", "ash",
        "nu", "elvish", "xonsh", "pwsh",
        // terminal emulators (comm, truncated to 15)
        "konsole", "yakuake", "gnome-terminal", "gnome-terminal-", "kgx",
        "xterm", "uxterm", "alacritty", "kitty", "foot", "footclient",
        "wezterm", "wezterm-gui", "terminator", "tilix", "xfce4-terminal",
        "mate-terminal", "lxterminal", "qterminal", "st", "urxvt", "urxvtd",
        "rxvt", "termite", "sakura", "guake", "tilda", "cool-retro-term",
        "deepin-termina", "terminology", "contour", "ghostty", "blackbox",
        "ptyxis", "eterm", "roxterm", "hyper", "wave", "warp",
        // multiplexers / session tools
        "tmux", "tmux: server", "screen", "byobu", "zellij", "dtach", "abduco",
        // agent / runtime that must never be frozen mid-session
        "claude", "node", "nodejs"
    };
    return names.contains(nameLower);
}

} // namespace

// ── App trees & activity (26.12) ─────────────────────────────────────────────
namespace {

using Activity = ReliefCandidate::Activity;

// comm + ppid of every process, from /proc/<pid>/stat.
struct ProcNode { QString comm; int ppid = 0; QString user; };

// Parents that END an app tree: the session, the service manager, the desktop
// shell, a terminal/shell. Everything below one of these is "one app".
bool isTreeBoundary(const QString &nameLower)
{
    static const QStringList names = {
        "systemd", "init", "(sd-pam)", "dbus-daemon", "dbus-broker", "dbus-broker-lau",
        "kdeinit5", "kdeinit6", "ksmserver", "plasmashell", "krunner", "kded5", "kded6",
        "startplasma-way", "startplasma-x11", "plasma_session", "kwin_wayland", "kwin_x11",
        "kwin_wayland_wr", "gnome-session-b", "gnome-shell", "xfce4-session", "lxqt-session",
        "mate-session", "cinnamon-sessio", "budgie-daemon", "sddm-helper", "gdm-session-wor",
        "lightdm", "xdg-desktop-por", "plasma-browser-", "flatpak-portal", "flatpak-session"
    };
    return names.contains(nameLower) || isTerminalOrShell(nameLower) || isDesktopShell(nameLower);
}

QHash<int, ProcNode> procTable(const QVector<ProcessData> &procs)
{
    QHash<int, ProcNode> t;
    t.reserve(procs.size());
    for (const ProcessData &p : procs) {
        QFile f(QString("/proc/%1/stat").arg(p.pid));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray raw = f.readAll();
        const int l = raw.indexOf('('), r = raw.lastIndexOf(')');
        if (l < 0 || r < l) continue;
        ProcNode n;
        n.comm = QString::fromLocal8Bit(raw.mid(l + 1, r - l - 1));
        const QList<QByteArray> fields = raw.mid(r + 1).simplified().split(' ');
        n.ppid = fields.size() > 1 ? fields[1].toInt() : 0;
        n.user = p.user;
        t.insert(p.pid, n);
    }
    return t;
}

// Top process of `pid`'s app: walk up while the parent is still "inside" the
// app (same user, not a session/terminal boundary).
int appRootOf(int pid, const QHash<int, ProcNode> &t, const QString &me)
{
    int cur = pid;
    for (int guard = 0; guard < 64; ++guard) {
        const auto it = t.constFind(cur);
        if (it == t.constEnd()) break;
        const int pp = it->ppid;
        const auto pit = t.constFind(pp);
        if (pp <= 1 || pit == t.constEnd() || pit->user != me
            || isTreeBoundary(pit->comm.toLower()))
            break;
        cur = pp;
    }
    return cur;
}

// PIDs of streams that are PLAYING sound (not paused/corked), via pactl —
// works with both PulseAudio and PipeWire (pipewire-pulse). Cached 4 s.
QSet<int> audioPids()
{
    static QSet<int> s_cache;
    static QElapsedTimer s_age;
    if (s_age.isValid() && s_age.elapsed() < 4000) return s_cache;
    s_age.start();
    s_cache.clear();
    if (!CommandUtil::commandExists("pactl")) return s_cache;
    const QString out = CommandUtil::execProgramC("pactl", {"list", "sink-inputs"}, 3000);
    static const QRegularExpression pidRe("application\\.process\\.id = \"(\\d+)\"");
    for (const QString &block : out.split("Sink Input #", Qt::SkipEmptyParts)) {
        if (block.contains("Corked: yes")) continue;          // paused
        const auto m = pidRe.match(block);
        if (m.hasMatch()) s_cache.insert(m.captured(1).toInt());
    }
    return s_cache;
}

// PIDs holding a logind sleep/idle "block" inhibitor (a download, a video,
// a presentation …). Cached 4 s.
QSet<int> inhibitorPids()
{
    static QSet<int> s_cache;
    static QElapsedTimer s_age;
    if (s_age.isValid() && s_age.elapsed() < 4000) return s_cache;
    s_age.start();
    s_cache.clear();
    const QString out = CommandUtil::execProgramC("systemd-inhibit", {"--list", "--no-legend"}, 3000);
    // WHO UID USER PID COMM WHAT WHY MODE — WHO and WHY may contain spaces.
    static const QRegularExpression row("^(.*?)\\s+(\\d+)\\s+(\\S+)\\s+(\\d+)\\s+(\\S+)\\s+(\\S+)\\s+.*\\s(block|delay)\\s*$");
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        const auto m = row.match(line);
        if (!m.hasMatch() || m.captured(7) != "block") continue;
        const QString what = m.captured(6);
        if (what.contains("sleep") || what.contains("idle")) s_cache.insert(m.captured(4).toInt());
    }
    return s_cache;
}

// Strength order — the strongest reason is the one shown.
int rank(Activity a)
{
    switch (a) {
    case Activity::Focused:   return 6;
    case Activity::Audio:     return 5;
    case Activity::Inhibitor: return 4;
    case Activity::Cpu:       return 3;
    case Activity::Disk:      return 2;
    case Activity::Recent:    return 1;
    default:                  return 0;
    }
}

constexpr double kBusyCpuPerCore = 10.0;    // % of ONE core
constexpr double kBusyIoKBps     = 256.0;
constexpr qint64 kGraceMs        = 20000;   // "was active" memory
// Main workload: an app must load at least half a core (or ~5 MB/s of disk) to
// be "the" workload, and a challenger must load TWICE as much to take over —
// so the choice doesn't flip between two similar apps every second.
constexpr double kMainMinLoad    = 50.0;
constexpr double kMainTakeover   = 2.0;
QString          s_mainName;

} // namespace

QVector<ReliefCandidate> ReliefTool::candidates(qint64 minMemoryKB, int focusedPid)
{
    QVector<ReliefCandidate> out;
    const QString me = currentUserName();
    const int selfPid = static_cast<int>(getpid());
    static const int ncpu = qMax(1, QThread::idealThreadCount());

    // Never suspend our own ancestor chain (the terminal + shell + node/claude
    // that launched gt-stacer) nor anything sharing our controlling-terminal
    // session — freezing any of them would hang the user's working session,
    // exactly the SIGSTOP incident this guards against.
    const QSet<int> ancestors = ancestorPids(selfPid);
    const int mySid = static_cast<int>(getsid(0));

    const QVector<ProcessData> procs = ProcessInfo::processes();
    const QHash<int, ProcNode> table = procTable(procs);

    // ── Disk I/O rate of every process of ours (delta since the previous call).
    static QHash<int, qint64> s_prevIo;
    static QElapsedTimer s_ioClock;
    const double dtSec = s_ioClock.isValid() ? s_ioClock.restart() / 1000.0 : 0.0;
    if (!s_ioClock.isValid()) s_ioClock.start();
    QHash<int, qint64> curIo;
    QHash<int, double> ioRate;
    for (const ProcessData &p : procs) {
        if (p.user != me) continue;
        const qint64 bytes = procIoBytes(p.pid);
        if (bytes < 0) continue;
        curIo.insert(p.pid, bytes);
        // Guard against a tiny interval (two back-to-back calls) turning a small
        // byte delta into an absurd rate; real refreshes are ~1-2 s apart.
        if (dtSec > 0.2 && s_prevIo.contains(p.pid)) {
            const qint64 delta = bytes - s_prevIo.value(p.pid);
            if (delta > 0) ioRate.insert(p.pid, double(delta) / 1024.0 / dtSec);
        }
    }
    s_prevIo = curIo;

    // ── Which APPS are in use: the strongest reason per app root.
    struct Why { Activity a = Activity::None; QString via; int pid = 0; };
    QHash<int, Why> appWhy;
    auto mark = [&](int pid, Activity a) {
        if (!table.contains(pid) || table.value(pid).user != me) return;
        const int root = appRootOf(pid, table, me);
        Why &w = appWhy[root];
        if (rank(a) > rank(w.a)) { w.a = a; w.pid = pid; w.via = table.value(pid).comm; }
    };
    if (focusedPid > 0 && focusedPid != selfPid) mark(focusedPid, Activity::Focused);
    for (int pid : audioPids())     mark(pid, Activity::Audio);
    for (int pid : inhibitorPids()) mark(pid, Activity::Inhibitor);
    for (const ProcessData &p : procs) {
        if (p.user != me || p.status == "T" || p.status == "t") continue;
        if (p.cpuPercent * ncpu >= kBusyCpuPerCore)       mark(p.pid, Activity::Cpu);
        else if (ioRate.value(p.pid) >= kBusyIoKBps)      mark(p.pid, Activity::Disk);
    }

    // Remember when each app was last active, so a short pause in a busy job
    // (linking, waiting for the network) doesn't make it look idle.
    static QHash<int, qint64> s_lastActive;
    static QElapsedTimer s_clock;
    if (!s_clock.isValid()) s_clock.start();
    const qint64 now = s_clock.elapsed();
    for (auto it = appWhy.cbegin(); it != appWhy.cend(); ++it)
        if (it->a != Activity::None && it->a != Activity::Focused) s_lastActive.insert(it.key(), now);
    for (auto it = s_lastActive.begin(); it != s_lastActive.end();) {
        if (!table.contains(it.key()) || now - it.value() > kGraceMs) it = s_lastActive.erase(it);
        else ++it;
    }

    // ── Load per APP (all of our running processes, not only the freezable
    // ones: a compile in a terminal can be the main workload too).
    QHash<int, double> appLoad;
    QHash<int, QPair<double, QString>> heaviest;   // app root → its heaviest process
    for (const ProcessData &p : procs) {
        if (p.user != me || p.status == "T" || p.status == "t" || p.pid == selfPid) continue;
        const double load = p.cpuPercent * ncpu + ioRate.value(p.pid) / 1024.0 * 10.0;
        if (load <= 0) continue;
        const int root = appRootOf(p.pid, table, me);
        appLoad[root] += load;
        if (load > heaviest.value(root).first) heaviest[root] = {load, p.name};
    }
    int best = 0;
    for (auto it = appLoad.cbegin(); it != appLoad.cend(); ++it)
        if (!best || it.value() > appLoad.value(best)) best = it.key();
    static int s_mainApp = 0;
    const bool mainAlive = s_mainApp && table.contains(s_mainApp);
    const double cur = mainAlive ? appLoad.value(s_mainApp) : 0.0;
    const double top = best ? appLoad.value(best) : 0.0;
    if (mainAlive && cur >= kMainMinLoad && cur * kMainTakeover >= top) {
        // the current main workload keeps its place
    } else if (mainAlive && s_lastActive.contains(s_mainApp) && top < kMainMinLoad * kMainTakeover) {
        // a short pause (linking, waiting for the network) — still the main job
    } else {
        s_mainApp = top >= kMainMinLoad ? best : 0;
    }
    s_mainName = s_mainApp ? heaviest.value(s_mainApp).second : QString();
    if (s_mainApp && s_mainName.isEmpty() && table.contains(s_mainApp)) s_mainName = table.value(s_mainApp).comm;

    for (const auto &p : procs) {
        if (p.pid <= 1)                      continue;   // init / kernel
        if (p.pid == selfPid)                continue;   // never freeze ourselves
        if (p.user != me)                    continue;   // only our own processes
        if (p.memoryKB < minMemoryKB)        continue;   // too small to matter
        if (p.status == "T" || p.status == "t") continue; // already stopped
        if (p.status == "Z")                 continue;   // zombie
        if (p.name.compare("gt-stacer", Qt::CaseInsensitive) == 0) continue;
        if (ancestors.contains(p.pid))       continue;   // our terminal/shell/parent tree
        if (mySid > 0 && sessionOf(p.pid) == mySid) continue; // same terminal session
        if (isTerminalOrShell(p.name.toLower())) continue;    // any shell/terminal/node
        if (ProcessInfo::isCriticalProcess(p.pid, p.name)) continue;
        if (isDesktopShell(p.name.toLower())) continue;

        ReliefCandidate c;
        c.pid        = p.pid;
        c.name       = p.name;
        c.user       = p.user;
        c.memoryKB   = p.memoryKB;
        c.cpuPercent = p.cpuPercent;
        c.status     = p.status;
        c.ioKBps     = ioRate.value(p.pid);
        c.appRoot    = appRootOf(p.pid, table, me);
        const Why w  = appWhy.value(c.appRoot);
        if (w.a != Activity::None) {
            c.activity  = w.a;
            c.activeVia = w.pid == c.pid ? QString() : w.via;
        } else if (s_lastActive.contains(c.appRoot)) {
            c.activity  = Activity::Recent;
        }
        c.appLoad      = appLoad.value(c.appRoot);
        c.mainWorkload = s_mainApp && c.appRoot == s_mainApp;
        out << c;
    }

    std::sort(out.begin(), out.end(), [](const ReliefCandidate &a, const ReliefCandidate &b) {
        return a.memoryKB > b.memoryKB;
    });
    return out;
}

QString ReliefTool::mainWorkloadName() { return s_mainName; }

QVector<int> ReliefTool::appTreePids(int pid)
{
    const QString me = currentUserName();
    const QVector<ProcessData> procs = ProcessInfo::processes();
    const QHash<int, ProcNode> table = procTable(procs);
    if (!table.contains(pid)) return {};
    const int root = appRootOf(pid, table, me);
    QVector<int> out;
    for (auto it = table.cbegin(); it != table.cend(); ++it)
        if (it->user == me && appRootOf(it.key(), table, me) == root) out << it.key();
    return out;
}

int ReliefTool::suspendPids(const QVector<int> &pids)
{
    int ok = 0;
    for (int pid : pids)
        if (ProcessInfo::suspend(pid)) ++ok;
    return ok;
}

int ReliefTool::resumePids(const QVector<int> &pids)
{
    int ok = 0;
    for (int pid : pids)
        if (ProcessInfo::resume(pid)) ++ok;
    return ok;
}

bool ReliefTool::dropCaches()
{
    // sync flushes dirty pages first so the drop actually frees the maximum.
    // The command string is a fixed literal (no user input) so the shell form
    // carries no injection risk; it goes through execProgram so Flatpak routes
    // it to the host via flatpak-spawn.
    return CommandUtil::execProgram(
        "pkexec", {"sh", "-c", "sync; echo 3 > /proc/sys/vm/drop_caches"}, 20000) == 0;
}

// ── Disk-I/O relief ──────────────────────────────────────────────────────────
double ReliefTool::ioPressure()
{
    QFile f("/proc/pressure/io");
    if (!f.open(QIODevice::ReadOnly))
        return -1;   // no PSI on this kernel
    // First line: "some avg10=NN.NN avg60=... avg300=... total=..."
    const QByteArray first = f.readLine();
    static const QRegularExpression re("avg10=([0-9.]+)");
    const auto m = re.match(QString::fromLatin1(first));
    return m.hasMatch() ? m.captured(1).toDouble() : -1;
}

int ReliefTool::easeIoPids(const QVector<int> &pids)
{
    int ok = 0;
    for (int pid : pids) {
        if (pid <= 1) continue;
        // ionice -c3 (idle) -p PID. No shell; our own processes need no root.
        if (CommandUtil::execProgram(
                "ionice", {"-c", "3", "-p", QString::number(pid)}, 5000) == 0)
            ++ok;
    }
    return ok;
}

QString ReliefTool::rootDisk()
{
    // Device backing "/", then its parent kernel disk (handles partitions, LVM,
    // dm-crypt — PKNAME walks to the underlying disk).
    const QString src = CommandUtil::execProgramOutput("findmnt", {"-no", "SOURCE", "/"}, 5000).trimmed();
    if (src.isEmpty()) return {};
    const QString pk = CommandUtil::execProgramOutput("lsblk", {"-no", "PKNAME", src}, 5000);
    for (const QString &l : pk.split('\n'))
        if (!l.trimmed().isEmpty()) return l.trimmed();
    // Fallback: strip a partition suffix from the basename (sda2→sda, nvme0n1p2→nvme0n1).
    QString d = src.section('/', -1);
    while (!d.isEmpty() && d.back().isDigit()) d.chop(1);
    if (d.endsWith('p')) d.chop(1);
    return d;
}

QStringList ReliefTool::schedulers(const QString &disk)
{
    if (!isSafeSysToken(disk)) return {};
    QFile f("/sys/block/" + disk + "/queue/scheduler");
    if (!f.open(QIODevice::ReadOnly)) return {};
    QStringList out;
    for (QString tok : QString::fromLatin1(f.readAll()).simplified().split(' ', Qt::SkipEmptyParts))
        out << tok.remove('[').remove(']');
    return out;
}

QString ReliefTool::ioScheduler(const QString &disk)
{
    if (!isSafeSysToken(disk)) return {};
    QFile f("/sys/block/" + disk + "/queue/scheduler");
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QString s = QString::fromLatin1(f.readAll());
    static const QRegularExpression re("\\[([a-z0-9-]+)\\]");   // active is bracketed
    const auto m = re.match(s);
    return m.hasMatch() ? m.captured(1) : s.simplified();
}

bool ReliefTool::setScheduler(const QString &disk, const QString &sched)
{
    if (!isSafeSysToken(disk) || !isSafeSysToken(sched))
        return false;   // never build a privileged command from untrusted tokens
    // Try to load the module (bfq is often not loaded by default), then activate.
    // Both tokens are validated to [a-z0-9-], so the interpolation is injection-safe.
    const QString cmd = QString("modprobe %1 2>/dev/null; echo %1 > /sys/block/%2/queue/scheduler")
                            .arg(sched, disk);
    return CommandUtil::execProgram("pkexec", {"sh", "-c", cmd}, 10000) == 0;
}

// ── Frozen-process journal ───────────────────────────────────────────────────
namespace {

QString journalDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/gt-stacer";
}

// /proc/<pid>/stat → {state letter, start time in clock ticks}; {} if gone.
// Fields after the ")" of the command name: [0]=state (field 3) … [19]=starttime (field 22).
QPair<QChar, QString> procStateAndStart(int pid)
{
    QFile f(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray s = f.readAll();
    const int close = s.lastIndexOf(')');
    if (close < 0) return {};
    const QList<QByteArray> fields = s.mid(close + 2).split(' ');
    if (fields.size() < 20) return {};
    return {QChar(fields[0].isEmpty() ? ' ' : fields[0][0]), QString::fromLatin1(fields[19])};
}

} // namespace

void ReliefTool::saveFrozenJournal(const QVector<int> &pids)
{
    const int self = static_cast<int>(getpid());
    const QString path = journalDir() + QStringLiteral("/relief-frozen-%1").arg(self);
    if (pids.isEmpty()) { QFile::remove(path); return; }
    QDir().mkpath(journalDir());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;
    QTextStream out(&f);
    out << "owner " << self << ' ' << procStateAndStart(self).second << '\n';
    for (int pid : pids) {
        const auto st = procStateAndStart(pid);
        if (!st.second.isEmpty()) out << pid << ' ' << st.second << '\n';
    }
    out.flush();
    f.commit();
}

int ReliefTool::recoverFrozenJournal()
{
    int thawed = 0;
    const QDir dir(journalDir());
    for (const QString &name : dir.entryList({"relief-frozen-*"}, QDir::Files)) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QStringList lines = QString::fromLatin1(f.readAll()).split('\n', Qt::SkipEmptyParts);
        f.close();
        if (lines.isEmpty()) { QFile::remove(f.fileName()); continue; }
        // Is the instance that wrote it still running? Then its freezes are its own.
        const QStringList owner = lines.first().split(' ');
        if (owner.size() >= 3 && owner[0] == "owner") {
            const int opid = owner[1].toInt();
            if (opid == static_cast<int>(getpid())) continue;
            if (opid > 0 && procStateAndStart(opid).second == owner[2]) continue;   // alive
        }
        for (int i = 1; i < lines.size(); ++i) {
            const QStringList e = lines[i].split(' ');
            if (e.size() < 2) continue;
            const int pid = e[0].toInt();
            const auto st = procStateAndStart(pid);
            // Same process (start time matches) and still stopped → thaw it.
            if (pid > 1 && st.second == e[1] && (st.first == 'T' || st.first == 't')
                && ::kill(pid, SIGCONT) == 0)
                ++thawed;
        }
        QFile::remove(f.fileName());
    }
    return thawed;
}
