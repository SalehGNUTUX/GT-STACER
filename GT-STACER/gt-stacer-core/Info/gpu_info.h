#pragma once
#include <QString>
#include <QVector>
#include <optional>

enum class GpuVendor { Unknown, Intel, AMD, NVIDIA };

struct GpuData {
    QString    name;
    GpuVendor  vendor        = GpuVendor::Unknown;
    QString    driverVersion;
    std::optional<int> usagePercent;
    std::optional<int> memUsedMB;
    std::optional<int> memTotalMB;
    std::optional<double> temperatureCelsius;
    std::optional<int> powerWatts;

    QString vendorName() const {
        switch (vendor) {
            case GpuVendor::Intel:  return "Intel";
            case GpuVendor::AMD:    return "AMD";
            case GpuVendor::NVIDIA: return "NVIDIA";
            default:                return "Unknown";
        }
    }
};

class GpuInfo {
public:
    static QVector<GpuData> gpus();

private:
    static std::optional<GpuData> readIntelGpu();
    static std::optional<GpuData> readAmdGpu();
    static std::optional<GpuData> readNvidiaGpu();
    static QString detectGpuName();
};
