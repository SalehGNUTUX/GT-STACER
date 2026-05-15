#include "process_info.h"
#include "../Utils/file_util.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

// We read /proc/<pid>/stat directly instead of shelling out to ps. CPU% is
// computed as a delta between two samples per PID using a thread-local cache.
namespace {

struct ProcSample {
    qint64 utime  = 0;   // user-mode jiffies
    qint64 stime  = 0;   // kernel-mode jiffies
    qint64 startTime = 0;
    qint64 sampledAtMs = 0;
};

QHash<int, ProcSample> &lastSamples()
{
    static QHash<int, ProcSample> s;
    return s;
}

QMutex &lastSamplesMutex()
{
    static QMutex m;
    return m;
}

QString resolveUser(uid_t uid)
{
    // Cache uid → name to avoid repeated getpwuid() calls.
    static QHash<uid_t, QString> cache;
    auto it = cache.find(uid);
    if (it != cache.end()) return it.value();
    QString name;
    if (auto *pw = getpwuid(uid)) name = QString::fromLocal8Bit(pw->pw_name);
    else name = QString::number(uid);
    cache.insert(uid, name);
    return name;
}

// /proc/<pid>/stat has the format:
//   pid (comm) state ppid pgrp ... utime stime ...
// The (comm) field may contain spaces and parentheses, so we parse by the
// last ')' rather than splitting on whitespace.
bool parseStat(const QByteArray &raw, ProcessData &out, qint64 &utime, qint64 &stime, qint64 &startTime)
{
    int rparen = raw.lastIndexOf(')');
    if (rparen < 0) return false;
    int lparen = raw.indexOf('(');
    if (lparen < 0 || lparen >= rparen) return false;

    QByteArray comm = raw.mid(lparen + 1, rparen - lparen - 1);
    QByteArray rest = raw.mid(rparen + 2);
    auto fields = rest.split(' ');
    // fields[0] = state, fields[1] = ppid, ..., fields[11] = utime, fields[12] = stime, fields[19] = starttime
    if (fields.size() < 22) return false;

    out.name   = QString::fromLocal8Bit(comm);
    out.status = QString::fromLocal8Bit(fields.value(0));
    utime      = fields.value(11).toLongLong();
    stime      = fields.value(12).toLongLong();
    startTime  = fields.value(19).toLongLong();
    return true;
}

uid_t parseUidFromStatus(const QByteArray &statusFile)
{
    for (const auto &line : statusFile.split('\n')) {
        if (line.startsWith("Uid:")) {
            auto parts = line.split('\t');
            if (parts.size() >= 2) return parts[1].toUInt();
        }
    }
    return 0;
}

qint64 parseRssFromStatus(const QByteArray &statusFile)
{
    for (const auto &line : statusFile.split('\n')) {
        if (line.startsWith("VmRSS:")) {
            auto parts = QString::fromLatin1(line).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) return parts[1].toLongLong();
        }
    }
    return 0;
}

QString readCmdline(int pid)
{
    QFile f(QString("/proc/%1/cmdline").arg(pid));
    if (!f.open(QIODevice::ReadOnly)) return {};
    // Read at most 160 bytes — full command lines can be many KB (Java, browsers)
    // and we only ever display the first part in the table.
    QByteArray raw = f.read(160);
    raw.replace('\0', ' ');
    QString s = QString::fromLocal8Bit(raw).trimmed();
    if (s.size() > 120) s = s.left(120) + QStringLiteral("…");
    return s;
}

} // namespace

QVector<ProcessData> ProcessInfo::processes()
{
    QVector<ProcessData> result;

    // System clock ticks per second (for CPU% normalisation).
    static const long hz = sysconf(_SC_CLK_TCK) > 0 ? sysconf(_SC_CLK_TCK) : 100;
    static const int  ncpu = static_cast<int>(QThread::idealThreadCount());

    QDir proc("/proc");
    const auto entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Unsorted);

    QHash<int, ProcSample> newSamples;
    QHash<int, ProcSample> snapshot;
    {
        QMutexLocker lk(&lastSamplesMutex());
        snapshot = lastSamples();
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (const auto &entry : entries) {
        bool ok = false;
        int pid = entry.toInt(&ok);
        if (!ok || pid <= 0) continue;

        QFile statF(QString("/proc/%1/stat").arg(pid));
        if (!statF.open(QIODevice::ReadOnly)) continue;
        QByteArray statRaw = statF.readAll();
        statF.close();

        qint64 utime = 0, stime = 0, startTime = 0;
        ProcessData p;
        p.pid = pid;
        if (!parseStat(statRaw, p, utime, stime, startTime)) continue;

        // /proc/<pid>/status for uid + VmRSS
        QFile statusF(QString("/proc/%1/status").arg(pid));
        if (statusF.open(QIODevice::ReadOnly)) {
            QByteArray statusRaw = statusF.readAll();
            statusF.close();
            p.user     = resolveUser(parseUidFromStatus(statusRaw));
            p.memoryKB = parseRssFromStatus(statusRaw);
        }

        p.command = readCmdline(pid);
        if (p.command.isEmpty()) p.command = p.name;

        // CPU% delta from previous sample
        ProcSample s;
        s.utime       = utime;
        s.stime       = stime;
        s.startTime   = startTime;
        s.sampledAtMs = nowMs;

        auto prev = snapshot.value(pid);
        if (prev.startTime == startTime && prev.sampledAtMs > 0) {
            qint64 dCpu = (utime + stime) - (prev.utime + prev.stime);
            qint64 dMs  = nowMs - prev.sampledAtMs;
            if (dMs > 0 && hz > 0 && ncpu > 0)
                p.cpuPercent = (100.0 * dCpu * 1000.0) / (static_cast<double>(dMs) * hz * ncpu);
            if (p.cpuPercent < 0)   p.cpuPercent = 0;
            if (p.cpuPercent > 100) p.cpuPercent = 100;
        }
        newSamples.insert(pid, s);

        result << p;
    }

    // Sort by CPU% descending (matches ps --sort=-%cpu behavior).
    std::sort(result.begin(), result.end(), [](const ProcessData &a, const ProcessData &b){
        return a.cpuPercent > b.cpuPercent;
    });

    {
        QMutexLocker lk(&lastSamplesMutex());
        lastSamples() = newSamples;
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
