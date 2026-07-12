#pragma once
#include <QColor>

// Central palette resolved against the active theme (AppManager::currentTheme()).
// Custom-painted widgets read these inside paintEvent() so they follow the theme
// automatically — qApp->setStyleSheet() on a theme change repaints every widget,
// which re-runs paintEvent and picks up the new colors with no extra wiring.
//
// Dark  = Catppuccin Mocha · Light = Catppuccin Latte.
namespace Theme {

bool   isDark();       // true for the dark (Mocha) palette

QColor base();         // window background
QColor mantle();       // card / raised panel background
QColor crust();        // deepest background (sidebar)
QColor surface();      // raised surface / progress track / inactive chip
QColor overlay();      // borders / muted separators
QColor text();         // primary text
QColor subtext();      // secondary text
QColor faint();        // most muted text (hints, captions)

QColor blue();         // primary accent
QColor sky();
QColor green();
QColor yellow();
QColor peach();
QColor red();
QColor mauve();

QColor gaugeTrack();   // translucent gauge track over the card background

} // namespace Theme
