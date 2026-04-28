#pragma once
#include <QString>

struct MemoryData {
    qint64 totalRam   = 0;
    qint64 usedRam    = 0;
    qint64 freeRam    = 0;
    qint64 cachedRam  = 0;
    qint64 totalSwap  = 0;
    qint64 usedSwap   = 0;
    qint64 freeSwap   = 0;

    double ramPercent()  const { return totalRam  > 0 ? (usedRam  * 100.0) / totalRam  : 0.0; }
    double swapPercent() const { return totalSwap > 0 ? (usedSwap * 100.0) / totalSwap : 0.0; }
};

class MemoryInfo {
public:
    static MemoryData memory();
};
