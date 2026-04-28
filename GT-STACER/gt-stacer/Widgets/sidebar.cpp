#include "sidebar.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSvgRenderer>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QGraphicsOpacityEffect>

// ══════════════════════════════════════════
//  SidebarButton
// ══════════════════════════════════════════
SidebarButton::SidebarButton(const SidebarItem &item, int index, QWidget *parent)
    : QWidget(parent), m_item(item), m_index(index)
{
    setCursor(Qt::PointingHandCursor);
    setToolTip(item.tooltip.isEmpty() ? item.label : item.tooltip);
    setMouseTracking(true);
    // Pre-render icon at 22px
    m_icon = renderIcon(22, QColor(0x6c, 0x70, 0x86));
}

QSize SidebarButton::sizeHint() const { return {220, 48}; }

QPixmap SidebarButton::renderIcon(int size, const QColor &color) const
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    if (m_item.iconSvg.isEmpty()) return pm;

    // Replace fill color in SVG
    QByteArray svg = m_item.iconSvg.toUtf8();
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(svg);
    QPainter painter(&pm);
    renderer.render(&painter);
    return pm;
}

void SidebarButton::setActive(bool active)
{
    m_active = active;
    // Re-render icon with new color
    QColor iconColor = active ? QColor(0x89, 0xb4, 0xfa) : QColor(0x6c, 0x70, 0x86);
    m_icon = renderIcon(22, iconColor);
    update();
}

void SidebarButton::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    update();
}

void SidebarButton::enterEvent(QEnterEvent *) { m_hovered = true;  update(); }
void SidebarButton::leaveEvent(QEvent *)       { m_hovered = false; m_pressed = false; update(); }
void SidebarButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) { m_pressed = true; update(); }
}
void SidebarButton::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_pressed) {
        m_pressed = false;
        emit clicked(m_index);
        update();
    }
}

void SidebarButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int W = width(), H = height();

    // Background
    QColor bg = Qt::transparent;
    if (m_active)  bg = QColor(0x31, 0x32, 0x44);
    else if (m_pressed) bg = QColor(0x2a, 0x2a, 0x3e);
    else if (m_hovered) bg = QColor(0x28, 0x28, 0x38);
    p.fillRect(rect(), bg);

    // Active left accent bar
    if (m_active) {
        p.fillRect(0, 0, 3, H, QColor(0x89, 0xb4, 0xfa));
    }

    // Hover left accent bar
    if (m_hovered && !m_active) {
        p.fillRect(0, 0, 3, H, QColor(0x45, 0x47, 0x5a));
    }

    // Icon (centered when collapsed, left-aligned when expanded)
    int iconX = m_collapsed ? (W - 22) / 2 : 20;
    int iconY = (H - 22) / 2;
    if (!m_icon.isNull())
        p.drawPixmap(iconX, iconY, m_icon);

    // Label (only in expanded mode)
    if (!m_collapsed) {
        QFont f = font();
        f.setPixelSize(13);
        if (m_active) f.setWeight(QFont::DemiBold);
        p.setFont(f);

        QColor textColor = m_active ? QColor(0x89, 0xb4, 0xfa)
                         : m_hovered ? QColor(0xcd, 0xd6, 0xf4)
                         : QColor(0xa6, 0xad, 0xc8);
        p.setPen(textColor);

        QRect textRect(52, 0, W - 56, H);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_item.label);
    }
}

