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

    // Language — the stored value may be "auto" (follow the system locale) or a
    // concrete UI code (e.g. "ar", "fr"). Default on first run is "auto".
    QString language() const;                 // raw stored value ("auto" or code)
    QString effectiveLanguage() const;        // "auto" resolved to a real code
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

    // ── System Relief ───────────────────────────────────────────────────
    bool reliefAutoMode() const;            // watch thresholds and auto-freeze
    void setReliefAutoMode(bool v);

    int  reliefCpuThreshold() const;        // %; 0 = ignore CPU
    void setReliefCpuThreshold(int v);

    int  reliefRamThreshold() const;        // %; 0 = ignore RAM
    void setReliefRamThreshold(int v);

    int  reliefHoldSeconds() const;         // sustained seconds before acting
    void setReliefHoldSeconds(int v);

    bool reliefDropCaches() const;          // also drop file caches on freeze
    void setReliefDropCaches(bool v);

    bool reliefAutoRefresh() const;         // periodically refresh the list
    void setReliefAutoRefresh(bool v);

    int  reliefRefreshSeconds() const;      // auto-refresh cadence
    void setReliefRefreshSeconds(int v);

    // ── Network connections page ────────────────────────────────────────
    bool connAutoRefresh() const;
    void setConnAutoRefresh(bool v);

    int  connRefreshSeconds() const;
    void setConnRefreshSeconds(int v);

private:
    explicit SettingManager(QObject *parent = nullptr);
    static SettingManager *m_instance;
};
