#include "battery_tool.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QFile>
#include <algorithm>

namespace {
const char *kBase = "/sys/class/power_supply";

QString readAttr(const QString &dir, const QString &attr)
{
    QFile f(dir + '/' + attr);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

// Directories under /sys/class/power_supply that are batteries.
QStringList batteryDirs()
{
    QStringList out;
    const QDir base(QString::fromLatin1(kBase));
    for (const QString &e : base.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString dir = base.filePath(e);
        if (readAttr(dir, "type").compare("Battery", Qt::CaseInsensitive) == 0)
            out << dir;
    }
    return out;
}
}

ChargeThresholds BatteryTool::thresholds()
{
    ChargeThresholds t;
    for (const QString &dir : batteryDirs()) {
        const QString end = readAttr(dir, "charge_control_end_threshold");
        if (end.isEmpty()) continue;          // this battery has no cap control
        t.supported = true;
        t.end       = end.toInt();
        const QString start = readAttr(dir, "charge_control_start_threshold");
        if (!start.isEmpty()) { t.hasStart = true; t.start = start.toInt(); }
        break;                                 // first capable battery drives the UI
    }
    return t;
}

bool BatteryTool::setThresholds(int start, int end)
{
    end = std::clamp(end, 1, 100);
    if (start >= 0) {
        start = std::clamp(start, 0, 99);
        if (start >= end) start = end - 1;
    }

    // Build a root shell snippet that writes every capable battery. Values are
    // integers we clamped above, so no untrusted text reaches the shell.
    QString script;
    for (const QString &dir : batteryDirs()) {
        if (readAttr(dir, "charge_control_end_threshold").isEmpty()) continue;
        // Some kernels reject end<start ordering, so write start first.
        if (start >= 0 && !readAttr(dir, "charge_control_start_threshold").isEmpty())
            script += QStringLiteral("echo %1 > %2/charge_control_start_threshold; ")
                          .arg(start).arg(dir);
        script += QStringLiteral("echo %1 > %2/charge_control_end_threshold; ")
                      .arg(end).arg(dir);
    }
    if (script.isEmpty()) return false;

    return CommandUtil::execProgram("pkexec", {"sh", "-c", script}, 15000) == 0;
}
