#include "setting_manager.h"
#include <QLocale>
#include <QSettings>

SettingManager *SettingManager::m_instance = nullptr;

namespace {
// The UI languages GT-STACER ships a translation for (must match the combo in
// settings_page.cpp). Used to map the system locale onto a supported code.
const QStringList &supportedLanguages()
{
    static const QStringList codes = {
        "en", "ar", "de", "fr", "hi", "it", "kn", "ml", "nl", "oc",
        "pl", "pt", "ru", "sv", "tr", "uk", "zh_CN", "zh_TW", "vi"
    };
    return codes;
}

// Best-effort map from the system locale to a shipped UI language. Tries the
// full "lang_REGION" code first (so zh_CN vs zh_TW is respected), then the bare
// language part, and finally falls back to English.
QString detectSystemLanguage()
{
    const QString sys = QLocale::system().name();          // e.g. "fr_FR", "zh_TW"
    if (supportedLanguages().contains(sys)) return sys;

    const QString lang = sys.section('_', 0, 0);           // "fr", "zh", "ar"
    if (lang == "zh") {
        const QString region = sys.section('_', 1, 1).toUpper();
        return (region == "TW" || region == "HK" || region == "MO") ? "zh_TW" : "zh_CN";
    }
    if (supportedLanguages().contains(lang)) return lang;
    return "en";
}
} // namespace

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

// Default "auto" follows the system locale; a concrete code (once the user
// picks one) is remembered verbatim.
QString SettingManager::language()             const { return cfg().value("language", "auto").toString(); }
void    SettingManager::setLanguage(const QString &v) { cfg().setValue("language", v); }

// "auto" → the shipped language matching the system locale, or English when the
// locale has no translation. A concrete stored code is returned as-is.
QString SettingManager::effectiveLanguage()    const { const QString v = language();
                                                       return v == "auto" ? detectSystemLanguage() : v; }

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

int  SettingManager::diskThresholdPercent()  const { return cfg().value("alerts/diskPercent", 95).toInt(); }
void SettingManager::setDiskThresholdPercent(int v){ cfg().setValue("alerts/diskPercent", v); }

int  SettingManager::batteryThresholdPercent() const { return cfg().value("alerts/batteryPercent", 15).toInt(); }
void SettingManager::setBatteryThresholdPercent(int v){ cfg().setValue("alerts/batteryPercent", v); }

// ── System Relief ───────────────────────────────────────────────────────
bool SettingManager::reliefAutoMode()        const { return cfg().value("relief/auto", false).toBool(); }
void SettingManager::setReliefAutoMode(bool v)     { cfg().setValue("relief/auto", v); }

int  SettingManager::reliefCpuThreshold()    const { return cfg().value("relief/cpuThresh", 90).toInt(); }
void SettingManager::setReliefCpuThreshold(int v)  { cfg().setValue("relief/cpuThresh", v); }

int  SettingManager::reliefRamThreshold()    const { return cfg().value("relief/ramThresh", 90).toInt(); }
void SettingManager::setReliefRamThreshold(int v)  { cfg().setValue("relief/ramThresh", v); }

int  SettingManager::reliefHoldSeconds()     const { return cfg().value("relief/holdSecs", 8).toInt(); }
void SettingManager::setReliefHoldSeconds(int v)   { cfg().setValue("relief/holdSecs", v); }

bool SettingManager::reliefDropCaches()      const { return cfg().value("relief/dropCaches", false).toBool(); }
void SettingManager::setReliefDropCaches(bool v)   { cfg().setValue("relief/dropCaches", v); }

bool SettingManager::reliefAutoRefresh()     const { return cfg().value("relief/autoRefresh", false).toBool(); }
void SettingManager::setReliefAutoRefresh(bool v)  { cfg().setValue("relief/autoRefresh", v); }

int  SettingManager::reliefRefreshSeconds()  const { return cfg().value("relief/refreshSecs", 10).toInt(); }
void SettingManager::setReliefRefreshSeconds(int v){ cfg().setValue("relief/refreshSecs", v); }

// ── Network connections page ────────────────────────────────────────────
bool SettingManager::connAutoRefresh()       const { return cfg().value("connections/autoRefresh", false).toBool(); }
void SettingManager::setConnAutoRefresh(bool v)    { cfg().setValue("connections/autoRefresh", v); }

int  SettingManager::connRefreshSeconds()    const { return cfg().value("connections/refreshSecs", 10).toInt(); }
void SettingManager::setConnRefreshSeconds(int v)  { cfg().setValue("connections/refreshSecs", v); }
