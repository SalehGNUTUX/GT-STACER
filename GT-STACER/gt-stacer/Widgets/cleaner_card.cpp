#include "cleaner_card.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QSvgRenderer>

CleanerCard::CleanerCard(const QString &iconSvg,
                          const QString &label,
                          const QString &description,
                          QWidget *parent)
    : QWidget(parent), m_iconSvg(iconSvg), m_label(label), m_description(description)
{
    setMinimumSize(140, 165);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setToolTip(description);
}

void CleanerCard::setChecked(bool on)
{
    if (m_checked == on) return;
    m_checked = on;
    update();
    emit toggled(on);
}

void CleanerCard::setSizeText(const QString &text)
{
    m_sizeText = text;
    update();
}

void CleanerCard::setHasDetails(bool on)
{
    if (m_hasDetails == on) return;
    m_hasDetails = on;
    update();
}

QPixmap CleanerCard::renderIcon(int size, const QColor &color) const
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QByteArray svg = m_iconSvg.toUtf8();
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
    QSvgRenderer renderer(svg);
    QPainter painter(&pm);
    renderer.render(&painter);
    return pm;
}

void CleanerCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) setChecked(!m_checked);
    QWidget::mousePressEvent(e);
}

void CleanerCard::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        // The first click already toggled in mousePressEvent — restore it so a
        // double-click stays a no-op on the check state.
        setChecked(!m_checked);
        emit doubleClicked();
    }
    QWidget::mouseDoubleClickEvent(e);
}

void CleanerCard::enterEvent(QEnterEvent *) { m_hovered = true;  update(); }
void CleanerCard::leaveEvent(QEvent *)       { m_hovered = false; update(); }

void CleanerCard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect().adjusted(2, 2, -2, -2);
    const double radius = 12.0;

    // Card background
    QColor bg = QColor(24, 24, 37);
    if (m_checked) bg = QColor(0x31, 0x32, 0x44);
    else if (m_hovered) bg = QColor(0x1f, 0x22, 0x38);

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    p.fillPath(path, bg);

    // Border (thicker + accent when checked)
    QPen border(QColor(0x31, 0x32, 0x44), 1);
    if (m_checked) border = QPen(QColor(0x89, 0xb4, 0xfa), 2);
    p.setPen(border);
    p.drawPath(path);

    // Icon — accent color when checked, muted otherwise
    const QColor iconColor = m_checked ? QColor(0xcb, 0xa6, 0xf7) : QColor(0xa6, 0xad, 0xc8);
    if (m_iconCache.isNull()) m_iconCache = renderIcon(48, QColor(0xa6, 0xad, 0xc8));
    QPixmap iconPm = renderIcon(48, iconColor);
    int iconX = (width() - 48) / 2;
    p.drawPixmap(iconX, 18, iconPm);

    // Label
    QFont labelFont = font();
    labelFont.setPixelSize(13);
    labelFont.setWeight(m_checked ? QFont::DemiBold : QFont::Normal);
    p.setFont(labelFont);
    p.setPen(m_checked ? QColor(0xcd, 0xd6, 0xf4) : QColor(0xa6, 0xad, 0xc8));
    QRectF labelRect(0, 76, width(), 22);
    p.drawText(labelRect, Qt::AlignCenter, m_label);

    // Size text (after scan)
    if (!m_sizeText.isEmpty()) {
        QFont sizeFont = font();
        sizeFont.setPixelSize(11);
        p.setFont(sizeFont);
        p.setPen(QColor(0x89, 0xb4, 0xfa));
        QRectF sizeRect(0, 98, width(), 18);
        p.drawText(sizeRect, Qt::AlignCenter, m_sizeText);
    }

    // Info badge in the top-right corner for categories that support drill-down.
    if (m_hasDetails) {
        const double br = 9.0;
        const QPointF c(width() - br - 8, br + 8);
        p.setBrush(QColor(0x89, 0xb4, 0xfa));
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, br, br);
        QFont infoFont = font();
        infoFont.setPixelSize(11);
        infoFont.setBold(true);
        p.setFont(infoFont);
        p.setPen(QColor(0x11, 0x11, 0x1b));
        p.drawText(QRectF(c.x() - br, c.y() - br, br * 2, br * 2),
                   Qt::AlignCenter, QStringLiteral("ⓘ"));
    }

    // Checkmark circle at the bottom
    const double cmRadius = 12;
    const QPointF cmCenter(width() / 2.0, height() - 22);
    p.setBrush(m_checked ? QColor(0x89, 0xb4, 0xfa) : QColor(0x31, 0x32, 0x44));
    p.setPen(QPen(m_checked ? QColor(0x89, 0xb4, 0xfa) : QColor(0x45, 0x47, 0x5a), 1.5));
    p.drawEllipse(cmCenter, cmRadius, cmRadius);
    if (m_checked) {
        p.setPen(QPen(QColor(24, 24, 37), 2));
        QPainterPath tick;
        tick.moveTo(cmCenter.x() - 5, cmCenter.y());
        tick.lineTo(cmCenter.x() - 1, cmCenter.y() + 4);
        tick.lineTo(cmCenter.x() + 5, cmCenter.y() - 3);
        p.drawPath(tick);
    }
}
