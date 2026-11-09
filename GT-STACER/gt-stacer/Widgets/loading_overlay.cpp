#include "loading_overlay.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QConicalGradient>
#include <QRadialGradient>
#include <cmath>
#include "../Managers/theme.h"

// ══════════════════════════════════════════
//  LoadingOverlay
// ══════════════════════════════════════════
namespace {
constexpr int kCardW = 360, kCardH = 250, kAnimY = 82;   // card size, animation centre
constexpr qreal kDelay = 0.15;   // s — a wait shorter than this never shows
QColor withAlpha(QColor c, int a) { c.setAlpha(qBound(0, a, 255)); return c; }
}

LoadingOverlay::LoadingOverlay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);   // block clicks underneath
    setAttribute(Qt::WA_NoSystemBackground);
    setObjectName("loadingOverlay");

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    QFont f = m_label->font();
    f.setPointSizeF(f.pointSizeF() * 1.15);
    f.setBold(true);
    m_label->setFont(f);

    m_detail = new QLabel(this);
    m_detail->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_detail->setWordWrap(true);

    m_timer = new QTimer(this);
    m_timer->setInterval(16);  // ~60fps
    connect(m_timer, &QTimer::timeout, this, &LoadingOverlay::tick);

    hide();
    if (parent) {
        resize(parent->size());
        parent->installEventFilter(this);   // follow the host's size by itself
    }
}

LoadingOverlay *LoadingOverlay::on(QWidget *host)
{
    if (!host) return nullptr;
    if (auto *o = host->findChild<LoadingOverlay *>(QString(), Qt::FindDirectChildrenOnly))
        return o;
    return new LoadingOverlay(host);
}

bool LoadingOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        resize(parentWidget()->size());
    return QWidget::eventFilter(watched, event);
}

void LoadingOverlay::applyColors()
{
    // Labels are real widgets (so Arabic text is laid out by Qt and the app-wide
    // bidi fixer); their colours follow the current theme.
    for (QLabel *l : {m_label, m_detail}) {
        QPalette pal = l->palette();
        pal.setColor(QPalette::WindowText, l == m_label ? Theme::text() : Theme::subtext());
        l->setPalette(pal);
        l->setStyleSheet(QString());   // no frozen colours
    }
}

void LoadingOverlay::start(const QString &message, const QString &detail)
{
    m_label->setText(message);
    m_detail->setText(detail);
    applyColors();
    if (isVisible() && m_timer->isActive()) {     // already waiting: new texts only
        m_detail->setVisible(m_clock.elapsed() >= kDelay * 1000 && !detail.isEmpty());
        return;
    }
    // Labels appear with the card, after the delay (see tick()).
    m_label->setVisible(false);
    m_detail->setVisible(false);
    m_angle = 0;
    m_clock.restart();
    if (parentWidget()) resize(parentWidget()->size());
    placeLabels();
    show();
    raise();
    m_timer->start();
}

void LoadingOverlay::setMessage(const QString &message) { m_label->setText(message); }

void LoadingOverlay::setDetail(const QString &detail)
{
    m_detail->setText(detail);
    m_detail->setVisible(m_label->isVisible() && !detail.isEmpty());
}

void LoadingOverlay::stop()
{
    m_timer->stop();
    hide();
}

void LoadingOverlay::tick()
{
    if (!m_label->isVisible() && m_clock.elapsed() >= kDelay * 1000) {
        m_label->setVisible(true);
        m_detail->setVisible(!m_detail->text().isEmpty());
    }
    m_angle = (m_angle + 5) % 360;
    update();
}

QRect LoadingOverlay::cardRect() const
{
    const int w = qMin(kCardW, width() - 32), h = qMin(kCardH, height() - 32);
    return QRect((width() - w) / 2, (height() - h) / 2, w, h);
}

void LoadingOverlay::placeLabels()
{
    const QRect c = cardRect();
    m_label->setGeometry(c.left() + 20, c.top() + kAnimY + 58, c.width() - 40, 28);
    m_detail->setGeometry(c.left() + 20, c.top() + kAnimY + 88, c.width() - 40, 44);
}

void LoadingOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal t = m_clock.isValid() ? m_clock.elapsed() / 1000.0 - kDelay : 0.0;
    if (t <= 0) return;                                     // clicks blocked, nothing drawn yet
    const qreal fade = qMin(1.0, t / 0.22);                 // ~220 ms fade-in

    // Backdrop: the theme's own background, dimmed — the page stays faintly visible.
    p.fillRect(rect(), withAlpha(Theme::base(), int(205 * fade)));

    // Card with a soft shadow and a hairline border.
    const QRect card = cardRect();
    p.setPen(Qt::NoPen);
    for (int i = 6; i >= 1; --i) {
        p.setBrush(withAlpha(QColor(0, 0, 0), int((Theme::isDark() ? 16 : 7) * fade)));
        p.drawRoundedRect(card.adjusted(-i, -i + 3, i, i + 3), 18 + i, 18 + i);
    }
    p.setBrush(withAlpha(Theme::mantle(), int(250 * fade)));
    p.setPen(QPen(withAlpha(Theme::overlay(), int(140 * fade)), 1));
    p.drawRoundedRect(card, 18, 18);

    const QPointF c(card.center().x(), card.top() + kAnimY);
    const QColor accent = Theme::blue(), accent2 = Theme::mauve();

    // 0) Soft accent glow behind the animation.
    QRadialGradient glow(c, 70);
    glow.setColorAt(0.0, withAlpha(accent, int((Theme::isDark() ? 55 : 35) * fade)));
    glow.setColorAt(1.0, withAlpha(accent, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(c, 70.0, 70.0);

    // 1) Pulse rings: three, staggered, expanding and fading — "searching".
    p.setBrush(Qt::NoBrush);
    for (int k = 0; k < 3; ++k) {
        const qreal ph = std::fmod(t / 1.8 + k / 3.0, 1.0);     // 0 → 1
        const qreal r  = 26 + ph * 34;
        p.setPen(QPen(withAlpha(accent, int((1.0 - ph) * 170 * fade)), 2));
        p.drawEllipse(c, r, r);
    }

    // 2) Track + rotating gradient arc.
    const QRectF ring(c.x() - 30, c.y() - 30, 60, 60);
    p.setPen(QPen(withAlpha(Theme::surface(), int(255 * fade)), 6, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(ring);
    QConicalGradient grad(c, m_angle);
    grad.setColorAt(0.0,  withAlpha(accent, int(255 * fade)));
    grad.setColorAt(0.45, withAlpha(accent2, int(255 * fade)));
    grad.setColorAt(0.8,  withAlpha(accent2, 0));
    grad.setColorAt(1.0,  withAlpha(accent, int(255 * fade)));
    p.setPen(QPen(QBrush(grad), 6, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(ring, m_angle * 16, -280 * 16);

    // 3) Breathing core.
    const qreal breath = 0.5 + 0.5 * std::sin(t * 3.2);
    QRadialGradient core(c, 15);
    core.setColorAt(0.0, withAlpha(accent.lighter(130), int(255 * fade)));
    core.setColorAt(1.0, withAlpha(accent2, int(170 * fade)));
    p.setPen(Qt::NoPen);
    p.setBrush(core);
    const qreal cr = 9 + 4 * breath;
    p.drawEllipse(c, cr, cr);

    // 4) Three bouncing dots at the bottom of the card.
    const qreal dy = card.bottom() - 22;
    for (int k = 0; k < 3; ++k) {
        const qreal b = std::max(0.0, std::sin(t * 5.0 - k * 0.7));
        p.setBrush(withAlpha(k == 1 ? accent2 : accent, int((110 + 145 * b) * fade)));
        p.drawEllipse(QPointF(card.center().x() + (k - 1) * 16, dy - 5 * b), 3.6, 3.6);
    }
}

void LoadingOverlay::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (parentWidget() && size() != parentWidget()->size())
        resize(parentWidget()->size());
    placeLabels();
}

// ══════════════════════════════════════════
//  SpinnerButton
// ══════════════════════════════════════════
SpinnerButton::SpinnerButton(QWidget *parent) : QWidget(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout, this, &SpinnerButton::tick);
    hide();
}

void SpinnerButton::start() { m_running = true;  show(); m_timer->start(); }
void SpinnerButton::stop()  { m_running = false; hide(); m_timer->stop();  }

void SpinnerButton::tick()  { m_angle = (m_angle + 8) % 360; update(); }

void SpinnerButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int s = qMin(width(), height()) - 2;
    QRect r((width()-s)/2, (height()-s)/2, s, s);
    p.setPen(QPen(Theme::surface(), 2));
    p.drawEllipse(r);
    QConicalGradient g(r.center(), m_angle);
    g.setColorAt(0,   Theme::blue());
    g.setColorAt(0.7, Theme::mauve());
    g.setColorAt(1,   Qt::transparent);
    p.setPen(QPen(QBrush(g), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r.adjusted(1,1,-1,-1), m_angle * 16, -270 * 16);
}
