#pragma once
#include <QObject>
#include <QString>

class SettingManager : public QObject {
    Q_OBJECT
public:
    static SettingManager *instance();

    // Theme — can be "dark", "light", or "auto" (follow system).
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

    // ── Alerts ──────────────────────────────────────────────────────────
    bool alertsEnabled() const;
    void setAlertsEnabled(bool val);

    int  cpuTempThresholdC() const;     // °C; 0 = disabled
    void setCpuTempThresholdC(int v);

    int  memThresholdPercent() const;   // %; 0 = disabled
    void setMemThresholdPercent(int v);

    int  diskThresholdPercent() const;
    void setDiskThresholdPercent(int v);

    int  batteryThresholdPercent() const;
    void setBatteryThresholdPercent(int v);

private:
    explicit SettingManager(QObject *parent = nullptr);
    static SettingManager *m_instance;
};
