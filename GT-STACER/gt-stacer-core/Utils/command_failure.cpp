#include "command_failure.h"
#include <QRegularExpression>
#include <QStringList>

namespace {

bool any(const QString &low, std::initializer_list<const char *> needles)
{
    for (const char *n : needles) if (low.contains(QLatin1String(n))) return true;
    return false;
}

// The line most worth showing: the last one that looks like an error, else the
// last non-empty line. Progress noise (\r updates) is ignored.
QString relevantLine(const QString &out)
{
    const QStringList lines = out.split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);
    static const QRegularExpression err(
        "(^E: |^error|\\berror\\b|failed|cannot|can't|unable|not found|denied|"
        "not authorized|no space|read-only|problem|conflict|unmet|lock)",
        QRegularExpression::CaseInsensitiveOption);
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString l = lines[i].trimmed();
        if (!l.isEmpty() && err.match(l).hasMatch()) return l.left(300);
    }
    for (int i = lines.size() - 1; i >= 0; --i)
        if (!lines[i].trimmed().isEmpty()) return lines[i].trimmed().left(300);
    return {};
}

} // namespace

CommandFailure::Failure CommandFailure::classify(const CommandResult &r)
{
    Failure f;
    if (r.ok()) return f;
    f.detail = relevantLine(r.output);
    const QString low = r.output.toLower();

    if (r.timedOut) { f.kind = Kind::Timeout; return f; }
    if (!r.started) { f.kind = Kind::NotInstalled; f.subject = r.viaPkexec ? QStringLiteral("pkexec") : r.program; return f; }

    // "sh: 1: foo: not found", "foo: command not found", exit 127 → missing.
    // Checked FIRST: pkexec passes through the inner program's 127, which must
    // not be mistaken for pkexec's own "not authorized" 127.
    static const QRegularExpression notFound("(?:^|\\n)(?:[^\\n:]*: )?(?:\\d+: )?([\\w.+-]+): (?:command )?not found");
    if (r.exitCode == 127) {
        const auto m = notFound.match(r.output);
        if (m.hasMatch()) { f.kind = Kind::NotInstalled; f.subject = m.captured(1); return f; }
    }

    // pkexec's own exit codes (see pkexec(1)): 126 = dialog dismissed,
    // 127 = not authorized / could not authenticate / could not run.
    if (r.viaPkexec && r.exitCode == 126) { f.kind = Kind::AuthCancelled; return f; }
    if (r.viaPkexec && r.exitCode == 127) {
        if (any(low, {"no authentication agent", "no agent"}))       f.kind = Kind::NoAuthAgent;
        else if (any(low, {"cannot run program", "no such file"})) { f.kind = Kind::NotInstalled; f.subject = r.program; }
        else                                                          f.kind = Kind::NotAuthorized;
        return f;
    }

    if (r.exitCode == 127) { f.kind = Kind::NotInstalled; f.subject = r.program; return f; }

    // udisks reports authorization through D-Bus error names.
    if (low.contains("udisks2.error.notauthorizeddismissed")) { f.kind = Kind::AuthCancelled; return f; }
    if (low.contains("udisks2.error.notauthorized"))          { f.kind = Kind::NotAuthorized; return f; }

    // "…lock…, are you root?" is a missing privilege, not a busy lock.
    if (any(low, {"are you root", "must be root", "you need to be root", "requires root",
                  "superuser privileges"}))
        f.kind = Kind::PermissionDenied;
    else if (any(low, {"could not get lock", "unable to acquire the dpkg frontend lock",
                  "unable to lock the administration directory", "unable to lock database",
                  "waiting for cache lock", "is another process using it",
                  "another instance of timeshift", "system management is locked",
                  "waiting for process with pid", "rpmdb: lock", "database is locked",
                  "another app is currently holding"}))
        f.kind = Kind::Busy;
    else if (any(low, {"target is busy", "device or resource busy", "udisks2.error.devicebusy"}))
        f.kind = Kind::DeviceBusy;
    else if (any(low, {"wrong fs type", "bad superblock", "unknown filesystem type", "not a mountable filesystem",
                       "can't read superblock", "structure needs cleaning",
                       "is not a block device"}))
        f.kind = Kind::BadFilesystem;
    else if (any(low, {"dpkg was interrupted"}))
        f.kind = Kind::DpkgInterrupted;
    else if (any(low, {"no space left on device", "not enough disk space", "not enough free space",
                       "insufficient space", "disk quota exceeded"}))
        f.kind = Kind::NoSpace;
    else if (any(low, {"read-only file system"}))
        f.kind = Kind::ReadOnly;
    else if (any(low, {"temporary failure resolving", "could not resolve host", "failed to fetch",
                       "network is unreachable", "no route to host", "curl error",
                       "failed to download metadata", "cannot retrieve repository metadata",
                       "could not retrieve mirrorlist", "failed retrieving file",
                       "connection timed out", "unable to connect"}))
        f.kind = Kind::NoNetwork;
    else if (any(low, {"unable to locate package", "no match for argument", "unable to find a match",
                       "target not found", "has no installation candidate", "no provider of",
                       "not found in package names"}))
        f.kind = Kind::PackageNotFound;
    else if (any(low, {"unmet dependencies", "broken packages", "nothing provides",
                       "unable to satisfy dependencies", "unable to correct problems",
                       "conflicting requirements", "could not satisfy dependencies",
                       "unresolvable", "conflicting packages", "dependency problems"}))
        f.kind = Kind::BrokenDependencies;
    else if (any(low, {"permission denied", "operation not permitted"}))
        f.kind = Kind::PermissionDenied;
    else
        f.kind = Kind::Unknown;
    return f;
}
