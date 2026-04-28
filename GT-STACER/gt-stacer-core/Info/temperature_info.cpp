#include "temperature_info.h"
#include "../Utils/file_util.h"
#include <QDir>

QVector<ThermalSensor> TemperatureInfo::hwmonSensors()
{
    QVector<ThermalSensor> result;
    QDir hwmon("/sys/class/hwmon");
    if (!hwmon.exists()) return result;

    for (const auto &hw : hwmon.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString base = QString("/sys/class/hwmon/%1/").arg(hw);
        QString name = FileUtil::readFile(base + "name").trimmed();

        // Find all temp*_input files
        QDir dir(base);
        for (const auto &f : dir.entryList({"temp*_input"}, QDir::Files)) {
            QString input = FileUtil::readFile(base + f).trimmed();
            if (input.isEmpty()) continue;

            ThermalSensor s;
            s.hwmonPath = base + f;
            QString idx  = f.mid(4, f.indexOf('_') - 4);  // "temp1_input" -> "1"

            s.name     = name;
            s.label    = FileUtil::readFile(base + QString("temp%1_label").arg(idx)).trimmed();
            if (s.label.isEmpty()) s.label = name + " #" + idx;
            s.current  = input.toLongLong() / 1000.0;

            QString crit = FileUtil::readFile(base + QString("temp%1_crit").arg(idx));
            QString high = FileUtil::readFile(base + QString("temp%1_max").arg(idx));
            if (!crit.isEmpty()) s.critical = crit.toLongLong() / 1000.0;
            if (!high.isEmpty()) s.high     = high.toLongLong() / 1000.0;

            result << s;
        }
    }
    return result;
}

QVector<ThermalZone> TemperatureInfo::thermalZones()
{
    QVector<ThermalZone> result;
    QDir thermal("/sys/class/thermal");
    if (!thermal.exists()) return result;

    for (const auto &tz : thermal.entryList({"thermal_zone*"}, QDir::Dirs)) {
        QString base = QString("/sys/class/thermal/%1/").arg(tz);
        QString type = FileUtil::readFile(base + "type").trimmed();
        QString temp = FileUtil::readFile(base + "temp").trimmed();
        if (temp.isEmpty()) continue;

        ThermalZone z;
        z.type        = type;
        z.temperature = temp.toLongLong() / 1000.0;
        z.path        = base;
        result << z;
    }
    return result;
}

std::optional<double> TemperatureInfo::cpuTemperature()
{
    // Try hwmon first (package temperature)
    for (const auto &s : hwmonSensors()) {
        if (s.label.toLower().contains("package") ||
            s.label.toLower().contains("cpu") ||
            s.name.toLower().contains("coretemp") ||
            s.name.toLower().contains("k10temp")) {
            return s.current;
        }
    }
    // Fallback: thermal zone x86_pkg_temp
    for (const auto &z : thermalZones()) {
        if (z.type.contains("x86_pkg_temp") || z.type.contains("cpu-thermal"))
            return z.temperature;
    }
    return std::nullopt;
}

bool TemperatureInfo::hasSensors()
{
    return !hwmonSensors().isEmpty() || !thermalZones().isEmpty();
}
