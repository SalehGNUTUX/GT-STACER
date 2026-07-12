#include "command_util.h"
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QDir>

namespace {
// True when GT-STACER is running inside a Flatpak sandbox. We detect this once
// at startup; the result never changes during a session.
bool runningInFlatpak()
{
    static const bool inside =
        !qEnvironmentVariableIsEmpty("FLATPAK_ID")
        || QFileInfo::exists("/.flatpak-info");
    return inside;
}

// Wrap an argv array with `flatpak-spawn --host` when we're sandboxed, so the
// command runs on the host system rather than inside the sandbox (which has
// no pkexec, no apt, no systemctl). Outside Flatpak this is a no-op.
void wrapForHost(QString &program, QStringList &args)
{
    if (!runningInFlatpak()) return;
    args.prepend(program);
    args.prepend("--host");
    program = "flatpak-spawn";
}
} // namespace

QString CommandUtil::exec(const QString &command)
{
    QProcess process;
    process.start("/bin/sh", {"-c", command});
    process.waitForFinished(10000);
    return process.readAllStandardOutput().trimmed();
}

QStringList CommandUtil::execLines(const QString &command)
{
    QString out = exec(command);
    if (out.isEmpty()) return {};
    return out.split('\n', Qt::SkipEmptyParts);
}

int CommandUtil::execStatus(const QString &command)
{
    QProcess process;
    process.start("/bin/sh", {"-c", command});
    process.waitForFinished(10000);
    return process.exitCode();
}

bool CommandUtil::commandExists(const QString &command)
{
    // Outside Flatpak this is a straight PATH lookup. Inside Flatpak the
    // sandbox PATH almost never matches the host's, so we ask the host via
    // `flatpak-spawn --host which …` instead — otherwise every PackageTool
    // probe ("does apt-get exist?") would falsely report missing.
    if (!runningInFlatpak())
        return !QStandardPaths::findExecutable(command).isEmpty();

    QProcess p;
    p.start("flatpak-spawn", {"--host", "which", command});
    if (!p.waitForFinished(3000)) { p.kill(); return false; }
    return p.exitCode() == 0;
}

QString CommandUtil::execSudo(const QString &command)
{
    return exec(QString("pkexec %1").arg(command));
}

int CommandUtil::execProgram(const QString &program, const QStringList &args, int timeoutMs)
{
    QString prog = program;
    QStringList a = args;
    wrapForHost(prog, a);     // no-op outside Flatpak
    QProcess p;
    p.start(prog, a);
    if (!p.waitForStarted(timeoutMs)) return -1;
    if (!p.waitForFinished(timeoutMs)) { p.kill(); return -1; }
    return p.exitCode();
}

QString CommandUtil::execProgramOutput(const QString &program, const QStringList &args, int timeoutMs)
{
    QString prog = program;
    QStringList a = args;
    wrapForHost(prog, a);
    QProcess p;
    p.start(prog, a);
    if (!p.waitForStarted(timeoutMs)) return {};
    if (!p.waitForFinished(timeoutMs)) { p.kill(); return {}; }
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

bool CommandUtil::pkexecWriteFile(const QString &destPath, const QByteArray &content,
                                   const QString &owner, const QString &group,
                                   const QString &mode)
{
    // Reject destination paths containing shell metacharacters as a defensive measure
    // (pkexec/install do not invoke a shell, but a path with NUL or newline is still invalid).
    if (destPath.contains(QChar(0)) || destPath.contains('\n') || destPath.isEmpty())
        return false;

    // Pick a temp directory that both the sandbox and the host can see.
    //  - Outside Flatpak: regular /tmp is fine.
    //  - Inside Flatpak:  /tmp is private to the sandbox, so we drop the file
    //    into ~/.cache/gt-stacer-tmp/ which both sides can read (we exposed
    //    --filesystem=home in the manifest).
    QString tmpDir;
    if (qEnvironmentVariableIsSet("FLATPAK_ID") || QFileInfo::exists("/.flatpak-info")) {
        tmpDir = QDir::homePath() + "/.cache/gt-stacer-tmp";
        QDir().mkpath(tmpDir);
    } else {
        tmpDir = QDir::tempPath();
    }

    QTemporaryFile tmp(tmpDir + "/gt-stacer-XXXXXX");
    tmp.setAutoRemove(true);
    if (!tmp.open()) return false;
    if (tmp.write(content) != content.size()) return false;
    tmp.flush();
    QString tmpPath = tmp.fileName();
    tmp.close(); // close handle so `install` can read it

    // Use `pkexec install` — passes file contents byte-for-byte, sets owner/group/mode atomically.
    // No part of `content` is ever interpreted by a shell.
    QStringList args = {
        "install",
        "-o", owner,
        "-g", group,
        "-m", mode,
        tmpPath,
        destPath
    };
    return execProgram("pkexec", args) == 0;
}

bool CommandUtil::isSafeIdentifier(const QString &s)
{
    if (s.isEmpty() || s.size() > 256) return false;
    // Allow common package/service name chars only.
    static const QRegularExpression rx("^[A-Za-z0-9._@:+/-]+$");
    return rx.match(s).hasMatch();
}
