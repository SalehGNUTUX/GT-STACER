#include "theme.h"
#include "app_manager.h"

namespace {
// Default to the dark palette until AppManager has resolved a theme.
bool dark()
{
    return AppManager::instance()->currentTheme() != QLatin1String("light");
}
} // namespace

namespace Theme {

bool isDark() { return dark(); }

QColor base()    { return dark() ? QColor("#1e1e2e") : QColor("#eff1f5"); }
QColor mantle()  { return dark() ? QColor("#181825") : QColor("#e6e9ef"); }
QColor crust()   { return dark() ? QColor("#11111b") : QColor("#dce0e8"); }
QColor surface() { return dark() ? QColor("#313244") : QColor("#ccd0da"); }
QColor overlay() { return dark() ? QColor("#45475a") : QColor("#bcc0cc"); }
QColor text()    { return dark() ? QColor("#cdd6f4") : QColor("#4c4f69"); }
QColor subtext() { return dark() ? QColor("#a6adc8") : QColor("#6c6f85"); }
QColor faint()   { return dark() ? QColor("#6c7086") : QColor("#8c8fa1"); }

QColor blue()    { return dark() ? QColor("#89b4fa") : QColor("#1e66f5"); }
QColor sky()     { return dark() ? QColor("#89dceb") : QColor("#209fb5"); }
QColor green()   { return dark() ? QColor("#a6e3a1") : QColor("#40a02b"); }
QColor yellow()  { return dark() ? QColor("#f9e2af") : QColor("#df8e1d"); }
QColor peach()   { return dark() ? QColor("#fab387") : QColor("#fe640b"); }
QColor red()     { return dark() ? QColor("#f38ba8") : QColor("#d20f39"); }
QColor mauve()   { return dark() ? QColor("#cba6f7") : QColor("#8839ef"); }

QColor gaugeTrack() { return dark() ? QColor(255, 255, 255, 18) : QColor(76, 79, 105, 30); }

} // namespace Theme
