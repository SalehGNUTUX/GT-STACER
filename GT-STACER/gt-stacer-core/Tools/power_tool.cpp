#include "power_tool.h"
#include "../Utils/command_util.h"
#include <QFile>

namespace {

// Ask logind (the same backend that actually performs the action) whether it
// can. Returns the raw verdict: "yes" (allowed, no auth), "challenge" (allowed,
// needs polkit auth), "no"/"na" (not possible), or empty if logind/busctl is
// unreachable. This catches cases the kernel's /sys/power/state can't — e.g.
// the kernel advertises "disk" but there is no usable swap/resume device, so
// logind reports "na" and `systemctl hibernate` would fail.
QString logindCan(const QString &method)
{
    const QString out = CommandUtil::execProgramOutput(
        "busctl",
        {"call", "org.freedesktop.login1", "/org/freedesktop/login1",
         "org.freedesktop.login1.Manager", method},
        8000);
    const int q1 = out.indexOf('"');
    if (q1 < 0) return {};
    const int q2 = out.indexOf('"', q1 + 1);
    if (q2 < 0) return {};
    return out.mid(q1 + 1, q2 - q1 - 1).trimmed();
}

bool kernelSupportsSleep(const QString &token)
{
    QFile f("/sys/power/state");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    return QString::fromLatin1(f.readAll()).contains(token);
}

} // namespace

bool PowerTool::perform(Action a)
{
    // systemctl talks to logind, which is the correct, polkit-mediated path for
    // power actions. We deliberately avoid `shutdown`/`halt` binaries so a
    // single code path covers suspend/hibernate too.
    return CommandUtil::execProgram("systemctl", {actionVerb(a)}, 15000) == 0;
}

QString PowerTool::actionVerb(Action a)
{
    switch (a) {
    case Shutdown:  return "poweroff";
    case Reboot:    return "reboot";
    case Suspend:   return "suspend";
    case Hibernate: return "hibernate";
    }
    return "poweroff";
}

bool PowerTool::isAvailable(Action a)
{
    if (a == Shutdown || a == Reboot) return true;
    if (a != Suspend && a != Hibernate) return false;

    // Prefer logind's own verdict — it's the source of truth for what
    // `systemctl suspend`/`hibernate` will actually do. "yes"/"challenge" both
    // mean the action can run (challenge just adds a polkit prompt).
    const QString verdict = logindCan(a == Suspend ? "CanSuspend" : "CanHibernate");
    if (!verdict.isEmpty())
        return verdict == "yes" || verdict == "challenge";

    // logind/busctl unreachable — fall back to the kernel's advertised states.
    return kernelSupportsSleep(a == Suspend ? "mem" : "disk");
}
