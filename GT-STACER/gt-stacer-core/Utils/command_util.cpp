#include "command_util.h"
#include <QProcess>
#include <QStandardPaths>

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
