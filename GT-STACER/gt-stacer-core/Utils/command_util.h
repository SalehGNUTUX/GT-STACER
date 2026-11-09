#pragma once
#include <QString>
#include <QStringList>
#include "command_failure.h"

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
    // Like execProgram, but keeps everything needed to EXPLAIN a failure: exit
    // code, merged output (messages in English), timeout, pkexec involvement.
    // Classify with CommandFailure::classify(). timeoutMs < 0 = no time limit.
    static CommandResult runCommand(const QString &program, const QStringList &args, int timeoutMs = 10000);
    // The result of the last execProgram/runCommand on THIS thread — lets callers
    // of bool-returning helpers (pkexecWriteFile, tool setters …) explain why.
    static CommandResult lastResult();
    static void          clearLastResult();   // call before an operation you'll explain
    static QString    execProgramOutput(const QString &program, const QStringList &args, int timeoutMs = 10000);
    // Like execProgramOutput, but forces LC_ALL=C (parsable, untranslated output —
    // also on the host side of Flatpak), keeps stdout untrimmed, and reports the
    // exit code (-1 = failed to start / timed out) through *exitCode.
    static QString    execProgramC(const QString &program, const QStringList &args,
                                   int timeoutMs = 20000, int *exitCode = nullptr);

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

    // Rewrite (program, args) to run on the host via `flatpak-spawn --host` when
    // inside the Flatpak sandbox; a no-op otherwise. Use this when you must start
    // a long-lived process yourself (e.g. a QProcess held open for the app's
    // lifetime) that execProgram's blocking model can't manage.
    static void       wrapForHost(QString &program, QStringList &args);

    // True inside the Flatpak sandbox (FLATPAK_ID set or /.flatpak-info exists).
    // Needed when a sandbox PID is meaningless on the host (e.g. signalling a
    // process started through flatpak-spawn).
    static bool       inFlatpak();

    // Wrap a LONG-RUNNING process you start yourself with QProcess so it can be
    // stopped reliably — and never outlives GT-STACER (26.12):
    //   (program, args) → [pkexec] /bin/sh -c <guard> gt-stacer-guard program args…
    // then Flatpak-wrapped. The guard keeps the QProcess's stdin pipe and starts
    // the program in the background; when that pipe is closed — by
    // QProcess::closeWriteChannel() (a Stop button), or by the kernel because
    // GT-STACER quit, crashed or was killed (even SIGKILL) — the guard sends the
    // program SIGTERM. Because the guard itself runs with the program's
    // privileges (root under pkexec), this works where our own signal would be
    // refused (EPERM). The program's exit code is passed through. Arguments are
    // positional parameters, never interpolated into the script text.
    // The QProcess must be started in the default ReadWrite mode.
    static void       guardForStop(QString &program, QStringList &args, bool asRoot);
};
