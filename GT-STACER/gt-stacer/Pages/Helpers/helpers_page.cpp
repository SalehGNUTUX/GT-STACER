#include "helpers_page.h"
#include "ui_helpers_page.h"
#include "../../../gt-stacer-core/Utils/file_util.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include <QMessageBox>

HelpersPage::HelpersPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::HelpersPage)
{
    ui->setupUi(this);
    connect(ui->loadHostsButton, &QPushButton::clicked, this, &HelpersPage::loadHosts);
    connect(ui->saveHostsButton, &QPushButton::clicked, this, &HelpersPage::saveHosts);
    loadHosts();
}

HelpersPage::~HelpersPage() { delete ui; }

void HelpersPage::loadHosts()
{
    QString content = FileUtil::readFile("/etc/hosts");
    ui->hostsEditor->setPlainText(content);
}

void HelpersPage::saveHosts()
{
    QString content = ui->hostsEditor->toPlainText();
    // Write via pkexec
    bool ok = CommandUtil::execStatus(
        QString("pkexec sh -c 'echo \"%1\" > /etc/hosts'").arg(content)) == 0;
    if (ok) QMessageBox::information(this, tr("Saved"), tr("/etc/hosts saved successfully."));
    else    QMessageBox::warning(this, tr("Error"), tr("Could not save /etc/hosts."));
}

void HelpersPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
