#include "welcome_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QApplication>

bool WelcomeDialog::shouldShow()
{
    QSettings s("GNUTUX", "GT-STACER");
    return !s.value("welcomeShown", false).toBool();
}

void WelcomeDialog::markShown()
{
    QSettings s("GNUTUX", "GT-STACER");
    s.setValue("welcomeShown", true);
}

struct PageData {
    QString icon, title, desc;
    QColor  accent;
};

static const QVector<PageData> PAGES = {
    {"🖥️",  "Welcome to GT-STACER",
     "GT-STACER is a modern Linux system optimizer and monitor.\n"
     "Monitor your CPU, GPU, RAM, disk, network, and temperature in real time.\n\n"
     "Inspired by Stacer — rebuilt for GNU/Linux 2026.",
     QColor("#89b4fa")},
    {"📊",  "Dashboard",
     "The Dashboard gives you an instant overview of your system:\n"
     "• Circular gauges for CPU, RAM, Disk and Swap\n"
     "• System info: hostname, OS, kernel, uptime\n"
     "• Network speed, GPU status, and battery level",
     QColor("#89dceb")},
    {"⚙️",  "Services & Processes",
     "Manage system services with full start/stop/enable/disable support.\n"
     "Supports systemd · OpenRC · runit · SysV init.\n\n"
     "Monitor and terminate running processes sorted by CPU or memory usage.",
     QColor("#cba6f7")},
    {"📦",  "Package Manager",
     "GT-STACER detects your distribution automatically and supports\n"
     "28+ package managers: APT · DNF · Pacman · Zypper · Flatpak · Snap\n"
     "XBPS · APK · Portage · Nix · Homebrew and many more.",
     QColor("#a6e3a1")},
    {"🧹",  "System Cleaner",
     "Free up disk space by removing:\n"
     "• Crash reports and system logs\n"
     "• Application caches and thumbnails\n"
     "• Trash contents and package cache",
     QColor("#f9e2af")},
    {"🌐",  "Helpers & APT Sources",
     "Edit your /etc/hosts file directly from the app.\n"
     "Manage APT repositories: add, remove, enable or disable sources.\n\n"
     "All operations are authenticated securely via polkit.",
     QColor("#fab387")},
};

WelcomeDialog::WelcomeDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Welcome to GT-STACER"));
    setMinimumSize(560, 420);
    setModal(true);
    setObjectName("welcomeDialog");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Pages stack
    m_stack = new QStackedWidget;
    for (const auto &p : PAGES)
        m_stack->addWidget(makePage(p.icon, p.title, p.desc, p.accent));
    root->addWidget(m_stack, 1);

    // Bottom bar
    auto *bar = new QWidget;
    bar->setObjectName("welcomeBar");
    bar->setFixedHeight(56);
    auto *barL = new QHBoxLayout(bar);
    barL->setContentsMargins(20, 0, 20, 0);

    m_skip = new QPushButton(tr("Skip"));
    m_skip->setObjectName("welcomeSkip");
    barL->addWidget(m_skip);

    barL->addStretch();

    m_dots = new QLabel;
    m_dots->setAlignment(Qt::AlignCenter);
    barL->addWidget(m_dots);

    barL->addStretch();

    m_prev = new QPushButton(tr("← Back"));
    m_prev->setObjectName("welcomeNav");
    m_next = new QPushButton(tr("Next →"));
    m_next->setObjectName("welcomeNext");
    barL->addWidget(m_prev);
    barL->addSpacing(8);
    barL->addWidget(m_next);

    root->addWidget(bar);

    connect(m_next, &QPushButton::clicked, this, &WelcomeDialog::nextPage);
    connect(m_prev, &QPushButton::clicked, this, &WelcomeDialog::prevPage);
    connect(m_skip, &QPushButton::clicked, this, [this]{
        markShown(); accept();
    });

    updateButtons();
}

QWidget *WelcomeDialog::makePage(const QString &icon, const QString &title,
                                  const QString &desc, const QColor &accent)
{
    auto *page = new QWidget;
    page->setObjectName("welcomePage");
    auto *l = new QVBoxLayout(page);
    l->setContentsMargins(48, 40, 48, 20);
    l->setSpacing(20);

    // Icon
    auto *iconLbl = new QLabel(icon);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet(QString("font-size:52px;"));
    l->addWidget(iconLbl);

    // Title
    auto *titleLbl = new QLabel(title);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QString("font-size:20px;font-weight:bold;color:%1;")
                                .arg(accent.name()));
    l->addWidget(titleLbl);

    // Separator
    auto *sep = new QWidget;
    sep->setFixedHeight(2);
    sep->setStyleSheet(QString("background:rgba(%1,%2,%3,80);border-radius:1px;")
        .arg(accent.red()).arg(accent.green()).arg(accent.blue()));
    l->addWidget(sep);

    // Description
    auto *descLbl = new QLabel(desc);
    descLbl->setAlignment(Qt::AlignCenter);
    descLbl->setWordWrap(true);
    descLbl->setStyleSheet("font-size:13px;color:#a6adc8;line-height:1.6;");
    l->addWidget(descLbl);
    l->addStretch();
    return page;
}

void WelcomeDialog::nextPage()
{
    int idx = m_stack->currentIndex();
    if (idx < m_stack->count() - 1) {
        m_stack->setCurrentIndex(idx + 1);
        updateButtons();
    } else {
        markShown();
        accept();
    }
}

void WelcomeDialog::prevPage()
{
    int idx = m_stack->currentIndex();
    if (idx > 0) { m_stack->setCurrentIndex(idx - 1); updateButtons(); }
}

void WelcomeDialog::updateButtons()
{
    int idx = m_stack->currentIndex();
    int last = m_stack->count() - 1;
    m_prev->setVisible(idx > 0);
    m_next->setText(idx == last ? tr("Get Started!") : tr("Next →"));

    // Dots indicator
    QString dots;
    for (int i = 0; i <= last; ++i)
        dots += (i == idx) ? "●  " : "○  ";
    m_dots->setText(dots.trimmed());
}
