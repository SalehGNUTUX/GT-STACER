#include "about_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QFrame>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About GT-STACER"));
    setMinimumWidth(460);
    setObjectName("aboutDialog");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 20);
    root->setSpacing(12);

    // Header row: logo + name/version
    auto *header = new QHBoxLayout;
    header->setSpacing(16);

    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(":/static/icons/gt-stacer.png")
        .scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    header->addWidget(logo, 0, Qt::AlignTop);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(2);
    auto *appName = new QLabel("GT-STACER");
    appName->setStyleSheet("font-size:24px;font-weight:bold;color:#cdd6f4;");
    auto *ver = new QLabel(tr("Version %1 · %2")
        .arg(APP_VERSION).arg(QLatin1String(APP_CHANNEL)));
    ver->setStyleSheet("color:#a6adc8;font-size:12px;");
    auto *tagline = new QLabel(tr("Linux System Optimizer & Monitor"));
    tagline->setStyleSheet("color:#89b4fa;font-size:12px;");
    titleCol->addWidget(appName);
    titleCol->addWidget(ver);
    titleCol->addWidget(tagline);
    titleCol->addStretch();
    header->addLayout(titleCol, 1);

    root->addLayout(header);

    // Separator
    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background:#313244;border:none;");
    sep->setFixedHeight(1);
    root->addWidget(sep);

    // Body — credits, license, links. Stored as a single rich-text label so
    // the dialog stays trivially translatable and copy-pasteable.
    auto *body = new QLabel;
    body->setTextFormat(Qt::RichText);
    body->setOpenExternalLinks(true);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextBrowserInteraction);
    body->setText(
        tr("<p><b>Developer:</b> GNUTUX &lt;gnutux.arabic@gmail.com&gt;<br>"
           "<b>License:</b> <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU GPL v3 or later</a><br>"
           "<b>Built with:</b> Qt 6 · C++17 · CMake</p>"
           "<p><b>Website:</b> <a href=\"https://salehgnutux.github.io/GT-STACER\">salehgnutux.github.io/GT-STACER</a><br>"
           "<b>Source code:</b> <a href=\"https://github.com/SalehGNUTUX/GT-STACER\">github.com/SalehGNUTUX/GT-STACER</a><br>"
           "<b>Report issues:</b> <a href=\"https://github.com/SalehGNUTUX/GT-STACER/issues\">github.com/SalehGNUTUX/GT-STACER/issues</a></p>"
           "<p>🙏 Inspired by <a href=\"https://github.com/oguzhaninan/Stacer\">Stacer</a> by Oguzhan INAN — "
           "rebuilt with modern tooling, broader hardware support, and security hardening for 2026.</p>"
           "<p style=\"color:#6c7086;font-size:11px;\">"
           "GT-STACER comes with ABSOLUTELY NO WARRANTY. This is free software, "
           "and you are welcome to redistribute it under the terms of the GPL v3."
           "</p>"));
    root->addWidget(body);

    // Close button
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *closeBtn = new QPushButton(tr("Close"));
    closeBtn->setObjectName("primaryButton");
    closeBtn->setDefault(true);
    closeBtn->setMinimumWidth(96);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);
}
