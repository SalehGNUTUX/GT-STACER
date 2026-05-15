#include "notification_tool.h"
#include "../Utils/command_util.h"

bool NotificationTool::isAvailable()
{
    static const bool found = CommandUtil::commandExists("notify-send");
    return found;
}

bool NotificationTool::notify(const QString &title,
                               const QString &body,
                               Urgency urgency,
                               const QString &iconName,
                               int timeoutMs)
{
    if (!isAvailable()) return false;

    QStringList args;
    args << "--app-name=GT-STACER";

    switch (urgency) {
    case Urgency::Low:      args << "--urgency=low";      break;
    case Urgency::Normal:   args << "--urgency=normal";   break;
    case Urgency::Critical: args << "--urgency=critical"; break;
    }

    if (timeoutMs > 0) args << QString("--expire-time=%1").arg(timeoutMs);
    if (!iconName.isEmpty()) args << QString("--icon=%1").arg(iconName);

    args << title << body;

    return CommandUtil::execProgram("notify-send", args, 5000) == 0;
}
