#include "command_util.h"
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QDir>

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
    return !QStandardPaths::findExecutable(command).isEmpty();
}

QString CommandUtil::execSudo(const QString &command)
{
    return exec(QString("pkexec %1").arg(command));
}

int CommandUtil::execProgram(const QString &program, const QStringList &args, int timeoutMs)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(timeoutMs)) return -1;
    if (!p.waitForFinished(timeoutMs)) { p.kill(); return -1; }
    return p.exitCode();
}

QString CommandUtil::execProgramOutput(const QString &program, const QStringList &args, int timeoutMs)
{
    QProcess p;
    p.start(program, args);
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

    // Write content to a temp file under the user's runtime/temp dir.
    QTemporaryFile tmp(QDir::tempPath() + "/gt-stacer-XXXXXX");
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
