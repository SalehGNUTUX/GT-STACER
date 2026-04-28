#include "service_tool.h"
#include "../Utils/command_util.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QFile>

InitSystem ServiceTool::s_init     = InitSystem::Unknown;
bool       ServiceTool::s_detected = false;

// ─── stateString ─────────────────────────────────────────────────────────
QString ServiceInfo::stateString() const
{
    switch (state) {
    case ServiceState::Active:   return "active";
    case ServiceState::Inactive: return "inactive";
    case ServiceState::Failed:   return "failed";
    default:                     return "unknown";
    }
}

// ─── Init system detection ───────────────────────────────────────────────
InitSystem ServiceTool::detectInitSystem()
{
    if (s_detected) return s_init;
    s_detected = true;

    // 1. systemd: PID 1 is systemd, or /run/systemd/private exists
    if (QFile::exists("/run/systemd/private") ||
        QFile::exists("/run/systemd/system") ||
        CommandUtil::exec("ps -p 1 -o comm=").trimmed() == "systemd") {
        return s_init = InitSystem::Systemd;
    }

    // 2. OpenRC
    if (QFile::exists("/run/openrc") || CommandUtil::commandExists("rc-service")) {
        return s_init = InitSystem::OpenRC;
    }

    // 3. runit: /run/runit or /etc/runit
    if (QFile::exists("/run/runit") || QDir("/etc/runit/runsvdir").exists()) {
        return s_init = InitSystem::Runit;
    }

    // 4. s6: /run/s6 directory
    if (QFile::exists("/run/s6") || CommandUtil::commandExists("s6-rc")) {
        return s_init = InitSystem::S6;
    }

    // 5. dinit
    if (CommandUtil::commandExists("dinitctl")) {
        return s_init = InitSystem::Dinit;
    }

    // 6. Upstart
    if (QFile::exists("/run/upstart") || CommandUtil::commandExists("initctl")) {
        return s_init = InitSystem::Upstart;
    }

    // 7. SysV fallback
    if (CommandUtil::commandExists("service") || QDir("/etc/init.d").exists()) {
        return s_init = InitSystem::SysV;
    }

    return s_init = InitSystem::Unknown;
}

QString ServiceTool::initSystemName()
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd: return "systemd";
    case InitSystem::OpenRC:  return "OpenRC";
    case InitSystem::Runit:   return "runit";
    case InitSystem::S6:      return "s6";
    case InitSystem::Dinit:   return "dinit";
    case InitSystem::SysV:    return "SysV init";
    case InitSystem::Upstart: return "Upstart";
    default:                  return "Unknown";
    }
}

// ─── Services list (dispatches to init-specific) ─────────────────────────
QVector<ServiceInfo> ServiceTool::services()
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd: return systemdServices();
    case InitSystem::OpenRC:  return openrcServices();
    case InitSystem::Runit:   return runitServices();
    default:                  return sysVServices();
    }
}

// ── Systemd ──────────────────────────────────────────────────────────────
QVector<ServiceInfo> ServiceTool::systemdServices()
{
    QVector<ServiceInfo> result;
    auto lines = CommandUtil::execLines(
        "systemctl list-units --type=service --all --no-pager --no-legend --plain 2>/dev/null");

    for (const auto &line : lines) {
        auto p = line.simplified().split(' ');
        if (p.size() < 4) continue;
        ServiceInfo s;
        s.name = p[0];
        if (s.name.endsWith(".service")) s.name.chop(8);
        QString active = p[2], sub = p[3];
        s.description = p.mid(4).join(' ');
        if      (sub == "running")           s.state = ServiceState::Active;
        else if (active == "failed")         s.state = ServiceState::Failed;
        else if (active == "inactive")       s.state = ServiceState::Inactive;
        s.enabled = CommandUtil::execStatus(
            "systemctl is-enabled " + s.name + ".service >/dev/null 2>&1") == 0;
        result << s;
    }
    return result;
}

bool ServiceTool::systemdAction(const QString &name, const QString &action)
{
    return CommandUtil::execStatus(
        "pkexec systemctl " + action + " " + name + ".service") == 0;
}

// ── OpenRC ───────────────────────────────────────────────────────────────
QVector<ServiceInfo> ServiceTool::openrcServices()
{
    QVector<ServiceInfo> result;
    // List services from /etc/init.d
    QDir initd("/etc/init.d");
    for (const auto &name : initd.entryList(QDir::Files | QDir::Executable)) {
        ServiceInfo s;
        s.name  = name;
        QString st = CommandUtil::exec("rc-service " + name + " status 2>/dev/null").toLower();
        if      (st.contains("started"))  s.state = ServiceState::Active;
        else if (st.contains("stopped"))  s.state = ServiceState::Inactive;
        else if (st.contains("crashed"))  s.state = ServiceState::Failed;
        // Check if in default runlevel
        s.enabled = QFile::exists("/etc/runlevels/default/" + name)
                 || QFile::exists("/etc/runlevels/sysinit/" + name);
        result << s;
    }
    return result;
}

