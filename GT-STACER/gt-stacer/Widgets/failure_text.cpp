#include "failure_text.h"
#include "bidi_text.h"
#include "../../gt-stacer-core/Utils/command_util.h"
#include <QMessageBox>

using Kind = CommandFailure::Kind;

QString FailureText::title(const CommandFailure::Failure &f)
{
    switch (f.kind) {
    case Kind::None:               return {};
    case Kind::AuthCancelled:      return tr("You cancelled the password prompt, so nothing was changed.");
    case Kind::NotAuthorized:      return tr("Authorization failed — the password was wrong or this account isn't allowed to do this.");
    case Kind::NoAuthAgent:        return tr("No password prompt could be shown (no authentication agent is running).");
    case Kind::NotInstalled:       return f.subject.isEmpty()
                                          ? tr("A required program is not installed.")
                                          : tr("The program “%1” is not installed.").arg(f.subject);
    case Kind::Busy:               return tr("Another program is using the package manager right now (an update, a software store or another install).");
    case Kind::DpkgInterrupted:    return tr("A previous package installation was interrupted and must be finished first.");
    case Kind::NoNetwork:          return tr("Could not download — there is no working Internet connection or the server is unreachable.");
    case Kind::NoSpace:            return tr("There is not enough free disk space.");
    case Kind::ReadOnly:           return tr("The target disk is mounted read-only.");
    case Kind::PackageNotFound:    return tr("The package was not found in your repositories.");
    case Kind::BrokenDependencies: return tr("The package's dependencies can't be satisfied (missing or conflicting packages).");
    case Kind::PermissionDenied:   return tr("Permission denied — this needs administrator rights or access to a protected file.");
    case Kind::Timeout:            return tr("The operation took too long and was stopped.");
    case Kind::DeviceBusy:         return tr("The disk is in use — a program still has files or a folder open on it.");
    case Kind::BadFilesystem:      return tr("The filesystem is damaged or of a type this system can't read.");
    case Kind::Unknown:            return tr("The operation failed.");
    }
    return {};
}

QString FailureText::advice(const CommandFailure::Failure &f)
{
    switch (f.kind) {
    case Kind::AuthCancelled:      return tr("Run it again and enter your password when asked.");
    case Kind::NotAuthorized:      return tr("Check the password, and that your account is an administrator (member of the sudo or wheel group).");
    case Kind::NoAuthAgent:        return tr("Log in to a full desktop session (KDE, GNOME, Xfce …), or install and start a polkit agent such as polkit-kde-agent or lxpolkit.");
    case Kind::NotInstalled:       return f.subject.isEmpty() ? QString()
                                          : tr("Install the package that provides “%1” with your package manager, then try again.").arg(f.subject);
    case Kind::Busy:               return tr("Wait until it finishes (or close the other program), then try again.");
    case Kind::DpkgInterrupted:    return tr("Open a terminal and run: sudo dpkg --configure -a — then try again.");
    case Kind::NoNetwork:          return tr("Check your connection (and proxy or mirror settings), then try again.");
    case Kind::NoSpace:            return tr("Free some space (e.g. with the System Cleaner) or choose another disk, then try again.");
    case Kind::ReadOnly:           return tr("Remount the disk read-write, or check it for errors — Linux mounts a damaged disk read-only.");
    case Kind::PackageNotFound:    return tr("Refresh the package lists (Upgrades → Check for upgrades), check the name, or enable the repository that provides it.");
    case Kind::BrokenDependencies: return tr("Update the system first; if it persists, the package may be meant for another distribution version.");
    case Kind::PermissionDenied:   return tr("Make sure you are allowed to change that file or folder.");
    case Kind::Timeout:            return tr("Try again; if it keeps happening, run it from a terminal to see where it stops.");
    case Kind::DeviceBusy:         return tr("Close the files, file-manager windows and terminals that are inside it, then try again.");
    case Kind::BadFilesystem:      return tr("Check the disk or image for errors, or install the tools for its filesystem (e.g. ntfs-3g, exfatprogs).");
    case Kind::None:
    case Kind::Unknown:            return {};
    }
    return {};
}

QString FailureText::message(const QString &what, const CommandResult &r)
{
    const auto f = CommandFailure::classify(r);
    if (f.kind == Kind::None) return {};
    QString m = what.isEmpty() ? title(f) : what + "\n" + title(f);
    const QString a = advice(f);
    if (!a.isEmpty()) m += "\n" + a;
    // The tool's own words add nothing for a cancelled prompt.
    if (!f.detail.isEmpty() && f.kind != Kind::AuthCancelled)
        m += "\n\n" + tr("Details: %1").arg(f.detail);
    return Bidi::isolate(m);
}

QString FailureText::brief(const CommandResult &r)
{
    return Bidi::isolate(title(CommandFailure::classify(r)));
}

QString FailureText::lastReason()
{
    const CommandResult r = CommandUtil::lastResult();
    if (r.program.isEmpty()) return {};          // nothing was run / recorded
    return brief(r);
}

void FailureText::show(QWidget *parent, const QString &caption, const QString &what,
                       const CommandResult &r)
{
    const auto f = CommandFailure::classify(r);
    if (f.kind == Kind::None) return;
    QMessageBox box(f.kind == Kind::AuthCancelled ? QMessageBox::Information : QMessageBox::Warning,
                    caption, Bidi::isolate(what.isEmpty() ? title(f) : what + "\n\n" + title(f)),
                    QMessageBox::Ok, parent);
    const QString a = advice(f);
    if (!a.isEmpty()) box.setInformativeText(Bidi::isolate(a));
    if (!r.output.trimmed().isEmpty() && f.kind != Kind::AuthCancelled)
        box.setDetailedText(r.output.trimmed().right(20000));
    box.exec();
}
