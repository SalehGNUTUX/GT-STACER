#include "per_core_bars.h"
#include <QPainter>

PerCoreBars::PerCoreBars(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(70);
}

void PerCoreBars::setValues(const QVector<double> &percents)
{
    m_values = percents;
    update();
}

static QColor loadColor(double p)
{
    if (p < 50)  return QColor(0xa6, 0xe3, 0xa1); // green
    if (p < 75)  return QColor(0xf9, 0xe2, 0xaf); // yellow
    if (p < 90)  return QColor(0xfa, 0xb3, 0x87); // orange
    return                QColor(0xf3, 0x8b, 0xa8); // red
}

void PerCoreBars::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_values.isEmpty()) {
        p.setPen(QColor(0x6c, 0x70, 0x86));
        p.drawText(rect(), Qt::AlignCenter, tr("No per-core data"));
        return;
    }

    const int   n        = m_values.size();
    const QFontMetricsF fm(font());
    const double labelH  = fm.height() + 2;
    const double topPad  = 4;
    const double leftPad = 6;
    const double rightPad= 6;

    const double areaW = qMax(40.0, width() - leftPad - rightPad);
    const double areaH = qMax(20.0, height() - topPad - labelH);
    const double slot  = areaW / n;
    const double barW  = qMax(3.0, qMin(slot * 0.7, 22.0));

    for (int i = 0; i < n; ++i) {
        double v = qBound(0.0, m_values[i], 100.0);
        double h = (v / 100.0) * areaH;
        double cx = leftPad + slot * (i + 0.5);
        QRectF bar(cx - barW/2.0, topPad + (areaH - h), barW, h);
        QRectF track(cx - barW/2.0, topPad, barW, areaH);
        p.fillRect(track, QColor(0x31, 0x32, 0x44, 120));
        p.fillRect(bar, loadColor(v));

        QString label = QString::number(i);
        p.setPen(QColor(0xa6, 0xad, 0xc8));
        p.drawText(QRectF(cx - slot/2.0, height() - labelH, slot, labelH),
                   Qt::AlignCenter, label);
    }
}
