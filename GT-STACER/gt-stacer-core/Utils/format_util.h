#pragma once
#include <QString>

class FormatUtil {
public:
    static QString formatBytes(qint64 bytes);
    static QString formatSpeed(qint64 bytesPerSec);
    static QString formatPercent(double value);
    static QString formatTemperature(double celsius);
    static QString formatDuration(int minutes);
    static QString formatFrequency(double mhz);
};
