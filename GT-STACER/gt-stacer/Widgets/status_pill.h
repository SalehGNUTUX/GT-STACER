#pragma once
#include <QColor>
#include <QLabel>
#include <QString>

// Style a QLabel as a compact status pill — bold coloured text on a translucent
// same-hue background. One source of truth for the little state badges used on
// the Relief, Firewall and Backup pages (green = good/idle, blue = active,
// red = attention). Header-only; no moc needed.
inline void setStatusPill(QLabel *label, const QString &text, const QColor &color)
{
    QColor bg = color; bg.setAlpha(40);
    label->setText(text);
    label->setStyleSheet(QStringLiteral(
        "padding:2px 10px;border-radius:9px;font-weight:bold;color:%1;"
        "background:rgba(%2,%3,%4,%5);")
        .arg(color.name())
        .arg(bg.red()).arg(bg.green()).arg(bg.blue()).arg(bg.alpha()));
}