bool ServiceTool::openrcAction(const QString &name, const QString &action)
{
    QString cmd;
    if      (action == "start")   cmd = "pkexec rc-service " + name + " start";
    else if (action == "stop")    cmd = "pkexec rc-service " + name + " stop";
    else if (action == "restart") cmd = "pkexec rc-service " + name + " restart";
    else if (action == "enable")  cmd = "pkexec rc-update add " + name + " default";
    else if (action == "disable") cmd = "pkexec rc-update del " + name + " default";
    else                          return false;
    return CommandUtil::execStatus(cmd) == 0;
}

// ── runit ─────────────────────────────────────────────────────────────────
QVector<ServiceInfo> ServiceTool::runitServices()
{
    QVector<ServiceInfo> result;

    // Services in /var/service (active) and /etc/sv (all)
    QDir allSvc("/etc/sv");
    if (!allSvc.exists()) allSvc.setPath("/etc/runit/sv");

    for (const auto &name : allSvc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        ServiceInfo s;
        s.name    = name;
        s.enabled = QFile::exists("/var/service/" + name)
                 || QFile::exists("/run/runit/runsvdir/default/" + name);
        if (s.enabled) {
            QString st = CommandUtil::exec("sv status " + name + " 2>/dev/null");
            s.state = st.contains("run:") ? ServiceState::Active : ServiceState::Inactive;
        } else {
            s.state = ServiceState::Inactive;
        }
        result << s;
    }
    return result;
}

bool ServiceTool::runitAction(const QString &name, const QString &action)
{
    if      (action == "start")   return CommandUtil::execStatus("pkexec sv start " + name) == 0;
    else if (action == "stop")    return CommandUtil::execStatus("pkexec sv stop "  + name) == 0;
    else if (action == "restart") return CommandUtil::execStatus("pkexec sv restart " + name) == 0;
    else if (action == "enable") {
        // Create symlink in /var/service
        return CommandUtil::execStatus(
            "pkexec ln -sf /etc/sv/" + name + " /var/service/" + name) == 0;
    }
    else if (action == "disable") {
        return CommandUtil::execStatus("pkexec rm -f /var/service/" + name) == 0;
    }
    return false;
}

// ── SysV ──────────────────────────────────────────────────────────────────
QVector<ServiceInfo> ServiceTool::sysVServices()
{
    QVector<ServiceInfo> result;
    QDir initd("/etc/init.d");
    for (const auto &name : initd.entryList(QDir::Files | QDir::Executable)) {
        ServiceInfo s;
        s.name    = name;
        QString st = CommandUtil::exec("service " + name + " status 2>/dev/null").toLower();
        s.state   = st.contains("running") ? ServiceState::Active : ServiceState::Inactive;
        s.enabled = QFile::exists("/etc/rc2.d/S" + name)
                 || QFile::exists("/etc/rc3.d/S" + name);
        result << s;
    }
    return result;
}

bool ServiceTool::sysVAction(const QString &name, const QString &action)
{
    return CommandUtil::execStatus("pkexec service " + name + " " + action) == 0;
}

// ─── Public API (dispatches) ─────────────────────────────────────────────
bool ServiceTool::start(const QString &name)
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd: return systemdAction(name, "start");
    case InitSystem::OpenRC:  return openrcAction(name, "start");
    case InitSystem::Runit:   return runitAction(name, "start");
    default:                  return sysVAction(name, "start");
    }
}

bool ServiceTool::stop(const QString &name)
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd: return systemdAction(name, "stop");
    case InitSystem::OpenRC:  return openrcAction(name, "stop");
    case InitSystem::Runit:   return runitAction(name, "stop");
    default:                  return sysVAction(name, "stop");
    }
}

bool ServiceTool::enable(const QString &name)
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd: return systemdAction(name, "enable");
    case InitSystem::OpenRC:  return openrcAction(name, "enable");
    case InitSystem::Runit:   return runitAction(name, "enable");
    default:                  return false;
    }
}

bool ServiceTool::disable(const QString &name)
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd: return systemdAction(name, "disable");
    case InitSystem::OpenRC:  return openrcAction(name, "disable");
    case InitSystem::Runit:   return runitAction(name, "disable");
    default:                  return false;
    }
}

bool ServiceTool::restart(const QString &name)
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd: return systemdAction(name, "restart");
    case InitSystem::OpenRC:  return openrcAction(name, "restart");
    case InitSystem::Runit:   return runitAction(name, "restart");
    default:                  return sysVAction(name, "restart");
    }
}

QString ServiceTool::status(const QString &name)
{
    switch (detectInitSystem()) {
    case InitSystem::Systemd:
        return CommandUtil::exec("systemctl status " + name + ".service --no-pager 2>&1");
    case InitSystem::OpenRC:
        return CommandUtil::exec("rc-service " + name + " status 2>&1");
    case InitSystem::Runit:
        return CommandUtil::exec("sv status " + name + " 2>&1");
    default:
        return CommandUtil::exec("service " + name + " status 2>&1");
    }
}
