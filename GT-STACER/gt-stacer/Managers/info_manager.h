#pragma once
#include <QObject>
#include "../../gt-stacer-core/Info/cpu_info.h"
#include "../../gt-stacer-core/Info/memory_info.h"
#include "../../gt-stacer-core/Info/disk_info.h"
#include "../../gt-stacer-core/Info/network_info.h"
#include "../../gt-stacer-core/Info/process_info.h"
#include "../../gt-stacer-core/Info/system_info.h"
#include "../../gt-stacer-core/Info/gpu_info.h"
#include "../../gt-stacer-core/Info/temperature_info.h"
#include "../../gt-stacer-core/Info/battery_info.h"

class InfoManager : public QObject {
    Q_OBJECT
public:
    static InfoManager *instance();

    CpuUsage            cpuUsage()      { return CpuInfo::usage(); }
    MemoryData          memory()        { return MemoryInfo::memory(); }
    QVector<DiskPartition> partitions() { return DiskInfo::partitions(); }
    QVector<NetworkInterface> networks(){ return NetworkInfo::interfaces(); }
    QVector<ProcessData> processes()    { return ProcessInfo::processes(); }
    SystemData          systemInfo()    { return SystemInfo::info(); }
    QVector<GpuData>    gpus()          { return GpuInfo::gpus(); }
    QVector<ThermalSensor> sensors()    { return TemperatureInfo::hwmonSensors(); }
    BatteryData         battery()       { return BatteryInfo::primaryBattery(); }

private:
    explicit InfoManager(QObject *parent = nullptr);
    static InfoManager *m_instance;
};
