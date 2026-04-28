#include "format_util.h"

QString FormatUtil::formatBytes(qint64 bytes)
{
    const double kb = 1024.0, mb = kb * 1024, gb = mb * 1024, tb = gb * 1024;
    if (bytes >= tb) return QString::number(bytes / tb, 'f', 2) + " TB";
    if (bytes >= gb) return QString::number(bytes / gb, 'f', 2) + " GB";
    if (bytes >= mb) return QString::number(bytes / mb, 'f', 2) + " MB";
    if (bytes >= kb) return QString::number(bytes / kb, 'f', 2) + " KB";
    return QString::number(bytes) + " B";
}

QString FormatUtil::formatSpeed(qint64 bytesPerSec)
{
    return formatBytes(bytesPerSec) + "/s";
}

QString FormatUtil::formatPercent(double value)
{
    return QString::number(value, 'f', 1) + "%";
}

QString FormatUtil::formatTemperature(double celsius)
{
    return QString::number(celsius, 'f', 1) + " °C";
}

QString FormatUtil::formatDuration(int minutes)
{
    if (minutes < 60) return QString("%1 min").arg(minutes);
    int h = minutes / 60, m = minutes % 60;
    return QString("%1h %2m").arg(h).arg(m, 2, 10, QChar('0'));
}

QString FormatUtil::formatFrequency(double mhz)
{
    if (mhz >= 1000) return QString::number(mhz / 1000.0, 'f', 2) + " GHz";
    return QString::number(mhz, 'f', 0) + " MHz";
}
