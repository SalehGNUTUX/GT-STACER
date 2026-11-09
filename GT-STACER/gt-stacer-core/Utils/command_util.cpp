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
void wrapForHostImpl(QString &program, QStringList &args)
{
    if (!runningInFlatpak()) return;
    args.prepend(program);
    args.prepend("--host");
    program = "flatpak-spawn";
}
} // namespace

void CommandUtil::wrapForHost(QString &program, QStringList &args)
{
    wrapForHostImpl(program, args);
}

bool CommandUtil::inFlatpak()
{
    return runningInFlatpak();
}

void CommandUtil::guardForStop(QString &program, QStringList &args, bool asRoot)
{
    // fd 3 = our pipe; the program gets /dev/null as stdin. `read` returns on
    // EOF *or* a line, then the guard stops the program; when the program
    // ends by itself, the guard drops its watcher and exits with its code.
    static const QString kGuard = QStringLiteral(
        "exec 3<&0 0</dev/null; "
        "\"$@\" & p=$!; "
        "{ read _ <&3; kill -TERM \"$p\" 2>/dev/null; } & w=$!; "
        "wait \"$p\"; rc=$?; kill \"$w\" 2>/dev/null; exit $rc");
    QStringList a{"/bin/sh", "-c", kGuard, "gt-stacer-guard", program};
    a += args;
    if (asRoot) { program = "pkexec"; args = a; }
    else        { program = a.takeFirst(); args = a; }
    wrapForHostImpl(program, args);
}

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

namespace {
thread_local CommandResult t_last;

// Run with English MESSAGES (so failures can be classified on any UI
// language) — only the message language changes, not the charset, so file
// names and parsers are unaffected.
void englishMessages(QProcess &p, QString &prog, QStringList &args)
{
    if (runningInFlatpak()) {
        // flatpak-spawn doesn't forward our environment to the host process.
        args.prepend(prog);
        args.prepend("--env=LANGUAGE=C");
        args.prepend("--env=LC_MESSAGES=C");
        args.prepend("--host");
        prog = "flatpak-spawn";
        return;
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_MESSAGES", "C");
    env.insert("LANGUAGE", "C");
    if (!env.value("LC_ALL").isEmpty()) env.insert("LC_ALL", "C.UTF-8");   // LC_ALL would override
    p.setProcessEnvironment(env);
}
} // namespace

CommandResult CommandUtil::runCommand(const QString &program, const QStringList &args, int timeoutMs)
{
    CommandResult r;
    r.viaPkexec = (program == "pkexec");
    r.program   = (r.viaPkexec && !args.isEmpty()) ? args.first() : program;
    QString prog = program;
    QStringList a = args;
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    englishMessages(p, prog, a);
    p.start(prog, a);
    if (!p.waitForStarted(timeoutMs < 0 ? 30000 : timeoutMs)) { t_last = r; return r; }
    r.started = true;
    if (!p.waitForFinished(timeoutMs)) {
        r.timedOut = true;
        p.kill();
        p.waitForFinished(2000);
    }
    r.output = QString::fromUtf8(p.readAll());
    r.exitCode = (p.exitStatus() == QProcess::NormalExit) ? p.exitCode() : -1;
    t_last = r;
    return r;
}

CommandResult CommandUtil::lastResult() { return t_last; }
void CommandUtil::clearLastResult() { t_last = CommandResult{}; }

int CommandUtil::execProgram(const QString &program, const QStringList &args, int timeoutMs)
{
    const CommandResult r = runCommand(program, args, timeoutMs);
    if (!r.started || r.timedOut) return -1;
    return r.exitCode;
}

QString CommandUtil::execProgramOutput(const QString &program, const QStringList &args, int timeoutMs)
{
    QString prog = program;
    QStringList a = args;
    wrapForHostImpl(prog, a);
    QProcess p;
    p.start(prog, a);
    if (!p.waitForStarted(timeoutMs)) return {};
    if (!p.waitForFinished(timeoutMs)) { p.kill(); return {}; }
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

QString CommandUtil::execProgramC(const QString &program, const QStringList &args,
                                  int timeoutMs, int *exitCode)
{
    QString prog = program;
    QStringList a = args;
    QProcess p;
    if (runningInFlatpak()) {
        // flatpak-spawn does not forward our environment to the host process.
        a.prepend(prog);
        a.prepend("--env=LC_ALL=C");
        a.prepend("--host");
        prog = "flatpak-spawn";
    } else {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LC_ALL", "C");
        p.setProcessEnvironment(env);
    }
    if (exitCode) *exitCode = -1;
    p.start(prog, a);
    if (!p.waitForStarted(timeoutMs)) return {};
    if (!p.waitForFinished(timeoutMs)) { p.kill(); p.waitForFinished(1000); return {}; }
    if (exitCode) *exitCode = (p.exitStatus() == QProcess::NormalExit) ? p.exitCode() : -1;
    return QString::fromUtf8(p.readAllStandardOutput());
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
    // Allow common package/service name chars only — and never a leading '-':
    // these values are passed as argv to apt/dnf/pacman/systemctl (often as
    // root), and a name like "-oAPT::…" or "--config=…" would be parsed as an
    // OPTION, not a name (argument injection). No real package, service or
    // source name starts with '-'.
    static const QRegularExpression rx("^[A-Za-z0-9._@:+/][A-Za-z0-9._@:+/-]*$");
    return rx.match(s).hasMatch();
}
