#pragma once
#include <QObject>
#include <QString>

// Which process owns the window the user is working in? (26.12)
//
// System Relief must never freeze the app in front of the user, and should
// thaw a frozen app the moment the user switches to it. Wayland deliberately
// gives ordinary programs no way to ask for the active window, so:
//
//  • KDE Plasma (Wayland AND X11): a tiny KWin script reports every window
//    activation over D-Bus — straight to our own connection's unique name,
//    so no bus name has to be owned. Event-driven, costs nothing while idle.
//  • Other X11 desktops: `xprop` (_NET_ACTIVE_WINDOW → _NET_WM_PID), asked
//    on demand.
//  • Elsewhere (e.g. GNOME on Wayland): unknown — Relief then relies on its
//    activity rules (CPU, disk, audio, sleep inhibitors) alone.
class FocusTracker : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gnutux.gtstacer.FocusTracker")
public:
    enum class Backend { None, KWin, Xprop };

    static FocusTracker *instance();

    // Start/stop tracking (reference-counted, so several users can share it).
    void acquire();
    void release();

    Backend backend() const { return m_backend; }
    // PID owning the active window, 0 if unknown.
    int focusedPid();

signals:
    void focusChanged(int pid);

public slots:
    // Called by the KWin script over D-Bus.
    Q_SCRIPTABLE void activated(int pid, const QString &windowClass);

private:
    explicit FocusTracker(QObject *parent = nullptr);
    ~FocusTracker() override;
    bool startKWin();
    void stopKWin();

    Backend m_backend = Backend::None;
    int     m_refs    = 0;
    int     m_pid     = 0;
    bool    m_registered = false;
};
