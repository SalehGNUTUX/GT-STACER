#pragma once
#include <QString>
#include <QVector>
#include <optional>

struct ThermalSensor {
    QString name;
    QString label;
    double  current = 0.0;   // Celsius
    std::optional<double> critical;
    std::optional<double> high;
    QString hwmonPath;
};

struct ThermalZone {
    QString type;
    double  temperature = 0.0;
    QString path;
};

class TemperatureInfo {
public:
    static QVector<ThermalSensor> hwmonSensors();
    static QVector<ThermalZone>   thermalZones();
    static std::optional<double>  cpuTemperature();
    static bool                   hasSensors();
};
