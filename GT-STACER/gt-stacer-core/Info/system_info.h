#pragma once
#include <QString>

struct SystemData {
    QString hostname;
    QString username;
    QString osName;
    QString osVersion;
    QString kernelVersion;
    QString architecture;
    QString desktopEnv;
    QString displayServer;  // X11 or Wayland
    QString shellName;
    qint64  uptimeSeconds = 0;
    QString uptimeFormatted;
    QString cpuModel;
    int     cpuCores = 0;
    qint64  totalRamMB = 0;
};

class SystemInfo {
public:
    static SystemData info();
    static QString    osRelease(const QString &key);
    static qint64     uptime();
    static QString    displayServer();
};
