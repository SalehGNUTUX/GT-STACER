#include "system_info.h"
#include "../Utils/file_util.h"
#include "../Utils/command_util.h"
#include <QSysInfo>
#include <QDir>
#include <unistd.h>
#include <sys/sysinfo.h>

SystemData SystemInfo::info()
{
    SystemData d;
    d.hostname       = QSysInfo::machineHostName();
    d.username       = qEnvironmentVariable("USER", qEnvironmentVariable("LOGNAME"));
    d.kernelVersion  = QSysInfo::kernelVersion();
    d.architecture   = QSysInfo::currentCpuArchitecture();
    d.osName         = osRelease("NAME");
    d.osVersion      = osRelease("VERSION_ID");
    d.desktopEnv     = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    d.displayServer  = displayServer();
    d.shellName      = QDir(qEnvironmentVariable("SHELL")).dirName();
    d.uptimeSeconds  = uptime();

    qint64 secs = d.uptimeSeconds;
    int days = secs / 86400; secs %= 86400;
    int hrs  = secs / 3600;  secs %= 3600;
    int mins = secs / 60;
    if (days > 0) d.uptimeFormatted = QString("%1d %2h %3m").arg(days).arg(hrs).arg(mins);
    else if (hrs > 0) d.uptimeFormatted = QString("%1h %2m").arg(hrs).arg(mins);
    else d.uptimeFormatted = QString("%1m").arg(mins);

    // CPU
    auto cpuLines = FileUtil::readFile("/proc/cpuinfo").split('\n');
    for (const auto &line : cpuLines) {
        if (line.startsWith("model name")) {
            d.cpuModel = line.split(':').last().trimmed();
            break;
        }
    }
    d.cpuCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

    struct sysinfo si{};
    if (sysinfo(&si) == 0)
        d.totalRamMB = (si.totalram * si.mem_unit) / (1024 * 1024);

    return d;
}

QString SystemInfo::osRelease(const QString &key)
{
    auto lines = FileUtil::readFile("/etc/os-release").split('\n');
    for (const auto &line : lines) {
        if (line.startsWith(key + "=")) {
            QString val = line.mid(key.length() + 1).trimmed();
            if (val.startsWith('"')) val = val.mid(1, val.length() - 2);
            return val;
        }
    }
    return {};
}

qint64 SystemInfo::uptime()
{
    QString content = FileUtil::readFile("/proc/uptime");
    return static_cast<qint64>(content.split(' ').first().toDouble());
}

QString SystemInfo::displayServer()
{
    if (!qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty()) return "Wayland";
    if (!qEnvironmentVariable("DISPLAY").isEmpty())         return "X11";
    return "Unknown";
}
