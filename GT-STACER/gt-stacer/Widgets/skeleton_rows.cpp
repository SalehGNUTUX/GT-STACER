#include "skeleton_rows.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

SkeletonRows::SkeletonRows(int rowCount, QWidget *parent)
    : QWidget(parent), m_rowCount(rowCount)
{
    setMinimumHeight(rowCount * (m_rowHeight + m_rowGap));
    m_timer = new QTimer(this);
    m_timer->setInterval(60); // ≈ 16 fps — enough for a gentle shimmer
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_phase += 0.04;
        if (m_phase > 1.6) m_phase = -0.6; // brief pause at each cycle's edge
        update();
    });
}

void SkeletonRows::setRowCount(int n)
{
    m_rowCount = qMax(1, n);
    setMinimumHeight(m_rowCount * (m_rowHeight + m_rowGap));
    update();
}

void SkeletonRows::start()
{
    if (!m_timer->isActive()) m_timer->start();
    show();
}

void SkeletonRows::stop()
{
    m_timer->stop();
}

QSize SkeletonRows::sizeHint() const
{
    return {380, m_rowCount * (m_rowHeight + m_rowGap)};
}

void SkeletonRows::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor base = QColor(0x31, 0x32, 0x44);
    const QColor highlight = QColor(0x45, 0x47, 0x5a);

    for (int i = 0; i < m_rowCount; ++i) {
        const int y = i * (m_rowHeight + m_rowGap);
        const QRectF rect(8, y, width() - 16, m_rowHeight);

        // Pill background
        QPainterPath rounded;
        rounded.addRoundedRect(rect, 8, 8);
        p.fillPath(rounded, base);

        // Shimmer band — diagonal gradient that slides across the row.
        // Stagger rows so they don't all flash together (m_phase + i * 0.07).
        double phase = m_phase + i * 0.07;
        const double bandStart = phase - 0.25;
        const double bandEnd   = phase + 0.25;
        QLinearGradient g(rect.left() + bandStart * rect.width(), 0,
                          rect.left() + bandEnd   * rect.width(), 0);
        g.setColorAt(0.0, Qt::transparent);
        g.setColorAt(0.5, highlight);
        g.setColorAt(1.0, Qt::transparent);
        p.setClipPath(rounded);
        p.fillRect(rect, g);
        p.setClipping(false);
    }
}
