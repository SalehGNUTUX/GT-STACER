#pragma once
#include <QString>

class QProcess;

// Cross-desktop "keep awake" for automatic sleep + screen locking.
//
// Prefers the freedesktop D-Bus interfaces that KDE/GNOME/XFCE/… all implement:
//   • org.freedesktop.PowerManagement.Inhibit  (blocks automatic sleep)
//   • org.freedesktop.ScreenSaver              (blocks the screen lock/saver)
// Using these means OUR block shows up in the desktop's own power UI, and — via
// PowerManagement's HasInhibit() — we can also DETECT a block set from elsewhere
// (e.g. KDE's own "Manually block", which never appears in systemd-inhibit).
//
// Falls back to `systemd-inhibit` (logind) on desktops without those services.
class SleepInhibitor {
public:
    ~SleepInhibitor();

    void block();                 // start holding an inhibit (ours)
    void unblock();               // release ours
    bool blockedByUs() const;

    // Available at all if we can inhibit by either mechanism.
    static bool available();
    // Is sleep/lock inhibited right now by ANYONE (our block, KDE's manual block,
    // a video player…)? Reflects the true system state for the UI.
    static bool systemInhibited();

private:
    bool dbusBlock();             // returns true if the D-Bus path succeeded
    void dbusUnblock();

    unsigned int m_pmCookie = 0;  // PowerManagement inhibit cookie
    unsigned int m_ssCookie = 0;  // ScreenSaver inhibit cookie
    bool         m_dbusHeld = false;
    QProcess    *m_proc     = nullptr;   // systemd-inhibit fallback
};
