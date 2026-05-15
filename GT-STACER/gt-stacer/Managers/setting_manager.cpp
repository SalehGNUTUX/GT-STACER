#include "setting_manager.h"
#include <QSettings>

SettingManager *SettingManager::m_instance = nullptr;

SettingManager *SettingManager::instance()
{
    if (!m_instance) m_instance = new SettingManager;
    return m_instance;
}

SettingManager::SettingManager(QObject *parent) : QObject(parent) {}

static QSettings &cfg()
{
    static QSettings s("GNUTUX", "GT-STACER");
    return s;
}

QString SettingManager::theme()             const { return cfg().value("theme", "dark").toString(); }
void    SettingManager::setTheme(const QString &v) { cfg().setValue("theme", v); }

QString SettingManager::language()             const { return cfg().value("language", "en").toString(); }
void    SettingManager::setLanguage(const QString &v) { cfg().setValue("language", v); }

bool    SettingManager::startMinimized()    const { return cfg().value("startMinimized", false).toBool(); }
void    SettingManager::setStartMinimized(bool v)  { cfg().setValue("startMinimized", v); }

int     SettingManager::updateIntervalMs() const { return cfg().value("updateInterval", 2000).toInt(); }
void    SettingManager::setUpdateIntervalMs(int v) { cfg().setValue("updateInterval", v); }

bool    SettingManager::showTrayIcon()     const { return cfg().value("showTrayIcon", true).toBool(); }
void    SettingManager::setShowTrayIcon(bool v)    { cfg().setValue("showTrayIcon", v); }

bool    SettingManager::minimizeToTray()   const { return cfg().value("minimizeToTray", true).toBool(); }
void    SettingManager::setMinimizeToTray(bool v)  { cfg().setValue("minimizeToTray", v); }

// ── Alerts ──────────────────────────────────────────────────────────────
bool SettingManager::alertsEnabled()         const { return cfg().value("alerts/enabled", true).toBool(); }
void SettingManager::setAlertsEnabled(bool v)      { cfg().setValue("alerts/enabled", v); }

int  SettingManager::cpuTempThresholdC()     const { return cfg().value("alerts/cpuTempC", 85).toInt(); }
void SettingManager::setCpuTempThresholdC(int v)   { cfg().setValue("alerts/cpuTempC", v); }

int  SettingManager::memThresholdPercent()   const { return cfg().value("alerts/memPercent", 90).toInt(); }
void SettingManager::setMemThresholdPercent(int v) { cfg().setValue("alerts/memPercent", v); }

int  SettingManager::diskThresholdPercent()  const { return cfg().value("alerts/diskPercent", 90).toInt(); }
void SettingManager::setDiskThresholdPercent(int v){ cfg().setValue("alerts/diskPercent", v); }

int  SettingManager::batteryThresholdPercent() const { return cfg().value("alerts/batteryPercent", 15).toInt(); }
void SettingManager::setBatteryThresholdPercent(int v){ cfg().setValue("alerts/batteryPercent", v); }
