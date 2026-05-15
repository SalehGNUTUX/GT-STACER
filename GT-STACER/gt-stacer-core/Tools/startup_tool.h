#pragma once
#include <QString>
#include <QVector>

struct StartupEntry {
    QString name;
    QString exec;
    QString comment;
    QString icon;
    bool    enabled = true;
    QString filePath;
};

class StartupTool {
public:
    // Autostart entries currently active for the user (~/.config/autostart).
    static QVector<StartupEntry> entries();
    static bool enable(const QString &filePath);
    static bool disable(const QString &filePath);
    static bool remove(const QString &filePath);
    static bool add(const StartupEntry &entry);
    static QString autostartDir();

    // All .desktop applications installed on the system (system-wide + user-local).
    // Returns one StartupEntry per .desktop file — `filePath` points at the source.
    static QVector<StartupEntry> systemApplications();
};
