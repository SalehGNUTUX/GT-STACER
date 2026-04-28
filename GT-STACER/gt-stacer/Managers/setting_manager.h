#pragma once
#include <QObject>
#include <QString>

class SettingManager : public QObject {
    Q_OBJECT
public:
    static SettingManager *instance();

    // Theme
    QString theme() const;
    void    setTheme(const QString &theme);

    // Language
    QString language() const;
    void    setLanguage(const QString &lang);

    // Window
    bool startMinimized() const;
    void setStartMinimized(bool val);

    // Update interval
    int  updateIntervalMs() const;
    void setUpdateIntervalMs(int ms);

    // Tray
    bool showTrayIcon() const;
    void setShowTrayIcon(bool val);

    bool minimizeToTray() const;
    void setMinimizeToTray(bool val);

private:
    explicit SettingManager(QObject *parent = nullptr);
    static SettingManager *m_instance;
};
