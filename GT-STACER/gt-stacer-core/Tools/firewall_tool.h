#pragma once
#include <QString>
#include <QVector>

// Front-end for the host firewall — ufw (preferred, common on Debian/Ubuntu) or
// firewalld. The enabled state is read cheaply without root (ufw.conf /
// firewall-cmd --state); listing and changing rules go through pkexec via
// CommandUtil, so each of those actions raises one polkit prompt.
struct FwRule {
    int     number = 0;   // ufw rule number (0 for firewalld)
    QString to;           // "22/tcp", a service, or a port
    QString action;       // ALLOW / DENY
    QString from;         // source ("Anywhere", a CIDR, …)
};

class FirewallTool {
public:
    enum Backend { None, Ufw, Firewalld };

    static Backend backend();
    static QString backendName();            // "ufw" | "firewalld" | ""
    static bool    isEnabled();              // no prompt (reads config)

    // Each of these performs its change AND returns the resulting rule list in a
    // SINGLE pkexec call, so the user is asked to authorize only once per action
    // (never twice — mutate then re-list).
    static QVector<FwRule> rules();                                    // list only
    static QVector<FwRule> setEnabled(bool on);                        // toggle + list
    static QVector<FwRule> addRule(int port, const QString &proto, bool allow);
    static QVector<FwRule> deleteRule(const FwRule &rule);
};
