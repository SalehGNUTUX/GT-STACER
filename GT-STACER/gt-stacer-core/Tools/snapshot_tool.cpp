#include "snapshot_tool.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace {
const char *kTsConf = "/etc/timeshift/default.json";
SnapshotTool::Backend s_active = SnapshotTool::None;

bool timeshiftConfigured()
{
    return CommandUtil::commandExists("timeshift") && QFile::exists(kTsConf);
}

QString snapperConfig()   // first configured snapper config name, else "root"
{
    const QStringList cfgs = QDir("/etc/snapper/configs")
        .entryList(QDir::Files | QDir::NoDotAndDotDot);
    return cfgs.isEmpty() ? QStringLiteral("root") : cfgs.first();
}
bool snapperConfigured()
{
    return CommandUtil::commandExists("snapper")
        && !QDir("/etc/snapper/configs").entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty();
}

bool zfsPool()
{
    if (!CommandUtil::commandExists("zpool")) return false;
    return !CommandUtil::execProgramOutput("zpool", {"list", "-H", "-o", "name"}, 5000)
                .trimmed().isEmpty();
}

QString displayDateFromTs(QString name)   // "2024-01-02_12-00-01" -> "2024-01-02 12:00:01"
{
    name.replace('_', ' ');
    const int sp = name.indexOf(' ');
    if (sp > 0) { QString t = name.mid(sp + 1); t.replace('-', ':'); name = name.left(sp + 1) + t; }
    return name;
}

QVector<Snapshot> parseTimeshift(const QString &text)
{
    static const QRegularExpression re(QStringLiteral(
        "^\\s*\\d+\\s+>?\\s*(\\d{4}-\\d{2}-\\d{2}_\\d{2}-\\d{2}-\\d{2})\\s+(\\S+)?\\s*(.*?)\\s*$"));
    QVector<Snapshot> out;
    for (const QString &line : text.split('\n')) {
        const auto m = re.match(line);
        if (!m.hasMatch()) continue;
        Snapshot s;
        s.id = m.captured(1);
        s.date = displayDateFromTs(s.id);
        s.tags = m.captured(2);
        s.description = m.captured(3);
        out << s;
    }
    return out;
}

QVector<Snapshot> parseSnapper(const QString &text)
{
    // Rows:  1 | single |  | 2024-01-01 12:00:00 | root | number | my snapshot |
    QVector<Snapshot> out;
    for (const QString &line : text.split('\n')) {
        if (!QRegularExpression("^\\s*\\d+\\s*\\|").match(line).hasMatch()) continue;
        QStringList f = line.split('|');
        for (QString &x : f) x = x.trimmed();
        Snapshot s;
        s.id = f.value(0);
        s.date = f.value(3);
        s.description = f.value(6);
        out << s;
    }
    return out;
}

QVector<Snapshot> parseZfs(const QString &text)
{
    QVector<Snapshot> out;
    for (const QString &line : text.split('\n', Qt::SkipEmptyParts)) {
        const QStringList f = line.split('\t');
        Snapshot s; s.id = f.value(0); s.date = f.value(1); s.size = f.value(2);
        out << s;
    }
    return out;
}
}

// ── detection / selection ────────────────────────────────────────────────────
bool SnapshotTool::hasTimeshift() { return CommandUtil::commandExists("timeshift"); }
bool SnapshotTool::hasSnapper()   { return CommandUtil::commandExists("snapper"); }
bool SnapshotTool::hasZfs()       { return zfsPool(); }

QString SnapshotTool::rootFsType()
{
    QString t = CommandUtil::execProgramOutput("findmnt", {"-n", "-o", "FSTYPE", "/"}, 4000).trimmed();
    if (!t.isEmpty()) return t;
    QFile f("/proc/mounts");   // fallback
    if (f.open(QIODevice::ReadOnly)) {
        for (const QByteArray &l : f.readAll().split('\n')) {
            const QList<QByteArray> c = l.split(' ');
            if (c.size() >= 3 && c[1] == "/") return QString::fromLatin1(c[2]);
        }
    }
    return {};
}

QVector<SnapshotTool::Backend> SnapshotTool::availableBackends()
{
    QVector<Backend> v;
    if (timeshiftConfigured()) v << Timeshift;   // rsync (any FS) or btrfs
    if (snapperConfigured())   v << Snapper;     // btrfs / lvm
    if (zfsPool())             v << Zfs;
    return v;
}

