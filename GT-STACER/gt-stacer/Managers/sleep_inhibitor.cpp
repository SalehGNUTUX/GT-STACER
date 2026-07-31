#include "sleep_inhibitor.h"
#include "../../gt-stacer-core/Tools/power_profile_tool.h"
#include "../../gt-stacer-core/Utils/command_util.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QProcess>
#include <QStringList>

namespace {
const char *kReason = "keep awake";

// Helpers build a QDBusInterface in place (it is a QObject and cannot be copied
// or returned by value). Kept in a macro-free form for clarity.
#define PM_IFACE QDBusInterface pm("org.freedesktop.PowerManagement", \
    "/org/freedesktop/PowerManagement/Inhibit", \
    "org.freedesktop.PowerManagement.Inhibit", QDBusConnection::sessionBus())
#define SS_IFACE QDBusInterface ss("org.freedesktop.ScreenSaver", \
    "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", \
    QDBusConnection::sessionBus())
}

SleepInhibitor::~SleepInhibitor()
{
    unblock();
}

bool SleepInhibitor::dbusBlock()
{
    // Use a SINGLE inhibitor to avoid duplicate entries in the desktop's power
    // UI. PowerManagement is preferred — on KDE/XFCE/… it blocks BOTH automatic
    // sleep and screen locking (they are one "inactivity" category there). Only
    // when that service is absent (e.g. GNOME) do we fall back to ScreenSaver.
    {
        PM_IFACE;
        QDBusReply<uint> r = pm.call("Inhibit", QStringLiteral("GT-STACER"), QString::fromUtf8(kReason));
        if (r.isValid()) { m_pmCookie = r.value(); m_dbusHeld = true; return true; }
    }
    {
        SS_IFACE;
        QDBusReply<uint> r = ss.call("Inhibit", QStringLiteral("GT-STACER"), QString::fromUtf8(kReason));
        if (r.isValid()) { m_ssCookie = r.value(); m_dbusHeld = true; return true; }
    }
    return false;   // no D-Bus service → caller falls back to systemd-inhibit
}

void SleepInhibitor::dbusUnblock()
{
    if (m_pmCookie) { PM_IFACE; pm.call("UnInhibit", m_pmCookie); }
    if (m_ssCookie) { SS_IFACE; ss.call("UnInhibit", m_ssCookie); }
    m_pmCookie = m_ssCookie = 0;
    m_dbusHeld = false;
}

void SleepInhibitor::block()
{
    if (blockedByUs()) return;
    if (dbusBlock()) return;                 // preferred, desktop-integrated path

    // Fallback: hold a logind inhibitor via a long-lived systemd-inhibit process.
    QString prog = "systemd-inhibit";
    QStringList args = {
        "--what=idle:sleep:handle-lid-switch",
        "--who=GT-STACER", "--why=Keep awake", "--mode=block",
        "sleep", "infinity"
    };
    CommandUtil::wrapForHost(prog, args);
    m_proc = new QProcess;
    m_proc->start(prog, args);
    if (!m_proc->waitForStarted(3000)) { delete m_proc; m_proc = nullptr; }
}

void SleepInhibitor::unblock()
{
    if (m_dbusHeld) dbusUnblock();
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
        delete m_proc;
        m_proc = nullptr;
    }
}

bool SleepInhibitor::blockedByUs() const
{
    return m_dbusHeld || (m_proc && m_proc->state() != QProcess::NotRunning);
}

bool SleepInhibitor::available()
{
    // The D-Bus service registers on demand; assume it's usable if the session
    // bus is up, otherwise rely on systemd-inhibit.
    if (QDBusConnection::sessionBus().isConnected()) return true;
    return PowerProfileTool::inhibitAvailable();
}

bool SleepInhibitor::systemInhibited()
{
    // HasInhibit() reflects the desktop's own inhibitors too (including KDE's
    // "Manually block sleep and screen locking"), which logind never sees.
    PM_IFACE;
    QDBusReply<bool> r = pm.call("HasInhibit");
    if (r.isValid()) return r.value();
    // Fall back to logind's block-mode sleep/idle inhibitors.
    return !PowerProfileTool::sleepBlockers().isEmpty();
}
