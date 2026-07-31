#pragma once
#include <QString>
#include <QStringList>

// Live power management. Prefers power-profiles-daemon (the profile system used
// by GNOME/KDE — works on desktops too, it drives the CPU governor); falls back
// to raw cpufreq governors when the daemon is absent. All state changes route
// through CommandUtil so the Flatpak host wrapper applies.
class PowerProfileTool {
public:
    // ── power-profiles-daemon (powerprofilesctl) ────────────────────────
    static bool        ppdAvailable();
    static QStringList profiles();        // e.g. {power-saver, balanced, performance}
    static QString     activeProfile();   // "" if unavailable
    static bool        setProfile(const QString &id);   // polkit; usually no password

    // ── cpufreq governor fallback ───────────────────────────────────────
    static bool        cpufreqAvailable();
    static QStringList governors();       // scaling_available_governors
    static QString     activeGovernor();  // cpu0's scaling_governor
    static bool        setGovernor(const QString &gov);  // pkexec, all CPUs

    // ── TLP presence (informational) ────────────────────────────────────
    static bool        tlpAvailable();

    // ── Sleep/idle inhibitors (system-wide harmony) ─────────────────────
    static bool        inhibitAvailable();   // systemd-inhibit present
    // WHO of every mode=block inhibitor currently preventing sleep or idle —
    // from ANY app (GT-STACER, KDE, a video player…), so the UI reflects the
    // real system state rather than only its own action.
    static QStringList sleepBlockers();
};
