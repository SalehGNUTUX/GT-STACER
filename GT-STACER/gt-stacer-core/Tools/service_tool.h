#pragma once
#include <QString>
#include <QVector>

// ─── أنظمة init المدعومة ─────────────────────────────────────────────────
enum class InitSystem {
    Systemd,   // Fedora, Ubuntu, Debian, Arch, openSUSE...  (الأغلبية)
    OpenRC,    // Alpine, Gentoo, Artix, Devuan, Chimera
    Runit,     // Void Linux, Artix (runit flavor)
    S6,        // Artix (s6), Chimera (s6-rc)
    Dinit,     // Artix (dinit), Chimera
    SysV,      // Debian 7-, CentOS 6-, older distros
    Upstart,   // Ubuntu 14.04-, CentOS 6
    Unknown
};

// ─── حالة الخدمة ─────────────────────────────────────────────────────────
enum class ServiceState { Active, Inactive, Failed, Unknown };

struct ServiceInfo {
    QString      name;
    QString      description;
    ServiceState state   = ServiceState::Unknown;
    bool         enabled = false;

    QString stateString() const;
};

// ─── ServiceTool ─────────────────────────────────────────────────────────
class ServiceTool {
public:
    // الكشف عن نظام init
    static InitSystem detectInitSystem();
    static QString    initSystemName();

    // إدارة الخدمات
    static QVector<ServiceInfo> services();
    static bool start(const QString &name);
    static bool stop(const QString &name);
    static bool enable(const QString &name);
    static bool disable(const QString &name);
    static bool restart(const QString &name);
    static QString status(const QString &name);

private:
    static InitSystem s_init;
    static bool       s_detected;

    // Systemd
    static QVector<ServiceInfo> systemdServices();
    static bool systemdAction(const QString &name, const QString &action);

    // OpenRC
    static QVector<ServiceInfo> openrcServices();
    static bool openrcAction(const QString &name, const QString &action);

    // runit
    static QVector<ServiceInfo> runitServices();
    static bool runitAction(const QString &name, const QString &action);

    // SysV
    static QVector<ServiceInfo> sysVServices();
    static bool sysVAction(const QString &name, const QString &action);
};
