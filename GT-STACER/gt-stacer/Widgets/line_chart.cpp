#include "line_chart.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

LineChart::LineChart(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(160, 100);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

int LineChart::addSeries(const QString &name, const QColor &color)
{
    m_series.append({name, color, {}});
    m_series.last().data.reserve(m_maxPoints);
    // Prefill with zeros so the line starts at the bottom rather than empty.
    for (int i = 0; i < m_maxPoints; ++i) m_series.last().data.append(0);
    return m_series.size() - 1;
}

void LineChart::appendPoint(int seriesIdx, double value)
{
    if (seriesIdx < 0 || seriesIdx >= m_series.size()) return;
    auto &d = m_series[seriesIdx].data;
    if (d.size() >= m_maxPoints) d.removeFirst();
    d.append(value);
    update();
}

void LineChart::setYRange(double min, double max)
{
    m_autoScaleY = false;
    m_yMin = min;
    m_yMax = max;
    update();
}

void LineChart::setAutoScaleY(double minRange)
{
    m_autoScaleY = true;
    m_autoScaleMin = minRange;
    update();
}

void LineChart::setMaxPoints(int n)
{
    m_maxPoints = qMax(2, n);
    for (auto &s : m_series) {
        while (s.data.size() > m_maxPoints) s.data.removeFirst();
        while (s.data.size() < m_maxPoints) s.data.prepend(0);
    }
    update();
}

void LineChart::setShowLegend(bool show)       { m_showLegend = show;  update(); }
void LineChart::setYLabelSuffix(const QString &s) { m_ySuffix = s;     update(); }
void LineChart::setBackgroundColor(const QColor &c) { m_bgColor = c;   update(); }
void LineChart::setGridColor(const QColor &c)     { m_gridColor = c;   update(); }
void LineChart::setAxisLabelColor(const QColor &c){ m_labelColor = c;  update(); }

double LineChart::effectiveYMax() const
{
    if (!m_autoScaleY) return m_yMax;
    double peak = 0;
    for (const auto &s : m_series)
        for (double v : s.data) if (v > peak) peak = v;
    double m = qMax(m_autoScaleMin, peak * 1.15);
    // Snap to round numbers to suppress visual jitter as the peak drifts.
    if      (m > 50000) m = qCeil(m / 10000.0) * 10000.0;
    else if (m > 5000)  m = qCeil(m / 1000.0)  * 1000.0;
    else if (m > 500)   m = qCeil(m / 100.0)   * 100.0;
    else if (m > 50)    m = qCeil(m / 10.0)    * 10.0;
    else                m = qCeil(m);
    return m;
}

void LineChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QFontMetricsF fm(font());
    // Reserve enough left margin for the widest Y label (e.g. "10240 KB/s").
    const double leftPad   = fm.horizontalAdvance(QStringLiteral("100000 ") + m_ySuffix) + 6;
    const double rightPad  = 6;
    const double topPad    = 6;
    const double legendH   = m_showLegend ? (fm.height() + 6) : 0;
    const double bottomPad = 4 + legendH;

    const QRectF plot(leftPad, topPad,
                      qMax(20.0, width()  - leftPad - rightPad),
                      qMax(20.0, height() - topPad  - bottomPad));

    if (m_bgColor.alpha() > 0) p.fillRect(rect(), m_bgColor);

    const double yMax = effectiveYMax();
    const double yMin = m_autoScaleY ? 0.0 : m_yMin;
    const double yRange = qMax(0.001, yMax - yMin);

    // ── Horizontal grid + Y labels (4 ticks) ───────────────────────────
    p.setPen(QPen(m_gridColor, 1, Qt::DotLine));
    for (int i = 0; i <= 4; ++i) {
        double y = plot.top() + (plot.height() * i) / 4.0;
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    p.setPen(m_labelColor);
    QFont labelFont = font();
    labelFont.setPixelSize(qMax(9, labelFont.pixelSize() > 0 ? labelFont.pixelSize() - 2 : 10));
    p.setFont(labelFont);
    for (int i = 0; i <= 4; ++i) {
        double val = yMin + (yRange * (4 - i)) / 4.0;
        QString txt;
        if (val >= 10) txt = QString::number(static_cast<qint64>(val));
        else           txt = QString::number(val, 'f', 1);
        if (!m_ySuffix.isEmpty()) txt += " " + m_ySuffix;
        double y = plot.top() + (plot.height() * i) / 4.0;
        QRectF labelRect(0, y - fm.height()/2.0, leftPad - 4, fm.height());
        p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, txt);
    }
    p.setFont(font());

    // ── Series lines ────────────────────────────────────────────────────
    for (const auto &s : m_series) {
        if (s.data.isEmpty()) continue;
        QPainterPath path;
        const int n = s.data.size();
        for (int i = 0; i < n; ++i) {
            double x = plot.left() + (plot.width() * i) / qMax(1, n - 1);
            double yNorm = (s.data[i] - yMin) / yRange;
            yNorm = qBound(0.0, yNorm, 1.0);
            double y = plot.bottom() - yNorm * plot.height();
            if (i == 0) path.moveTo(x, y);
            else        path.lineTo(x, y);
        }
        p.setPen(QPen(s.color, 1.6));
        p.drawPath(path);
    }

    // ── Legend ──────────────────────────────────────────────────────────
    if (m_showLegend && !m_series.isEmpty()) {
        double x = plot.left();
        const double y = height() - legendH + 2;
        for (const auto &s : m_series) {
            p.setPen(QPen(s.color, 2));
            p.drawLine(QPointF(x, y + fm.height()/2.0),
                       QPointF(x + 14, y + fm.height()/2.0));
            p.setPen(m_labelColor);
            p.drawText(QPointF(x + 18, y + fm.ascent()), s.name);
            x += 24 + fm.horizontalAdvance(s.name);
        }
    }
}
