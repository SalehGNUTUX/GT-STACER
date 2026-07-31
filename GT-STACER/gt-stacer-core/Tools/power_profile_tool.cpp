#include "power_profile_tool.h"
#include "../Utils/command_util.h"
#include <QFile>
#include <QRegularExpression>

namespace {
QString readSys(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}
const char *kGov0 = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor";
const char *kGovAvail = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors";
}

// ── power-profiles-daemon ───────────────────────────────────────────────────
bool PowerProfileTool::ppdAvailable()
{
    return CommandUtil::commandExists("powerprofilesctl");
}

QStringList PowerProfileTool::profiles()
{
    // `powerprofilesctl list` prints a block per profile; the header line is the
    // profile id followed by ':' (the active one is prefixed with "* ").
    const QString out = CommandUtil::execProgramOutput("powerprofilesctl", {"list"}, 6000);
    static const QRegularExpression re(QStringLiteral("^[*\\s]*([a-z][a-z-]*):\\s*$"));
    QStringList ids;
    for (const QString &line : out.split('\n')) {
        const auto m = re.match(line);
        if (m.hasMatch()) ids << m.captured(1);
    }
    return ids;
}

QString PowerProfileTool::activeProfile()
{
    if (!ppdAvailable()) return {};
    return CommandUtil::execProgramOutput("powerprofilesctl", {"get"}, 6000).trimmed();
}

bool PowerProfileTool::setProfile(const QString &id)
{
    if (!CommandUtil::isSafeIdentifier(id)) return false;
    return CommandUtil::execProgram("powerprofilesctl", {"set", id}, 8000) == 0;
}

// ── cpufreq governor fallback ────────────────────────────────────────────────
bool PowerProfileTool::cpufreqAvailable()
{
    return QFile::exists(QString::fromLatin1(kGov0));
}

QStringList PowerProfileTool::governors()
{
    return readSys(QString::fromLatin1(kGovAvail)).split(' ', Qt::SkipEmptyParts);
}

QString PowerProfileTool::activeGovernor()
{
    return readSys(QString::fromLatin1(kGov0));
}

bool PowerProfileTool::setGovernor(const QString &gov)
{
    // Only allow a governor the kernel actually advertises — the value is then
    // interpolated into a root shell loop that writes every CPU's governor file.
    if (!governors().contains(gov)) return false;
    const QString cmd = QStringLiteral(
        "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; "
        "do echo %1 > \"$f\"; done").arg(gov);
    return CommandUtil::execProgram("pkexec", {"sh", "-c", cmd}, 15000) == 0;
}

// ── TLP ─────────────────────────────────────────────────────────────────────
bool PowerProfileTool::tlpAvailable()
{
    return CommandUtil::commandExists("tlp");
}

// ── Sleep/idle inhibitors ────────────────────────────────────────────────────
bool PowerProfileTool::inhibitAvailable()
{
    return CommandUtil::commandExists("systemd-inhibit");
}

QStringList PowerProfileTool::sleepBlockers()
{
    // `systemd-inhibit --list --no-legend` columns are:
    //   WHO UID USER PID COMM WHAT [WHY…] MODE
    // WHY can contain spaces, but WHAT is always field 5 and MODE is the last
    // field. Only mode=block inhibitors covering sleep/idle actually prevent it
    // (delay-mode ones from ModemManager/UPower/NetworkManager do not).
    const QString out = CommandUtil::execProgramOutput(
        "systemd-inhibit", {"--list", "--no-legend"}, 6000);
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    QStringList who;
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        const QStringList f = line.split(ws, Qt::SkipEmptyParts);
        if (f.size() < 7) continue;
        if (f.last() != "block") continue;
        const QString what = f.at(5);
        if (what.contains("sleep") || what.contains("idle"))
            who << f.first();
    }
    return who;
}