SnapshotTool::Backend SnapshotTool::idealBackend()
{
    const QVector<Backend> av = availableBackends();
    if (av.isEmpty()) return None;
    const QString fs = rootFsType();
    if (fs == "zfs"   && av.contains(Zfs))       return Zfs;
    if (fs == "btrfs") {
        // Prefer a copy-on-write-native engine (instant snapshots).
        if (av.contains(Timeshift) && mode() == "btrfs") return Timeshift;
        if (av.contains(Snapper))                        return Snapper;
        if (av.contains(Timeshift))                      return Timeshift;
    }
    // Non-CoW filesystems (ext4/xfs/…) — Timeshift's rsync mode is the practical
    // choice; otherwise whatever is available.
    if (av.contains(Timeshift)) return Timeshift;
    return av.first();
}

void SnapshotTool::setBackend(Backend b) { s_active = b; }

SnapshotTool::Backend SnapshotTool::backend()
{
    const QVector<Backend> av = availableBackends();
    if (av.isEmpty()) return None;
    if (s_active != None && av.contains(s_active)) return s_active;
    return av.first();
}

QString SnapshotTool::nameOf(Backend b)
{
    switch (b) {
    case Timeshift: return "Timeshift";
    case Snapper:   return "Snapper";
    case Zfs:       return "ZFS";
    default:        return {};
    }
}
QString SnapshotTool::backendName() { return nameOf(backend()); }
bool    SnapshotTool::available()   { return backend() != None; }
bool    SnapshotTool::canRestore()  { const Backend b = backend(); return b == Timeshift || b == Zfs; }

QString SnapshotTool::mode()
{
    if (!timeshiftConfigured()) return {};
    QFile f(kTsConf);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "rsync";
    return QString::fromUtf8(f.readAll())
        .contains(QRegularExpression("\"btrfs_mode\"\\s*:\\s*\"true\"")) ? "btrfs" : "rsync";
}

// ── operations (use the active backend) ──────────────────────────────────────
QVector<Snapshot> SnapshotTool::list()
{
    switch (backend()) {
    case Timeshift:
        return parseTimeshift(CommandUtil::execProgramOutput("pkexec", {"timeshift", "--list"}, 30000));
    case Snapper:
        return parseSnapper(CommandUtil::execProgramOutput(
            "pkexec", {"snapper", "-c", snapperConfig(), "list"}, 15000));
    case Zfs:
        return parseZfs(CommandUtil::execProgramOutput(
            "pkexec", {"zfs", "list", "-t", "snapshot", "-H", "-o", "name,creation,used"}, 15000));
    default:
        return {};
    }
}

bool SnapshotTool::create(const QString &comment)
{
    switch (backend()) {
    case Timeshift:
        return CommandUtil::execProgram(
            "pkexec", {"timeshift", "--create", "--comments", comment, "--scripted"}, 300000) == 0;
    case Snapper:
        return CommandUtil::execProgram(
            "pkexec", {"snapper", "-c", snapperConfig(), "create", "-d", comment}, 60000) == 0;
    case Zfs: {
        const QString ds = CommandUtil::execProgramOutput("zfs", {"list", "-H", "-o", "name", "/"}, 5000).trimmed();
        if (ds.isEmpty()) return false;
        const QString name = ds + "@gt-stacer-" +
            QString::fromUtf8(comment.toUtf8().toBase64()).left(12).remove(QRegularExpression("[^A-Za-z0-9]"));
        return CommandUtil::execProgram("pkexec", {"zfs", "snapshot", name}, 15000) == 0;
    }
    default:
        return false;
    }
}

bool SnapshotTool::remove(const Snapshot &s)
{
    if (!CommandUtil::isSafeIdentifier(s.id)) return false;
    switch (backend()) {
    case Timeshift:
        return CommandUtil::execProgram(
            "pkexec", {"timeshift", "--delete", "--snapshot", s.id, "--scripted"}, 120000) == 0;
    case Snapper:
        return CommandUtil::execProgram(
            "pkexec", {"snapper", "-c", snapperConfig(), "delete", s.id}, 60000) == 0;
    case Zfs:
        return CommandUtil::execProgram("pkexec", {"zfs", "destroy", s.id}, 30000) == 0;
    default:
        return false;
    }
}

bool SnapshotTool::restore(const Snapshot &s)
{
    if (!CommandUtil::isSafeIdentifier(s.id)) return false;
    switch (backend()) {
    case Timeshift:
        return CommandUtil::execProgram(
            "pkexec", {"timeshift", "--restore", "--snapshot", s.id, "--scripted"}, 600000) == 0;
    case Zfs:
        return CommandUtil::execProgram("pkexec", {"zfs", "rollback", "-r", s.id}, 60000) == 0;
    case Snapper:   // rollback is btrfs-boot-specific; not wired into the GUI yet
    default:
        return false;
    }
}