// ══════════════════════════════════════════
//  Sidebar
// ══════════════════════════════════════════
Sidebar::Sidebar(QWidget *parent) : QWidget(parent)
{
    setObjectName("sidebar");
    setFixedWidth(EXPANDED_W);

    m_anim = new QPropertyAnimation(this, "sidebarWidth", this);
    m_anim->setDuration(250);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ──────────────────────────────────────────────────────
    // نستخدم QStackedWidget: صفحة موسّعة + صفحة مطوية
    auto *headerWidget = new QWidget;
    headerWidget->setObjectName("sidebarHeader");
    headerWidget->setFixedHeight(64);

    // وضع موسّع: [logo] [اسم+إصدار] [stretch] [زر الطيّ]
    auto *expandedHeader = new QWidget(headerWidget);
    auto *expandedLayout = new QHBoxLayout(expandedHeader);
    expandedLayout->setContentsMargins(12, 0, 8, 0);
    expandedLayout->setSpacing(8);

    m_logoLabel = new QLabel(expandedHeader);
    m_logoLabel->setPixmap(
        QPixmap(":/static/icons/gt-stacer.png").scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_logoLabel->setFixedSize(34, 34);
    expandedLayout->addWidget(m_logoLabel);

    auto *nameCol = new QVBoxLayout;
    nameCol->setSpacing(1);
    m_appName = new QLabel("GT-STACER");
    m_appName->setObjectName("sidebarAppName");
    m_version = new QLabel("v26.04");
    m_version->setObjectName("sidebarVersion");
    nameCol->addWidget(m_appName);
    nameCol->addWidget(m_version);
    expandedLayout->addLayout(nameCol);
    expandedLayout->addStretch();

    m_toggleBtn = new QPushButton("◀", expandedHeader);
    m_toggleBtn->setObjectName("sidebarToggle");
    m_toggleBtn->setFixedSize(22, 22);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setToolTip(tr("Collapse sidebar"));
    expandedLayout->addWidget(m_toggleBtn, 0, Qt::AlignVCenter);

    // وضع مطوي: [شعار مركزي] + [زر ▶ صغير بجانبه]
    auto *collapsedHeader = new QWidget(headerWidget);
    auto *collapsedLayout = new QHBoxLayout(collapsedHeader);
    collapsedLayout->setContentsMargins(4, 0, 4, 0);
    collapsedLayout->setSpacing(4);

    // شعار مصغر في وضع الطيّ
    auto *smallLogo = new QLabel(collapsedHeader);
    smallLogo->setPixmap(
        QPixmap(":/static/icons/gt-stacer.png").scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    smallLogo->setFixedSize(28, 28);
    collapsedLayout->addWidget(smallLogo, 0, Qt::AlignVCenter);

    // زر التوسيع ▶ صغير بجانب الشعار
    auto *expandBtn = new QPushButton("▶", collapsedHeader);
    expandBtn->setObjectName("sidebarToggle");
    expandBtn->setFixedSize(20, 20);
    expandBtn->setCursor(Qt::PointingHandCursor);
    expandBtn->setToolTip(tr("Expand sidebar"));
    collapsedLayout->addWidget(expandBtn, 0, Qt::AlignVCenter);
    collapsedHeader->hide();

    // نخزن مرجعاً للرأس المطوي لاستخدامه في toggleCollapse
    m_collapsedHeaderWidget = collapsedHeader;
    m_expandedHeaderWidget  = expandedHeader;

    // Layout الرئيسي للرأس
    auto *headerMain = new QHBoxLayout(headerWidget);
    headerMain->setContentsMargins(0, 0, 0, 0);
    headerMain->addWidget(expandedHeader);
    headerMain->addWidget(collapsedHeader);

    connect(m_toggleBtn, &QPushButton::clicked, this, &Sidebar::toggleCollapse);
    connect(expandBtn,   &QPushButton::clicked, this, &Sidebar::toggleCollapse);

    root->addWidget(headerWidget);

    // ── Separator ───────────────────────────────────────────────────
    auto *sep = new QWidget; sep->setFixedHeight(1);
    sep->setStyleSheet("background-color: #313244;");
    root->addWidget(sep);

    root->addSpacing(8);

    // ── Buttons placeholder (added via addItem) ──────────────────────
    // We add them dynamically after construction
    // (Qt layouts can't take a "placeholder"; we'll add directly to root)

    root->addStretch();

    // ── Version / attribution ───────────────────────────────────────
    auto *footerSep = new QWidget; footerSep->setFixedHeight(1);
    footerSep->setStyleSheet("background-color: #313244;");
    root->addWidget(footerSep);

    // Footer: النص الكامل في الوضع الموسّع، نقطة صغيرة في المطوي
    auto *footer = new QLabel("GNUTUX © 2026");
    footer->setObjectName("sidebarFooter");
    footer->setAlignment(Qt::AlignCenter);
    footer->setFixedHeight(30);
    footer->setWordWrap(false);
    // نخزن مرجعاً لإخفاء/إظهاره عند الطيّ
    m_footerLabel = footer;
    root->addWidget(footer);
}

void Sidebar::addItem(const SidebarItem &item)
{
    auto *btn = new SidebarButton(item, m_buttons.size(), this);
    connect(btn, &SidebarButton::clicked, this, &Sidebar::onButtonClicked);
    m_buttons << btn;

    auto *l = static_cast<QVBoxLayout*>(layout());
    int stretchIdx = l->count() - 3; // قبل فاصل Footer و Footer
    l->insertWidget(stretchIdx, btn);

    // تطبيق الوضع الحالي (مطوي/موسّع) على الزر الجديد
    btn->setCollapsed(m_collapsed);
}

void Sidebar::clearItems()
{
    auto *l = static_cast<QVBoxLayout*>(layout());
    for (auto *btn : m_buttons) {
        l->removeWidget(btn);
        delete btn;
    }
    m_buttons.clear();
}

void Sidebar::setActiveIndex(int index)
{
    m_activeIndex = index;
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons[i]->setActive(i == index);
}

void Sidebar::setSidebarWidth(int w)
{
    m_width = w;
    setFixedWidth(w);
}

void Sidebar::onButtonClicked(int index)
{
    setActiveIndex(index);
    emit pageRequested(index);
}

void Sidebar::toggleCollapse()
{
    m_collapsed = !m_collapsed;
    updateCollapsedState();

    m_anim->stop();
    m_anim->setStartValue(m_width);
    m_anim->setEndValue(m_collapsed ? COLLAPSED_W : EXPANDED_W);
    m_anim->start();
}

void Sidebar::updateCollapsedState()
{
    // تبديل بين الرأس الموسّع والمطوي
    if (m_expandedHeaderWidget)  m_expandedHeaderWidget->setVisible(!m_collapsed);
    if (m_collapsedHeaderWidget) m_collapsedHeaderWidget->setVisible(m_collapsed);
    // Footer: إخفاء النص في الوضع المطوي (لا يتسع للنص في 64px)
    if (m_footerLabel) {
        m_footerLabel->setText(m_collapsed ? "©" : "GNUTUX © 2026");
    }

    for (auto *btn : m_buttons)
        btn->setCollapsed(m_collapsed);
}
