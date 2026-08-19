#include "relief_tool.h"
#include "../Info/process_info.h"
#include "../Utils/command_util.h"
#include <QSet>
#include <QFile>
#include <QByteArray>
#include <QList>
#include <QHash>
#include <QElapsedTimer>
#include <QRegularExpression>
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

QVector<ReliefCandidate> ReliefTool::candidates(qint64 minMemoryKB)
{
    QVector<ReliefCandidate> out;
    const QString me = currentUserName();
    const int selfPid = static_cast<int>(getpid());

    // Never suspend our own ancestor chain (the terminal + shell + node/claude
    // that launched gt-stacer) nor anything sharing our controlling-terminal
    // session — freezing any of them would hang the user's working session,
    // exactly the SIGSTOP incident this guards against.
    const QSet<int> ancestors = ancestorPids(selfPid);
    const int mySid = static_cast<int>(getsid(0));

    for (const auto &p : ProcessInfo::processes()) {
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
        out << c;
    }

    // Disk I/O rate per candidate: delta of block-layer bytes since the previous
    // call, divided by the wall time between calls. First call reads 0 (no prior
    // sample). State is process-static so successive refreshes give a live rate.
    static QHash<int, qint64> s_prevIo;
    static QElapsedTimer s_ioClock;
    const double dtSec = s_ioClock.isValid() ? s_ioClock.restart() / 1000.0 : 0.0;
    if (!s_ioClock.isValid()) s_ioClock.start();
    QHash<int, qint64> curIo;
    for (ReliefCandidate &c : out) {
        const qint64 bytes = procIoBytes(c.pid);
        if (bytes < 0) continue;
        curIo.insert(c.pid, bytes);
        // Guard against a tiny interval (two back-to-back calls) turning a small
        // byte delta into an absurd rate; real refreshes are ~1-2 s apart.
        if (dtSec > 0.2 && s_prevIo.contains(c.pid)) {
            const qint64 delta = bytes - s_prevIo.value(c.pid);
            if (delta > 0) c.ioKBps = double(delta) / 1024.0 / dtSec;
        }
    }
    s_prevIo = curIo;

    std::sort(out.begin(), out.end(), [](const ReliefCandidate &a, const ReliefCandidate &b) {
        return a.memoryKB > b.memoryKB;
    });
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
