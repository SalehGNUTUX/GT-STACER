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
        WhatsNewDialog::tr("💽 Disk-aware System Relief — see disk pressure (PSI) and each process's I/O, ease a busy disk with one click (ionice), and switch to the BFQ scheduler for a responsive old drive."),
        WhatsNewDialog::tr("⏻ The Power timer (scheduled shutdown / suspend / hibernate) is now on the Power page too, not only in Settings."),
        WhatsNewDialog::tr("🎬 A re-openable welcome tour and a \"what's new\" dialog after each update — both with an in-dialog language picker."),
        WhatsNewDialog::tr("🔔 Optional update check that tells you when a newer release is out, plus a one-click \"copy share text\" button."),
        WhatsNewDialog::tr("🧭 Services now sits under Processes in the sidebar; assorted fixes."),
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
