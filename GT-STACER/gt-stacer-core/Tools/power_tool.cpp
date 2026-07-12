#include "power_tool.h"
#include "../Utils/command_util.h"
#include <QFile>

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

    QFile f("/sys/power/state");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString states = QString::fromLatin1(f.readAll());
    if (a == Suspend)   return states.contains("mem");
    if (a == Hibernate) return states.contains("disk");
    return false;
}
