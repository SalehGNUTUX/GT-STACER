#include "relief_tool.h"
#include "../Info/process_info.h"
#include "../Utils/command_util.h"
#include <QSet>
#include <QFile>
#include <QByteArray>
#include <QList>
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
