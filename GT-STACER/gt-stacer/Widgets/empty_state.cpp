#include "empty_state.h"
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QVBoxLayout>

EmptyState::EmptyState(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 40, 40, 40);
    root->setSpacing(8);
    root->addStretch();

    // The icon itself is drawn in paintEvent (so the SVG can be re-coloured
    // by setIconSvg). We just reserve vertical space for it here.
    auto *spacer = new QWidget; spacer->setFixedHeight(0);
    root->addWidget(spacer, 0, Qt::AlignHCenter);

    m_titleLbl = new QLabel;
    m_titleLbl->setAlignment(Qt::AlignCenter);
    m_titleLbl->setStyleSheet("color:#cdd6f4;font-size:15px;font-weight:600;");
    m_titleLbl->setWordWrap(true);
    root->addWidget(m_titleLbl);

    m_subLbl = new QLabel;
    m_subLbl->setAlignment(Qt::AlignCenter);
    m_subLbl->setStyleSheet("color:#6c7086;font-size:12px;");
    m_subLbl->setWordWrap(true);
    root->addWidget(m_subLbl);

    m_actionBtn = new QPushButton;
    m_actionBtn->setObjectName("primaryButton");
    m_actionBtn->setMinimumWidth(140);
    m_actionBtn->hide();
    connect(m_actionBtn, &QPushButton::clicked, this, &EmptyState::actionClicked);
    auto *btnRow = new QVBoxLayout;
    btnRow->setAlignment(Qt::AlignHCenter);
    btnRow->addSpacing(8);
    btnRow->addWidget(m_actionBtn, 0, Qt::AlignHCenter);
    root->addLayout(btnRow);

    root->addStretch();
}

void EmptyState::setIconSvg(const QString &svg, int sizePx)
{
    m_iconSvg = svg;
    m_iconSize = sizePx;
    setContentsMargins(0, 0, 0, 0);
    update();
}

void EmptyState::setTitle(const QString &t)    { m_titleLbl->setText(t); }
void EmptyState::setSubtitle(const QString &s) { m_subLbl->setText(s); }

void EmptyState::setActionLabel(const QString &label)
{
    m_actionBtn->setText(label);
    m_actionBtn->setVisible(!label.isEmpty());
}

void EmptyState::clearAction() { m_actionBtn->hide(); }

void EmptyState::paintEvent(QPaintEvent *)
{
    if (m_iconSvg.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QByteArray svg = m_iconSvg.toUtf8();
    // Recolour SVG to muted blue-grey so the glyph reads as "empty" not "error".
    svg.replace("currentColor", "#6c7086");
    QSvgRenderer r(svg);
    int x = (width() - m_iconSize) / 2;
    // Place the icon slightly above vertical centre — the title/subtitle stack
    // sits in the lower half of the widget.
    int y = height() / 2 - m_iconSize - 30;
    if (y < 16) y = 16;
    r.render(&p, QRectF(x, y, m_iconSize, m_iconSize));
}
