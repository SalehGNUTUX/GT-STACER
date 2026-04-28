#include "battery_info.h"
#include "../Utils/file_util.h"
#include <QDir>

QString BatteryData::statusString() const
{
    switch (status) {
        case BatteryStatus::Charging:    return "Charging";
        case BatteryStatus::Discharging: return "Discharging";
        case BatteryStatus::Full:        return "Full";
        case BatteryStatus::NotCharging: return "Not Charging";
        default:                         return "Unknown";
    }
}

static BatteryData readBattery(const QString &name)
{
    QString base = QString("/sys/class/power_supply/%1/").arg(name);
    BatteryData b;
    b.name    = name;
    b.present = FileUtil::readFile(base + "present").trimmed() == "1";

    QString statusStr = FileUtil::readFile(base + "status").trimmed();
    if      (statusStr == "Charging")    b.status = BatteryStatus::Charging;
    else if (statusStr == "Discharging") b.status = BatteryStatus::Discharging;
    else if (statusStr == "Full")        b.status = BatteryStatus::Full;
    else if (statusStr == "Not charging") b.status = BatteryStatus::NotCharging;

    // Capacity percent
    QString cap = FileUtil::readFile(base + "capacity").trimmed();
    if (!cap.isEmpty()) b.percent = cap.toInt();

    // Energy/charge values
    QString energyNow  = FileUtil::readFile(base + "energy_now");
    QString energyFull = FileUtil::readFile(base + "energy_full");
    QString powerNow   = FileUtil::readFile(base + "power_now");
    QString voltageNow = FileUtil::readFile(base + "voltage_now");

    if (!energyFull.isEmpty()) b.capacityWh = energyFull.toLongLong() / 1e6;
    if (!powerNow.isEmpty())   b.powerW     = powerNow.toLongLong() / 1e6;
    if (!voltageNow.isEmpty()) b.voltageV   = voltageNow.toLongLong() / 1e6;

    // Estimate time remaining
    if (b.powerW.has_value() && *b.powerW > 0.1 && b.capacityWh.has_value()) {
        double energyRemaining = energyNow.isEmpty() ? 0 : energyNow.toLongLong() / 1e6;
        if (b.status == BatteryStatus::Discharging && energyRemaining > 0)
            b.timeRemainingMin = static_cast<int>((energyRemaining / *b.powerW) * 60);
        else if (b.status == BatteryStatus::Charging && energyRemaining > 0 && b.capacityWh.has_value())
            b.timeRemainingMin = static_cast<int>(((*b.capacityWh - energyRemaining) / *b.powerW) * 60);
    }

    b.technology   = FileUtil::readFile(base + "technology").trimmed();
    b.manufacturer = FileUtil::readFile(base + "manufacturer").trimmed();
    b.model        = FileUtil::readFile(base + "model_name").trimmed();

    return b;
}

QVector<BatteryData> BatteryInfo::batteries()
{
    QVector<BatteryData> result;
    QDir ps("/sys/class/power_supply");
    if (!ps.exists()) return result;

    for (const auto &name : ps.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString type = FileUtil::readFile(
            QString("/sys/class/power_supply/%1/type").arg(name)).trimmed();
        if (type == "Battery")
            result << readBattery(name);
    }
    return result;
}

BatteryData BatteryInfo::primaryBattery()
{
    auto bats = batteries();
    return bats.isEmpty() ? BatteryData{} : bats.first();
}

bool BatteryInfo::hasBattery()
{
    return !batteries().isEmpty();
}

bool BatteryInfo::isOnAC()
{
    QDir ps("/sys/class/power_supply");
    if (!ps.exists()) return true;
    for (const auto &name : ps.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString type   = FileUtil::readFile(QString("/sys/class/power_supply/%1/type").arg(name)).trimmed();
        QString online = FileUtil::readFile(QString("/sys/class/power_supply/%1/online").arg(name)).trimmed();
        if (type == "Mains" && online == "1") return true;
    }
    return false;
}
