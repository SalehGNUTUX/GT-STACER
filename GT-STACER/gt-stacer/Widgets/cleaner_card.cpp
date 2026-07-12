#include "cleaner_card.h"
#include "../Managers/theme.h"
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

    // Card background — hover sits halfway between the card and the raised surface.
    QColor bg = Theme::mantle();
    if (m_checked) bg = Theme::surface();
    else if (m_hovered) {
        const QColor a = Theme::mantle(), b = Theme::surface();
        bg = QColor((a.red()+b.red())/2, (a.green()+b.green())/2, (a.blue()+b.blue())/2);
    }

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    p.fillPath(path, bg);

    // Border (thicker + accent when checked)
    QPen border(Theme::overlay(), 1);
    if (m_checked) border = QPen(Theme::blue(), 2);
    p.setPen(border);
    p.drawPath(path);

    // Icon — accent color when checked, muted otherwise
    const QColor iconColor = m_checked ? Theme::mauve() : Theme::subtext();
    QPixmap iconPm = renderIcon(48, iconColor);
    int iconX = (width() - 48) / 2;
    p.drawPixmap(iconX, 18, iconPm);

    // Label
    QFont labelFont = font();
    labelFont.setPixelSize(13);
    labelFont.setWeight(m_checked ? QFont::DemiBold : QFont::Normal);
    p.setFont(labelFont);
    p.setPen(m_checked ? Theme::text() : Theme::subtext());
    QRectF labelRect(0, 76, width(), 22);
    p.drawText(labelRect, Qt::AlignCenter, m_label);

    // Size text (after scan)
    if (!m_sizeText.isEmpty()) {
        QFont sizeFont = font();
        sizeFont.setPixelSize(11);
        p.setFont(sizeFont);
        p.setPen(Theme::blue());
        QRectF sizeRect(0, 98, width(), 18);
        p.drawText(sizeRect, Qt::AlignCenter, m_sizeText);
    }

    // Info badge in the top-right corner for categories that support drill-down.
    if (m_hasDetails) {
        const double br = 9.0;
        const QPointF c(width() - br - 8, br + 8);
        p.setBrush(Theme::blue());
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, br, br);
        QFont infoFont = font();
        infoFont.setPixelSize(11);
        infoFont.setBold(true);
        p.setFont(infoFont);
        p.setPen(Theme::isDark() ? QColor(0x11, 0x11, 0x1b) : QColor(0xef, 0xf1, 0xf5));
        p.drawText(QRectF(c.x() - br, c.y() - br, br * 2, br * 2),
                   Qt::AlignCenter, QStringLiteral("ⓘ"));
    }

    // Checkmark circle at the bottom
    const double cmRadius = 12;
    const QPointF cmCenter(width() / 2.0, height() - 22);
    p.setBrush(m_checked ? Theme::blue() : Theme::surface());
    p.setPen(QPen(m_checked ? Theme::blue() : Theme::overlay(), 1.5));
    p.drawEllipse(cmCenter, cmRadius, cmRadius);
    if (m_checked) {
        p.setPen(QPen(Theme::isDark() ? QColor(24, 24, 37) : QColor(0xef, 0xf1, 0xf5), 2));
        QPainterPath tick;
        tick.moveTo(cmCenter.x() - 5, cmCenter.y());
        tick.lineTo(cmCenter.x() - 1, cmCenter.y() + 4);
        tick.lineTo(cmCenter.x() + 5, cmCenter.y() - 3);
        p.drawPath(tick);
    }
}
