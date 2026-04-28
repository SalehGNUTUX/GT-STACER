#include "loading_overlay.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QConicalGradient>

// ══════════════════════════════════════════
//  LoadingOverlay
// ══════════════════════════════════════════
LoadingOverlay::LoadingOverlay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground);
    setObjectName("loadingOverlay");

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);

    // spacer to push spinner to center
    layout->addStretch();

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("color: #cdd6f4; font-size: 14px;");
    layout->addWidget(m_label);
    layout->addStretch();

    m_timer = new QTimer(this);
    m_timer->setInterval(16);  // ~60fps
    connect(m_timer, &QTimer::timeout, this, &LoadingOverlay::tick);

    hide();
    if (parent) resize(parent->size());
}

void LoadingOverlay::start(const QString &message)
{
    m_label->setText(message);
    m_angle = 0;
    show();
    raise();
    m_timer->start();
}

void LoadingOverlay::stop()
{
    m_timer->stop();
    hide();
}

void LoadingOverlay::tick()
{
    m_angle = (m_angle + 6) % 360;
    update();
}

void LoadingOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent background
    p.fillRect(rect(), QColor(0x18, 0x18, 0x25, 200));

    // Spinner
    const int side = 52;
    const int cx = width() / 2, cy = height() / 2 - 16;
    QRect spinRect(cx - side/2, cy - side/2, side, side);

    // Track
    p.setPen(QPen(QColor(255,255,255,30), 5, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(spinRect);

    // Arc
    QConicalGradient grad(spinRect.center(), m_angle);
    grad.setColorAt(0.0,  QColor("#89b4fa"));
    grad.setColorAt(0.75, QColor("#89b4fa"));
    grad.setColorAt(1.0,  Qt::transparent);
    p.setPen(QPen(QBrush(grad), 5, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(spinRect.adjusted(3, 3, -3, -3), m_angle * 16, -300 * 16);
}

void LoadingOverlay::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (parentWidget())
        resize(parentWidget()->size());
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
    p.setPen(QPen(QColor(255,255,255,30), 2));
    p.drawEllipse(r);
    QConicalGradient g(r.center(), m_angle);
    g.setColorAt(0,   QColor("#89b4fa"));
    g.setColorAt(0.7, QColor("#89b4fa"));
    g.setColorAt(1,   Qt::transparent);
    p.setPen(QPen(QBrush(g), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r.adjusted(1,1,-1,-1), m_angle * 16, -270 * 16);
}
