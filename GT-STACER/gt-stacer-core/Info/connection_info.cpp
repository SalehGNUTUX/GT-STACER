#include "connection_info.h"
#include "../Utils/command_util.h"
#include <QRegularExpression>
#include <QStringList>

QVector<Connection> ConnectionInfo::list(bool privileged)
{
    // `ss -tunaHp`: -t tcp, -u udp, -a all (incl. listening), -n numeric,
    // -H no header, -p show owning process. Columns (space-separated):
    //   netid state recv-q send-q local-addr peer-addr [users:(("proc",pid=N,fd=M))]
    QString out;
    if (privileged)
        out = CommandUtil::execProgramOutput("pkexec", {"ss", "-tunaHp"}, 15000);
    else
        out = CommandUtil::execProgramOutput("ss", {"-tunaHp"}, 15000);

    // First name+pid inside the process column, e.g. "firefox",pid=1234
    static const QRegularExpression procRe(QStringLiteral("\"([^\"]*)\",pid=(\\d+)"));

    QVector<Connection> result;
    const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
    result.reserve(lines.size());

    for (const QString &line : lines) {
        const QStringList f = line.simplified().split(' ', Qt::SkipEmptyParts);
        if (f.size() < 6) continue;   // not a socket row

        Connection c;
        c.proto     = f.at(0);
        c.state     = f.at(1);
        c.localAddr = f.at(4);
        c.peerAddr  = f.at(5);

        // Everything past the fixed 6 columns is the process description; join it
        // back so a quoted name containing a space is not truncated.
        if (f.size() > 6) {
            const QString procCol = QStringList(f.mid(6)).join(' ');
            const auto m = procRe.match(procCol);
            if (m.hasMatch()) {
                c.process = m.captured(1);
                c.pid     = m.captured(2).toInt();
            }
        }
        result << c;
    }
    return result;
}
