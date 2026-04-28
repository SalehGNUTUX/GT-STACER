#pragma once
#include <QString>
#include <QVector>
#include <optional>

enum class BatteryStatus {
    Unknown,
    Charging,
    Discharging,
    Full,
    NotCharging
};

struct BatteryData {
    QString name;
    bool    present     = false;
    int     percent     = 0;
    BatteryStatus status = BatteryStatus::Unknown;
    std::optional<int>    timeRemainingMin;
    std::optional<double> capacityWh;
    std::optional<double> voltageV;
    std::optional<double> powerW;
    std::optional<double> temperature;
    QString technology;
    QString manufacturer;
    QString model;

    QString statusString() const;
};

class BatteryInfo {
public:
    static QVector<BatteryData> batteries();
    static BatteryData          primaryBattery();
    static bool                 hasBattery();
    static bool                 isOnAC();
};
