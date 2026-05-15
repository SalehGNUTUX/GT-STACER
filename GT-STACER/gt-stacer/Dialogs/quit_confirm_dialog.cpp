#include "quit_confirm_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSettings>

namespace {
QSettings &cfg()
{
    static QSettings s("GNUTUX", "GT-STACER");
    return s;
}
}

QuitConfirmDialog::QuitConfirmDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Quit — GT-STACER"));
    setObjectName("quitConfirmDialog");
    setMinimumWidth(400);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(14);

    auto *msg = new QLabel(tr("Will the program continue to work in the system tray?"));
    msg->setWordWrap(true);
    msg->setStyleSheet("font-size:13px;color:#cdd6f4;");
    root->addWidget(msg);

    m_dontAsk = new QCheckBox(tr("Don't ask again"));
    m_dontAsk->setStyleSheet("color:#a6adc8;");
    root->addWidget(m_dontAsk, 0, Qt::AlignRight);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addStretch();

    auto *contBtn = new QPushButton(tr("Continue"));
    contBtn->setObjectName("primaryButton");
    contBtn->setMinimumWidth(110);
    contBtn->setDefault(true);
    connect(contBtn, &QPushButton::clicked, this, &QuitConfirmDialog::onContinue);
    btnRow->addWidget(contBtn);

    auto *quitBtn = new QPushButton(tr("Quit"));
    quitBtn->setObjectName("dangerButton");
    quitBtn->setMinimumWidth(110);
    connect(quitBtn, &QPushButton::clicked, this, &QuitConfirmDialog::onQuit);
    btnRow->addWidget(quitBtn);

    root->addLayout(btnRow);
}

void QuitConfirmDialog::onContinue()
{
    if (m_dontAsk->isChecked()) {
        cfg().setValue("quitConfirm/dontAsk", true);
        cfg().setValue("quitConfirm/action", "continue");
    }
    accept();
}

void QuitConfirmDialog::onQuit()
{
    if (m_dontAsk->isChecked()) {
        cfg().setValue("quitConfirm/dontAsk", true);
        cfg().setValue("quitConfirm/action", "quit");
    }
    reject();
}

bool QuitConfirmDialog::shouldShow()
{
    return !cfg().value("quitConfirm/dontAsk", false).toBool();
}

QString QuitConfirmDialog::rememberedAction()
{
    return cfg().value("quitConfirm/action", "quit").toString();
}
