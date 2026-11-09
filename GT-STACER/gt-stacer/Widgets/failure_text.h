#pragma once
#include <QCoreApplication>
#include <QString>
#include "../../gt-stacer-core/Utils/command_failure.h"

class QWidget;

// The ONE place that turns a failed command into words for the user (26.12):
// what happened, in plain language, and what to do about it — per case
// (cancelled password prompt, another package manager running, no network,
// disk full, missing tool …), translated. Pages call this instead of writing
// their own vague "authorization declined?" guesses.
class FailureText {
    Q_DECLARE_TR_FUNCTIONS(FailureText)
public:
    static QString title(const CommandFailure::Failure &f);    // one line: what happened
    static QString advice(const CommandFailure::Failure &f);   // what to do (may be empty)

    // "<what>: <title>\n<advice>" (+ the tool's own line, if any).
    static QString message(const QString &what, const CommandResult &r);
    // Just the short title — for status labels.
    static QString brief(const CommandResult &r);
    // brief() of CommandUtil::lastResult() — for tools that only return bool.
    // Empty if no command ran since CommandUtil::clearLastResult(). Thread-safe
    // (call it on the thread that ran the tool).
    static QString lastReason();

    // A warning box: `what` + explanation + advice; the full command output
    // under "Show Details". Does nothing for a successful result.
    static void show(QWidget *parent, const QString &caption, const QString &what,
                     const CommandResult &r);
};
