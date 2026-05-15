#pragma once
#include <QString>

// Inline SVG icons for the System Cleaner categories. Each function returns
// an SVG payload where `currentColor` is the recolor token (so the card can
// recolor it to match its accent gradient).
namespace CleanerIcons {

inline QString trash() {
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        "stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
        "<path d='M3 6h18'/>"
        "<path d='M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2'/>"
        "<path d='M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6'/>"
        "<path d='M10 11v6'/><path d='M14 11v6'/></svg>");
}

inline QString appCache() {
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        "stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
        "<rect x='3' y='4' width='18' height='5' rx='1.5'/>"
        "<rect x='3' y='11' width='18' height='5' rx='1.5'/>"
        "<path d='M8 6h0.01'/><path d='M8 13h0.01'/>"
        "<path d='M6 18h12'/></svg>");
}

inline QString appLogs() {
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        "stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
        "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
        "<polyline points='14 2 14 8 20 8'/>"
        "<line x1='8' y1='13' x2='16' y2='13'/>"
        "<line x1='8' y1='17' x2='13' y2='17'/></svg>");
}

inline QString crashReports() {
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        "stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
        "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
        "<polyline points='14 2 14 8 20 8'/>"
        "<line x1='9' y1='13' x2='15' y2='17'/>"
        "<line x1='15' y1='13' x2='9' y2='17'/></svg>");
}

inline QString aptCache() {
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        "stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
        "<path d='M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z'/>"
        "<line x1='3.27' y1='6.96' x2='12' y2='12.01'/>"
        "<line x1='12' y1='22.08' x2='12' y2='12'/>"
        "<polyline points='20.73 6.96 12 12.01 3.27 6.96'/></svg>");
}

inline QString thumbnails() {
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        "stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
        "<rect x='3' y='3' width='18' height='18' rx='2'/>"
        "<circle cx='8.5' cy='8.5' r='1.5'/>"
        "<polyline points='21 15 16 10 5 21'/></svg>");
}

inline QString oldKernels() {
    return QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        "stroke-width='1.4' stroke-linecap='round' stroke-linejoin='round'>"
        "<rect x='4' y='4' width='16' height='16' rx='2'/>"
        "<rect x='9' y='9' width='6' height='6'/>"
        "<path d='M9 1v3'/><path d='M15 1v3'/>"
        "<path d='M9 20v3'/><path d='M15 20v3'/>"
        "<path d='M20 9h3'/><path d='M20 15h3'/>"
        "<path d='M1 9h3'/><path d='M1 15h3'/></svg>");
}

} // namespace CleanerIcons
