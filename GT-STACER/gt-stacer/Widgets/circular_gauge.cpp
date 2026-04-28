#include "circular_gauge.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

CircularGauge::CircularGauge(QWidget *parent) : QWidget(parent)
{
    m_anim = new QPropertyAnimation(this, "value", this);
    m_anim->setDuration(600);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// المساحة السفلية: اسم (Label) + نص فرعي (subText) + هامش
QSize CircularGauge::sizeHint()        const { return {180, 210}; }
QSize CircularGauge::minimumSizeHint() const { return {120, 145}; }

void CircularGauge::setLabel(const QString &l)    { m_label   = l; update(); }
void CircularGauge::setUnit(const QString &u)     { m_unit    = u; update(); }
void CircularGauge::setSubText(const QString &t)  { m_subText = t; update(); }
void CircularGauge::setRange(double mn, double mx){ m_min = mn; m_max = mx; update(); }
void CircularGauge::setAccentColor(const QColor &c){ m_accent = c; update(); }

void CircularGauge::setValue(double v)
{
    v = qBound(m_min, v, m_max);
    if (qFuzzyCompare(v, m_value)) return;
    m_value = v;
    emit valueChanged(v);
    update();
}

void CircularGauge::setValueAnimated(double v)
{
    v = qBound(m_min, v, m_max);
    m_anim->stop();
    m_anim->setStartValue(m_value);
    m_anim->setEndValue(v);
    m_anim->start();
}

QColor CircularGauge::arcColor() const
{
    double pct = (m_max > m_min) ? (m_value - m_min) / (m_max - m_min) * 100.0 : 0.0;
    if (pct < 60.0) return QColor("#a6e3a1");
    if (pct < 80.0) return QColor("#f9e2af");
    if (pct < 90.0) return QColor("#fab387");
    return QColor("#f38ba8");
}

void CircularGauge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int W = width(), H = height();

    // ── تقسيم المساحة الرأسية ───────────────────────────────────────
    // الدائرة تأخذ الجزء الأكبر، والنصوص تحتها بوضوح
    const int subH   = m_subText.isEmpty() ? 0 : 18;  // ارتفاع النص الفرعي
    const int labelH = 22;                              // ارتفاع الاسم
    const int footH  = labelH + subH + 4;              // المساحة السفلية الكاملة

    const int side = qMin(W, H - footH) - 12;
    if (side < 20) return;

    const int cx = W / 2;
    const int cy = (H - footH) / 2;
    const QRect gaugeRect(cx - side/2, cy - side/2, side, side);
    const int lw = qMax(6, side / 11);

    // ── Track arc ────────────────────────────────────────────────────
    QPen trackPen(QColor(255, 255, 255, 18), lw, Qt::SolidLine, Qt::FlatCap);
    p.setPen(trackPen);
    p.setBrush(Qt::NoBrush);
    const QRect arcRect = gaugeRect.adjusted(lw/2, lw/2, -lw/2, -lw/2);
    p.drawArc(arcRect, 225 * 16, -270 * 16);

    // ── Progress arc ─────────────────────────────────────────────────
    double pct = (m_max > m_min) ? (m_value - m_min) / (m_max - m_min) : 0.0;
    if (pct > 0.005) {
        QConicalGradient grad(arcRect.center(), 225.0);
        grad.setColorAt(0.0, m_accent);
        grad.setColorAt(1.0, arcColor());
        QPen arcPen(QBrush(grad), lw, Qt::SolidLine, Qt::RoundCap);
        p.setPen(arcPen);
        p.drawArc(arcRect, 225 * 16, -static_cast<int>(270 * 16 * pct));
    }

    // ── Glow ─────────────────────────────────────────────────────────
    int glowR = side / 3;
    QRadialGradient glow(cx, cy, glowR);
    glow.setColorAt(0.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 25));
    glow.setColorAt(1.0, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(glow));
    p.drawEllipse(QPoint(cx, cy), glowR, glowR);

    // ── قيمة المركز (النسبة المئوية) ─────────────────────────────────
    QFont valFont = font();
    valFont.setPixelSize(qMax(11, side / 4));
    valFont.setBold(true);
    p.setPen(QColor(0xcd, 0xd6, 0xf4));
    p.setFont(valFont);

    QString valStr = (m_unit == "%")
        ? QString::number(static_cast<int>(m_value)) + "%"
        : QString::number(m_value, 'f', 1) + " " + m_unit;
    p.drawText(gaugeRect, Qt::AlignCenter, valStr);

    // ── الاسم (Label) ─────────────────────────────────────────────────
    int labelY = H - footH;          // يبدأ مباشرة أسفل الدائرة
    QFont labelFont = font();
    labelFont.setPixelSize(qMax(10, side / 8));
    labelFont.setBold(true);
    p.setFont(labelFont);
    p.setPen(QColor(0xa6, 0xad, 0xc8));
    p.drawText(QRect(0, labelY, W, labelH), Qt::AlignCenter, m_label);

    // ── النص الفرعي (مثل "4.6 GB / 7.65 GB") — أسفل الاسم ───────────
    if (!m_subText.isEmpty()) {
        QFont subFont = font();
        subFont.setPixelSize(qMax(8, side / 10));
        p.setFont(subFont);
        p.setPen(QColor(0x6c, 0x70, 0x86));
        p.drawText(QRect(4, labelY + labelH, W - 8, subH),
                   Qt::AlignCenter, m_subText);
    }
}
