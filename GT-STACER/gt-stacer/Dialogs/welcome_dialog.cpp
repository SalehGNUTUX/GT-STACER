#include "welcome_dialog.h"
#include "../Managers/language_util.h"
#include "../Managers/app_manager.h"
#include "../Managers/setting_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
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

namespace {
struct PageData { QString icon, title, desc; QColor accent; };

// Built fresh each time so tr() resolves in the current language (the content is
// translatable, unlike a static array evaluated before the translator loads).
QVector<PageData> pages()
{
    return {
        {"🖥️", WelcomeDialog::tr("Welcome to GT-STACER"),
         WelcomeDialog::tr("GT-STACER is a modern Linux system optimizer and monitor.\n"
            "Monitor your CPU, GPU, RAM, disk, network, and temperature in real time.\n\n"
            "Inspired by Stacer — rebuilt for GNU/Linux 2026."),
         QColor("#89b4fa")},
        {"📊", WelcomeDialog::tr("Dashboard"),
         WelcomeDialog::tr("The Dashboard gives you an instant overview of your system:\n"
            "• Circular gauges for CPU, RAM, Disk and Swap\n"
            "• System info: hostname, OS, kernel, uptime\n"
            "• Network speed, GPU status, and battery level"),
         QColor("#89dceb")},
        {"⚙️", WelcomeDialog::tr("Services & Processes"),
         WelcomeDialog::tr("Manage system services with full start/stop/enable/disable support.\n"
            "Supports systemd · OpenRC · runit · SysV init.\n\n"
            "Monitor and terminate running processes sorted by CPU or memory usage."),
         QColor("#cba6f7")},
        {"📦", WelcomeDialog::tr("Package Manager"),
         WelcomeDialog::tr("GT-STACER detects your distribution automatically and supports\n"
            "28+ package managers: APT · DNF · Pacman · Zypper · Flatpak · Snap\n"
            "XBPS · APK · Portage · Nix · Homebrew and many more."),
         QColor("#a6e3a1")},
        {"💾", WelcomeDialog::tr("Backup, Recovery & Cleaner"),
         WelcomeDialog::tr("Create system restore points (Timeshift / Snapper / ZFS) and mirror\n"
            "your home folder. Recover deleted files with PhotoRec. Free up disk space\n"
            "by clearing caches, logs, crash reports and the trash."),
         QColor("#f9e2af")},
        {"🛡️", WelcomeDialog::tr("Network, Power & Security"),
         WelcomeDialog::tr("Watch live connections, switch the power profile and keep the system\n"
            "awake, manage the firewall, and relieve RAM/CPU/disk pressure.\n\n"
            "All privileged operations are authenticated securely via polkit."),
         QColor("#fab387")},
    };
}
}

WelcomeDialog::WelcomeDialog(QWidget *parent) : QDialog(parent)
{
    setMinimumSize(560, 440);
    setModal(true);
    setObjectName("welcomeDialog");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Language picker strip — the welcome tour is the natural place to choose it.
    auto *langBar = new QHBoxLayout;
    langBar->setContentsMargins(16, 10, 16, 0);
    auto *langLbl = new QLabel("🌐");
    m_lang = new QComboBox;
    m_lang->setMinimumWidth(180);
    langBar->addWidget(langLbl);
    langBar->addWidget(m_lang);
    langBar->addStretch();
    root->addLayout(langBar);

    m_stack = new QStackedWidget;
    root->addWidget(m_stack, 1);

    auto *bar = new QWidget;
    bar->setObjectName("welcomeBar");
    bar->setFixedHeight(56);
    auto *barL = new QHBoxLayout(bar);
    barL->setContentsMargins(20, 0, 20, 0);
    m_skip = new QPushButton;
    m_skip->setObjectName("welcomeSkip");
    barL->addWidget(m_skip);
    barL->addStretch();
    m_dots = new QLabel;
    m_dots->setAlignment(Qt::AlignCenter);
    barL->addWidget(m_dots);
    barL->addStretch();
    m_prev = new QPushButton;
    m_prev->setObjectName("welcomeNav");
    m_next = new QPushButton;
    m_next->setObjectName("welcomeNext");
    barL->addWidget(m_prev);
    barL->addSpacing(8);
    barL->addWidget(m_next);
    root->addWidget(bar);

    connect(m_next, &QPushButton::clicked, this, &WelcomeDialog::nextPage);
    connect(m_prev, &QPushButton::clicked, this, &WelcomeDialog::prevPage);
    connect(m_skip, &QPushButton::clicked, this, [this]{ markShown(); accept(); });

    // Populate the language picker and reflect the current language.
    LangUtil::fillCombo(m_lang, tr("Auto (system language)"));
    const int cur = m_lang->findData(SettingManager::instance()->language());
    m_lang->setCurrentIndex(cur >= 0 ? cur : 0);
    connect(m_lang, QOverload<int>::of(&QComboBox::activated), this, [this](int){
        AppManager::instance()->changeLanguage(m_lang->currentData().toString());
        retranslate();   // the dialog isn't a page, so it re-translates itself
    });

    rebuildPages();
    retranslate();
}

void WelcomeDialog::rebuildPages()
{
    const int keep = m_stack->currentIndex();
    while (m_stack->count()) {
        QWidget *w = m_stack->widget(0);
        m_stack->removeWidget(w);
        w->deleteLater();
    }
    for (const auto &p : pages())
        m_stack->addWidget(makePage(p.icon, p.title, p.desc, p.accent));
    m_stack->setCurrentIndex(qBound(0, keep, m_stack->count() - 1));
}

void WelcomeDialog::retranslate()
{
    setWindowTitle(tr("Welcome to GT-STACER"));
    setLayoutDirection(qApp->layoutDirection());
    m_skip->setText(tr("Skip"));
    m_prev->setText(tr("← Back"));
    // Refill the picker's translatable "Auto" label without losing the choice.
    LangUtil::fillCombo(m_lang, tr("Auto (system language)"));
    const int cur = m_lang->findData(SettingManager::instance()->language());
    m_lang->setCurrentIndex(cur >= 0 ? cur : 0);
    rebuildPages();
    updateButtons();
}

QWidget *WelcomeDialog::makePage(const QString &icon, const QString &title,
                                  const QString &desc, const QColor &accent)
{
    auto *page = new QWidget;
    page->setObjectName("welcomePage");
    auto *l = new QVBoxLayout(page);
    l->setContentsMargins(48, 32, 48, 20);
    l->setSpacing(20);

    auto *iconLbl = new QLabel(icon);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet(QString("font-size:52px;"));
    l->addWidget(iconLbl);

    auto *titleLbl = new QLabel(title);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(QString("font-size:20px;font-weight:bold;color:%1;").arg(accent.name()));
    l->addWidget(titleLbl);

    auto *sep = new QWidget;
    sep->setFixedHeight(2);
    sep->setStyleSheet(QString("background:rgba(%1,%2,%3,80);border-radius:1px;")
        .arg(accent.red()).arg(accent.green()).arg(accent.blue()));
    l->addWidget(sep);

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

    QString dots;
    for (int i = 0; i <= last; ++i)
        dots += (i == idx) ? "●  " : "○  ";
    m_dots->setText(dots.trimmed());
}
