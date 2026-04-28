#include "process_info.h"
#include "../Utils/command_util.h"
#include "../Utils/file_util.h"
#include <QDir>
#include <QRegularExpression>
#include <algorithm>
#include <signal.h>

QVector<ProcessData> ProcessInfo::processes()
{
    QVector<ProcessData> result;

    auto lines = CommandUtil::execLines(
        "ps -eo pid,user,stat,pcpu,rss,comm,cmd --no-headers --sort=-%cpu");

    for (const auto &line : lines) {
        auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 6) continue;

        ProcessData p;
        p.pid        = parts[0].toInt();
        p.user       = parts[1];
        p.status     = parts[2];
        p.cpuPercent = parts[3].toDouble();
        p.memoryKB   = parts[4].toLongLong();
        p.name       = parts[5];
        p.command    = parts.mid(6).join(' ');
        result << p;
    }
    return result;
}

bool ProcessInfo::kill(int pid)
{
    return ::kill(pid, SIGTERM) == 0;
}

int ProcessInfo::count()
{
    auto entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return static_cast<int>(std::count_if(entries.cbegin(), entries.cend(),
        [](const QString &e){ return e.toInt() > 0; }));
}
