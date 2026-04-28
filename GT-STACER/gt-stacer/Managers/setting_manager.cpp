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
