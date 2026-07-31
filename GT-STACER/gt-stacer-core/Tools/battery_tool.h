#pragma once
#include <QString>

// Laptop battery-preservation controls (charge thresholds). Many laptops expose
// /sys/class/power_supply/BAT*/charge_control_{start,end}_threshold; capping the
// end threshold (e.g. 80%) markedly extends battery lifespan. On desktops (or
// laptops without the feature) `supported` is false and the UI hides the section.
// Reads are from sysfs; writes go through pkexec via CommandUtil.
struct ChargeThresholds {
    bool supported = false;   // an end-threshold attribute exists
    bool hasStart  = false;   // a start-threshold attribute exists too
    int  start     = -1;      // -1 when not applicable
    int  end       = 100;
};

class BatteryTool {
public:
    static ChargeThresholds thresholds();
    // Writes end (and start when hasStart) to every battery that supports it.
    // start<0 leaves the start threshold untouched. Values are clamped 0..100
    // and require start<end.
    static bool setThresholds(int start, int end);
};
