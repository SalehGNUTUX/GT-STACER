#pragma once
#include <QString>

// Why did a command fail? (26.12)
//
// 26.11 mostly reduced a failed system command to true/false, so the UI could
// only guess ("authorization declined?") even when the output said exactly
// what went wrong — another package manager holding the lock, no network, a
// full disk, a cancelled password prompt… CommandResult keeps the exit code
// and output; CommandFailure::classify() turns them into a Kind the UI maps to
// a translated, case-specific explanation and fix (see gt-stacer/Widgets/
// failure_text.h). The core stays text-free: `detail` is the tool's own most
// relevant line, shown as-is under the explanation.

struct CommandResult {
    QString program;          // the real program (after pkexec), e.g. "apt-get"
    bool    viaPkexec = false;
    bool    started   = false;
    bool    timedOut  = false;
    int     exitCode  = -1;
    QString output;           // stdout+stderr, messages in English
    bool ok() const { return started && !timedOut && exitCode == 0; }
};

class CommandFailure {
public:
    enum class Kind {
        None,
        AuthCancelled,        // the password dialog was closed/cancelled (pkexec 126)
        NotAuthorized,        // wrong password / policy refused (pkexec 127)
        NoAuthAgent,          // no polkit agent running to ask for the password
        NotInstalled,         // the program (or a helper) isn't installed
        Busy,                 // another package manager / instance holds the lock
        DpkgInterrupted,      // a previous dpkg run was interrupted
        NoNetwork,            // download / name resolution failed
        NoSpace,              // disk full
        ReadOnly,             // read-only filesystem
        PackageNotFound,      // no such package in the repositories
        BrokenDependencies,   // unmet / conflicting dependencies
        PermissionDenied,     // EACCES / EPERM on a file
        Timeout,              // took too long and was stopped
        DeviceBusy,           // a disk/mount is in use (open files, a shell inside it)
        BadFilesystem,        // damaged or unsupported filesystem / image
        Unknown
    };

    struct Failure {
        Kind    kind = Kind::None;
        QString subject;      // e.g. the missing program's name
        QString detail;       // the tool's most relevant output line
    };

    static Failure classify(const CommandResult &r);
};
