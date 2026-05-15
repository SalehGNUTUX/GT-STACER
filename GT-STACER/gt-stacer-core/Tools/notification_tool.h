#pragma once
#include <QString>

// Thin wrapper around `notify-send`. We deliberately shell-out rather than
// link libnotify to keep the dependency surface small (libnotify pulls in
// glib + gdk-pixbuf). `notify-send` is part of `libnotify-bin` (Debian) /
// `libnotify` (Fedora) and is installed on virtually every Linux desktop.
class NotificationTool {
public:
    enum class Urgency { Low, Normal, Critical };

    // Returns true if notify-send is installed.
    static bool isAvailable();

    // Display a desktop notification. Title and body are passed as separate
    // argv elements via QProcess, so they cannot inject shell commands.
    // `category` and `iconName` are optional. Returns true on success.
    static bool notify(const QString &title,
                       const QString &body,
                       Urgency urgency      = Urgency::Normal,
                       const QString &iconName = QString(),
                       int timeoutMs        = 5000);
};
