#pragma once
#include <QString>
#include <QVector>

// A single active network socket as reported by `ss`.
struct Connection {
    QString proto;       // "tcp" | "udp"
    QString state;       // ESTAB, LISTEN, TIME-WAIT, UNCONN, …
    QString localAddr;   // ip:port (may include %iface or [v6])
    QString peerAddr;    // ip:port, or *:* for a listener
    QString process;     // owning process name ("" if not visible without root)
    int     pid = 0;     // owning pid (0 if unknown)
};

// Lists active TCP/UDP sockets by parsing `ss -tunaHp`. The process/pid columns
// are only populated for sockets the current user owns; pass privileged=true to
// run through pkexec and see every process. All command execution goes through
// CommandUtil so the Flatpak host wrapper applies.
class ConnectionInfo {
public:
    static QVector<Connection> list(bool privileged = false);
};
