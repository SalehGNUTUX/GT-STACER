#include "gpu_info.h"
#include "../Utils/file_util.h"
#include "../Utils/command_util.h"
#include <QDir>

QVector<GpuData> GpuInfo::gpus()
{
    QVector<GpuData> result;

    if (auto nvidia = readNvidiaGpu()) result << *nvidia;
    if (auto amd    = readAmdGpu())    result << *amd;
    if (auto intel  = readIntelGpu())  result << *intel;

    if (result.isEmpty()) {
        // Fallback: try to read GPU name from lspci
        GpuData fallback;
        fallback.name   = detectGpuName();
        fallback.vendor = GpuVendor::Unknown;
        if (!fallback.name.isEmpty()) result << fallback;
    }
    return result;
}

std::optional<GpuData> GpuInfo::readNvidiaGpu()
{
    if (!CommandUtil::commandExists("nvidia-smi")) return std::nullopt;

    QString out = CommandUtil::exec(
        "nvidia-smi --query-gpu=name,driver_version,utilization.gpu,"
        "memory.used,memory.total,temperature.gpu,power.draw "
        "--format=csv,noheader,nounits");

    if (out.isEmpty()) return std::nullopt;

    auto parts = out.split(',');
    GpuData g;
    g.vendor        = GpuVendor::NVIDIA;
    g.name          = parts.value(0).trimmed();
    g.driverVersion = parts.value(1).trimmed();
    if (parts.size() > 2) g.usagePercent = parts[2].trimmed().toInt();
    if (parts.size() > 3) g.memUsedMB    = parts[3].trimmed().toInt();
    if (parts.size() > 4) g.memTotalMB   = parts[4].trimmed().toInt();
    if (parts.size() > 5) g.temperatureCelsius = parts[5].trimmed().toDouble();
    if (parts.size() > 6) {
        bool ok;
        double pw = parts[6].trimmed().toDouble(&ok);
        if (ok) g.powerWatts = static_cast<int>(pw);
    }
    return g;
}

std::optional<GpuData> GpuInfo::readAmdGpu()
{
    QDir drm("/sys/class/drm");
    if (!drm.exists()) return std::nullopt;

    for (const auto &entry : drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!entry.startsWith("card") || entry.contains('-')) continue;
        QString base = QString("/sys/class/drm/%1/device/").arg(entry);

        QString vendor = FileUtil::readFile(base + "vendor").trimmed();
        if (vendor != "0x1002") continue; // AMD vendor ID

        GpuData g;
        g.vendor = GpuVendor::AMD;

        // Read GPU name
        QString hwmonBase = base + "hwmon/";
        QDir hwmon(hwmonBase);
        if (hwmon.exists()) {
            for (const auto &hw : hwmon.entryList({"hwmon*"}, QDir::Dirs)) {
                g.name = FileUtil::readFile(hwmonBase + hw + "/name").trimmed();
                QString tempStr = FileUtil::readFile(hwmonBase + hw + "/temp1_input");
                if (!tempStr.isEmpty()) g.temperatureCelsius = tempStr.toLongLong() / 1000.0;
                QString powerStr = FileUtil::readFile(hwmonBase + hw + "/power1_average");
                if (!powerStr.isEmpty()) g.powerWatts = static_cast<int>(powerStr.toLongLong() / 1000000);
            }
        }

        // Read GPU utilization
        QString usageStr = FileUtil::readFile(base + "gpu_busy_percent");
        if (!usageStr.isEmpty()) g.usagePercent = usageStr.toInt();

        // Read VRAM
        QString memUsed  = FileUtil::readFile(base + "mem_info_vram_used");
        QString memTotal = FileUtil::readFile(base + "mem_info_vram_total");
        if (!memUsed.isEmpty())  g.memUsedMB  = static_cast<int>(memUsed.toLongLong()  / (1024*1024));
        if (!memTotal.isEmpty()) g.memTotalMB = static_cast<int>(memTotal.toLongLong() / (1024*1024));

        if (!g.name.isEmpty() || g.usagePercent.has_value()) return g;
    }
    return std::nullopt;
}

std::optional<GpuData> GpuInfo::readIntelGpu()
{
    // Intel GPU via /sys/class/drm/card*/gt/gt*/rc6_residency_ms or i915 driver
    QDir drm("/sys/class/drm");
    if (!drm.exists()) return std::nullopt;

    for (const auto &entry : drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!entry.startsWith("card") || entry.contains('-')) continue;
        QString base   = QString("/sys/class/drm/%1/device/").arg(entry);
        QString vendor = FileUtil::readFile(base + "vendor").trimmed();
        if (vendor != "0x8086") continue; // Intel vendor ID

        GpuData g;
        g.vendor = GpuVendor::Intel;
        g.name   = "Intel Integrated Graphics";

        // Read temperature via hwmon
        QString hwmonBase = base + "hwmon/";
        QDir hwmon(hwmonBase);
        if (hwmon.exists()) {
            for (const auto &hw : hwmon.entryList({"hwmon*"}, QDir::Dirs)) {
                QString name = FileUtil::readFile(hwmonBase + hw + "/name").trimmed();
                if (name.contains("i915") || name.contains("xe")) {
                    g.name = "Intel GPU (" + name + ")";
                    QString tempStr = FileUtil::readFile(hwmonBase + hw + "/temp1_input");
                    if (!tempStr.isEmpty()) g.temperatureCelsius = tempStr.toLongLong() / 1000.0;
                }
            }
        }

        // Try to get usage via intel_gpu_top (if available) or frequency scaling
        QString freqCur  = FileUtil::readFile(QString("/sys/class/drm/%1/gt_cur_freq_mhz").arg(entry));
        QString freqMax  = FileUtil::readFile(QString("/sys/class/drm/%1/gt_max_freq_mhz").arg(entry));
        if (!freqCur.isEmpty() && !freqMax.isEmpty()) {
            int cur = freqCur.toInt(), max = freqMax.toInt();
            if (max > 0) g.usagePercent = (cur * 100) / max;
        }
        return g;
    }
    return std::nullopt;
}

QString GpuInfo::detectGpuName()
{
    return CommandUtil::exec(
        "lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1 | sed 's/.*: //'");
}
