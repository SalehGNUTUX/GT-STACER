#include "firewall_tool.h"
#include "../Utils/command_util.h"
#include <QFile>
#include <QRegularExpression>

namespace {
// Parse `ufw status numbered` rows:
//   [ 1] 22/tcp                     ALLOW IN    Anywhere
QVector<FwRule> parseUfw(const QString &text)
{
    static const QRegularExpression head(QStringLiteral("^\\[\\s*(\\d+)\\]\\s+(.*)$"));
    static const QRegularExpression gap(QStringLiteral("\\s{2,}"));
    QVector<FwRule> out;
    for (const QString &line : text.split('\n')) {
        const auto m = head.match(line);
        if (!m.hasMatch()) continue;
        const QStringList cols = m.captured(2).trimmed().split(gap);
        FwRule r;
        r.number = m.captured(1).toInt();
        r.to     = cols.value(0);
        r.action = cols.value(1);
        r.from   = cols.value(2);
        out << r;
    }
    return out;
}

// Run an optional ufw mutation and then `ufw status numbered` in ONE pkexec, so
// only a single polkit prompt appears. `mutation` is a fixed, validated ufw
// sub-command (no untrusted text); it is empty for a plain listing.
QVector<FwRule> ufwApply(const QString &mutation)
{
    const QString script = mutation.isEmpty()
        ? QStringLiteral("ufw status numbered")
        : mutation + QStringLiteral(" >/dev/null 2>&1; ufw status numbered");
    const QString out = CommandUtil::execProgramOutput("pkexec", {"sh", "-c", script}, 20000);
    return parseUfw(out);
}

QVector<FwRule> firewalldList()
{
    const QString ports = CommandUtil::execProgramOutput(
        "firewall-cmd", {"--list-ports"}, 8000).trimmed();
    QVector<FwRule> out;
    for (const QString &p : ports.split(' ', Qt::SkipEmptyParts)) {
        FwRule r; r.to = p; r.action = "ALLOW"; r.from = "Anywhere";
        out << r;
    }
    return out;
}
}

FirewallTool::Backend FirewallTool::backend()
{
    if (CommandUtil::commandExists("ufw"))          return Ufw;
    if (CommandUtil::commandExists("firewall-cmd"))  return Firewalld;
    return None;
}

QString FirewallTool::backendName()
{
    switch (backend()) {
    case Ufw:       return "ufw";
    case Firewalld: return "firewalld";
    default:        return {};
    }
}

bool FirewallTool::isEnabled()
{
    if (backend() == Ufw) {
        // /etc/ufw/ufw.conf is world-readable, so we report state without root.
        QFile f("/etc/ufw/ufw.conf");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString c = QString::fromUtf8(f.readAll());
            return c.contains(QRegularExpression("ENABLED\\s*=\\s*yes",
                                                 QRegularExpression::CaseInsensitiveOption));
        }
        return false;
    }
    if (backend() == Firewalld)
        return CommandUtil::execProgramOutput("firewall-cmd", {"--state"}, 5000)
                   .trimmed() == "running";
    return false;
}

QVector<FwRule> FirewallTool::rules()
{
    if (backend() == Ufw)       return ufwApply({});
    if (backend() == Firewalld) return firewalldList();
    return {};
}

QVector<FwRule> FirewallTool::setEnabled(bool on)
{
    if (backend() == Ufw)
        return ufwApply(on ? QStringLiteral("ufw --force enable")
                           : QStringLiteral("ufw disable"));
    if (backend() == Firewalld) {
        CommandUtil::execProgram(
            "pkexec", {"systemctl", on ? "enable" : "disable", "--now", "firewalld"}, 15000);
        return firewalldList();
    }
    return {};
}

QVector<FwRule> FirewallTool::addRule(int port, const QString &proto, bool allow)
{
    if (port < 1 || port > 65535)         return rules();
    if (proto != "tcp" && proto != "udp") return rules();
    const QString spec = QString("%1/%2").arg(port).arg(proto);

    if (backend() == Ufw)
        return ufwApply(QStringLiteral("ufw %1 %2").arg(allow ? "allow" : "deny", spec));
    if (backend() == Firewalld) {
        if (allow) {
            CommandUtil::execProgram(
                "pkexec", {"firewall-cmd", "--permanent", "--add-port=" + spec}, 15000);
            CommandUtil::execProgram("pkexec", {"firewall-cmd", "--reload"}, 15000);
        }
        return firewalldList();
    }
    return {};
}

QVector<FwRule> FirewallTool::deleteRule(const FwRule &rule)
{
    if (backend() == Ufw) {
        if (rule.number <= 0) return rules();
        return ufwApply(QStringLiteral("ufw --force delete %1").arg(rule.number));
    }
    if (backend() == Firewalld) {
        if (!rule.to.isEmpty()) {
            CommandUtil::execProgram(
                "pkexec", {"firewall-cmd", "--permanent", "--remove-port=" + rule.to}, 15000);
            CommandUtil::execProgram("pkexec", {"firewall-cmd", "--reload"}, 15000);
        }
        return firewalldList();
    }
    return {};
}
