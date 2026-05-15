#pragma once
#include <QString>
#include <QStringList>

class CommandUtil {
public:
    static QString exec(const QString &command);
    static QStringList execLines(const QString &command);
    static int execStatus(const QString &command);
    static bool commandExists(const QString &command);
    static QString execSudo(const QString &command);

    // Safe variants: program + args are passed to QProcess directly (no shell),
    // so user-controlled values cannot inject shell metacharacters.
    static int        execProgram(const QString &program, const QStringList &args, int timeoutMs = 10000);
    static QString    execProgramOutput(const QString &program, const QStringList &args, int timeoutMs = 10000);

    // Write `content` atomically to `destPath` as root via pkexec.
    // Uses a temp file + `pkexec install` so no part of `content` is interpreted by a shell.
    // Returns true on success.
    static bool       pkexecWriteFile(const QString &destPath, const QByteArray &content,
                                       const QString &owner = "root",
                                       const QString &group = "root",
                                       const QString &mode  = "0644");

    // Validate a shell-safe identifier (package, service, source name).
    // Rejects any string containing characters outside [A-Za-z0-9._@:+-/].
    static bool       isSafeIdentifier(const QString &s);
};
