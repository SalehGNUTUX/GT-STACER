#pragma once
#include <QString>

// Scheduled / immediate system power actions. All actions are performed through
// systemctl (logind), which honours polkit — for an active local session the
// desktop authorises poweroff/reboot/suspend without a password on most
// distros; where a policy demands it, the host's polkit agent prompts. Every
// call routes through CommandUtil::execProgram so it also works inside Flatpak
// (flatpak-spawn --host).
class PowerTool {
public:
    enum Action {
        Shutdown,   // systemctl poweroff
        Reboot,     // systemctl reboot
        Suspend,    // systemctl suspend    — suspend to RAM (ACPI S3), instant resume
        Hibernate   // systemctl hibernate  — suspend to disk (ACPI S4), zero power, needs swap
    };

    // Execute the action now. Returns true if systemctl accepted the request.
    static bool    perform(Action a);

    // Human-readable (untranslated) verb, for logging/debug.
    static QString actionVerb(Action a);

    // Whether the kernel advertises support: /sys/power/state must list "mem"
    // for Suspend and "disk" for Hibernate. Shutdown/Reboot are always true.
    static bool    isAvailable(Action a);
};
