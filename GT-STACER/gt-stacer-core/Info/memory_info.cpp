#include "memory_info.h"
#include "../Utils/file_util.h"
#include <QRegularExpression>

MemoryData MemoryInfo::memory()
{
    MemoryData data;
    auto lines = FileUtil::readFile("/proc/meminfo").split('\n');

    qint64 memFree = 0, buffers = 0, cached = 0, sReclaimable = 0;

    auto parseKB = [](const QString &line) -> qint64 {
        auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        return parts.size() >= 2 ? parts[1].toLongLong() * 1024 : 0;
    };

    for (const auto &line : lines) {
        if      (line.startsWith("MemTotal:"))          data.totalRam    = parseKB(line);
        else if (line.startsWith("MemFree:"))           memFree           = parseKB(line);
        else if (line.startsWith("Buffers:"))           buffers           = parseKB(line);
        else if (line.startsWith("Cached:"))            cached            = parseKB(line);
        else if (line.startsWith("SReclaimable:"))      sReclaimable      = parseKB(line);
        else if (line.startsWith("SwapTotal:"))         data.totalSwap   = parseKB(line);
        else if (line.startsWith("SwapFree:"))          data.freeSwap    = parseKB(line);
    }

    data.cachedRam = cached + buffers + sReclaimable;
    data.freeRam   = memFree;
    data.usedRam   = data.totalRam - memFree - data.cachedRam;
    if (data.usedRam < 0) data.usedRam = data.totalRam - memFree;
    data.usedSwap  = data.totalSwap - data.freeSwap;

    return data;
}
