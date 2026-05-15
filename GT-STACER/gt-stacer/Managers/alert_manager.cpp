#include "alert_manager.h"
#include "setting_manager.h"
#include "../../gt-stacer-core/Info/cpu_info.h"
#include "../../gt-stacer-core/Info/memory_info.h"
#include "../../gt-stacer-core/Info/disk_info.h"
#include "../../gt-stacer-core/Info/temperature_info.h"
#include "../../gt-stacer-core/Info/gpu_info.h"
#include "../../gt-stacer-core/Info/battery_info.h"
#include "../../gt-stacer-core/Tools/notification_tool.h"

AlertManager *AlertManager::s_instance = nullptr;

AlertManager *AlertManager::instance()
{
    if (!s_instance) s_instance = new AlertManager;
    return s_instance;
}

AlertManager::AlertManager(QObject *parent)
    : QObject(parent)
{
    auto *s = SettingManager::instance();
    m_enabled = s->alertsEnabled();

    m_timer = new QTimer(this);
    // Poll every 30s — alerts are coarse-grained on purpose. We don't want
    // to consult sensors more often than necessary.
    m_timer->setInterval(30 * 1000);
    connect(m_timer, &QTimer::timeout, this, &AlertManager::tick);
    if (m_enabled) m_timer->start();
}

void AlertManager::setEnabled(bool on)
{
    m_enabled = on;
    SettingManager::instance()->setAlertsEnabled(on);
    if (on) { if (!m_timer->isActive()) m_timer->start(); tick(); }
    else    m_timer->stop();
}

bool AlertManager::readyToFire(const QString &key)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 last = m_lastFiredMs.value(key, 0);
    if (now - last < kCooldownMs) return false;
    m_lastFiredMs[key] = now;
    return true;
}

void AlertManager::fire(const QString &key, const QString &title,
                         const QString &body, bool critical, const QString &icon)
{
    if (!readyToFire(key)) return;
    NotificationTool::notify(
        title, body,
        critical ? NotificationTool::Urgency::Critical
                 : NotificationTool::Urgency::Normal,
        icon.isEmpty() ? QStringLiteral("gt-stacer") : icon,
        critical ? 0 : 8000); // critical alerts stay until dismissed
}

void AlertManager::tick()
{
    if (!m_enabled || !NotificationTool::isAvailable()) return;
    auto *s = SettingManager::instance();

    // ── CPU temperature ─────────────────────────────────────────────────
    const int tempThreshold = s->cpuTempThresholdC();
    if (auto t = TemperatureInfo::cpuTemperature(); t.has_value() && tempThreshold > 0) {
        if (*t >= tempThreshold)
            fire("cpuTemp",
                 tr("CPU running hot"),
                 tr("Temperature reached %1°C (limit: %2°C).")
                     .arg(static_cast<int>(*t)).arg(tempThreshold),
                 *t >= tempThreshold + 10,
                 "dialog-warning");
    }

    // ── Memory ──────────────────────────────────────────────────────────
    auto mem = MemoryInfo::memory();
    const int memThreshold = s->memThresholdPercent();
    if (memThreshold > 0 && mem.ramPercent() >= memThreshold)
        fire("mem",
             tr("Memory pressure"),
             tr("RAM usage at %1% (limit: %2%).")
                 .arg(static_cast<int>(mem.ramPercent())).arg(memThreshold),
             mem.ramPercent() >= 95,
             "dialog-warning");

    // ── Disk usage ──────────────────────────────────────────────────────
    const int diskThreshold = s->diskThresholdPercent();
    if (diskThreshold > 0) {
        for (const auto &p : DiskInfo::partitions()) {
            if (p.total == 0) continue;
            int pct = static_cast<int>((p.used * 100) / p.total);
            if (pct >= diskThreshold)
                fire("disk:" + p.mountPoint,
                     tr("Disk almost full"),
                     tr("%1 is %2% full (limit: %3%).")
                         .arg(p.mountPoint).arg(pct).arg(diskThreshold),
                     pct >= 95,
                     "drive-harddisk");
        }
    }

    // ── Battery ─────────────────────────────────────────────────────────
    const int battThreshold = s->batteryThresholdPercent();
    auto bat = BatteryInfo::primaryBattery();
    if (bat.present && bat.status == BatteryStatus::Discharging
        && battThreshold > 0 && bat.percent <= battThreshold)
        fire("battery",
             tr("Battery low"),
             tr("Battery at %1% — consider plugging in.").arg(bat.percent),
             bat.percent <= 10,
             "battery-low");

    // ── GPU temperature (where reported) ────────────────────────────────
    if (tempThreshold > 0) {
        for (const auto &g : GpuInfo::gpus()) {
            if (g.temperatureCelsius.has_value()
                && *g.temperatureCelsius >= tempThreshold)
                fire("gpuTemp:" + g.name,
                     tr("GPU running hot"),
                     tr("%1 reached %2°C.")
                         .arg(g.name).arg(static_cast<int>(*g.temperatureCelsius)),
                     *g.temperatureCelsius >= tempThreshold + 10,
                     "dialog-warning");
        }
    }
}
