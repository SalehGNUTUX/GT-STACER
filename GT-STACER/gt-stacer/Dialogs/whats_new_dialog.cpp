#include "whats_new_dialog.h"
#include "../Managers/language_util.h"
#include "../Managers/app_manager.h"
#include "../Managers/setting_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QApplication>

namespace {
// Highlights of the CURRENT release. Update this list every release (kept short —
// the full history lives in CHANGELOG.md, linked below).
QStringList highlights()
{
    return {
        WhatsNewDialog::tr("💿 Disk images — mount ISO and disk images (hybrid ISOs too), extract them, and copy a CD/DVD to an ISO file or burn an ISO onto a disc, with verification."),
        WhatsNewDialog::tr("🔌 USB drives — check, repair and format USB sticks and memory cards, read their health, scan the surface for bad blocks, and back them up to an image."),
        WhatsNewDialog::tr("🥾 Boot (GRUB) — change the boot menu settings and theme safely: every change is checked before it is applied, backed up, and can be restored."),
        WhatsNewDialog::tr("⚡ Smarter System Relief — the heaviest app you are working with keeps the machine; the others are paused in stages and resumed when the pressure drops."),
        WhatsNewDialog::tr("📦 Upgrades from every source — APT, Flatpak, Snap and more in one list; untick whole sources or single packages before upgrading. Install package files you downloaded, with their dependencies."),
        WhatsNewDialog::tr("🩺 Clear explanations — a failed command now says why (password dismissed, missing tool, busy lock …), and a crash is recorded and reported at the next start."),
        WhatsNewDialog::tr("✨ One animated waiting card everywhere, naming the task and its current step; Arabic text mixed with commands and paths is no longer scrambled."),
    };
}
}

WhatsNewDialog::WhatsNewDialog(QWidget *parent) : QDialog(parent)
{
    setMinimumWidth(540);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 16, 24, 20);
    root->setSpacing(12);

    // Language picker.
    auto *langBar = new QHBoxLayout;
    m_lang = new QComboBox;
    m_lang->setMinimumWidth(180);
    langBar->addWidget(new QLabel("🌐"));
    langBar->addWidget(m_lang);
    langBar->addStretch();
    root->addLayout(langBar);

    m_title = new QLabel;
    m_title->setObjectName("pageTitle");
    root->addWidget(m_title);
    m_sub = new QLabel;
    m_sub->setObjectName("introText");
    m_sub->setWordWrap(true);
    root->addWidget(m_sub);

    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *host = new QWidget;
    m_listLay = new QVBoxLayout(host);
    m_listLay->setContentsMargins(0, 0, 0, 0);
    m_listLay->setSpacing(10);
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    auto *btnRow = new QHBoxLayout;
    m_link = new QLabel;
    m_link->setOpenExternalLinks(true);
    m_ok = new QPushButton;
    m_ok->setObjectName("primaryButton");
    connect(m_ok, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(m_link);
    btnRow->addStretch();
    btnRow->addWidget(m_ok);
    root->addLayout(btnRow);

    LangUtil::fillCombo(m_lang, tr("Auto (system language)"));
    const int cur = m_lang->findData(SettingManager::instance()->language());
    m_lang->setCurrentIndex(cur >= 0 ? cur : 0);
    connect(m_lang, QOverload<int>::of(&QComboBox::activated), this, [this](int){
        AppManager::instance()->changeLanguage(m_lang->currentData().toString());
        retranslate();
    });

    retranslate();
}

void WhatsNewDialog::retranslate()
{
    setWindowTitle(tr("What's new"));
    setLayoutDirection(qApp->layoutDirection());
    m_title->setText(tr("What's new in GT-STACER %1").arg(APP_VERSION));
    m_sub->setText(tr("Thanks for updating. Here's what changed in this release:"));
    m_link->setText(tr("<a href=\"https://github.com/SalehGNUTUX/GT-STACER/blob/main/CHANGELOG.md\">"
                       "View full changelog</a>"));
    m_ok->setText(tr("Got it"));

    LangUtil::fillCombo(m_lang, tr("Auto (system language)"));
    const int cur = m_lang->findData(SettingManager::instance()->language());
    m_lang->setCurrentIndex(cur >= 0 ? cur : 0);

    // Rebuild the highlight rows in the new language.
    while (QLayoutItem *it = m_listLay->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    for (const QString &h : highlights()) {
        auto *l = new QLabel(h);
        l->setWordWrap(true);
        m_listLay->addWidget(l);
    }
    m_listLay->addStretch();
}
