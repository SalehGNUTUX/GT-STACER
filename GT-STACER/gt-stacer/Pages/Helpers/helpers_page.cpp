#include "helpers_page.h"
#include "ui_helpers_page.h"
#include "../../../gt-stacer-core/Utils/file_util.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include <QMessageBox>
#include <QProcessEnvironment>

HelpersPage::HelpersPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::HelpersPage)
{
    ui->setupUi(this);
    connect(ui->loadHostsButton,        &QPushButton::clicked, this, &HelpersPage::loadHosts);
    connect(ui->saveHostsButton,        &QPushButton::clicked, this, &HelpersPage::saveHosts);
    connect(ui->dnsFlushButton,         &QPushButton::clicked, this, &HelpersPage::flushDns);
    connect(ui->swappinessApplyButton,  &QPushButton::clicked, this, &HelpersPage::applySwappiness);

    loadHosts();
    loadBootParams();
    loadLocale();
    loadSwappiness();
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
    bool ok = CommandUtil::pkexecWriteFile("/etc/hosts", content.toUtf8(),
                                            "root", "root", "0644");
    if (ok) QMessageBox::information(this, tr("Saved"), tr("/etc/hosts saved successfully."));
    else    QMessageBox::warning(this, tr("Error"),
                tr("Could not save /etc/hosts. Authorization may have been denied."));
}

// ── DNS flush ───────────────────────────────────────────────────────────
void HelpersPage::flushDns()
{
    // Try resolvers in order of likelihood. First match wins.
    struct Resolver { const char *probe; QStringList args; const char *label; };
    static const QVector<Resolver> resolvers = {
        {"systemd-resolve",   {"systemd-resolve", "--flush-caches"},                 "systemd-resolved"},
        {"resolvectl",        {"resolvectl",      "flush-caches"},                   "systemd-resolved"},
        {"nscd",              {"nscd",            "-i", "hosts"},                    "nscd"},
        {"unbound-control",   {"unbound-control", "flush_zone", "."},                "unbound"},
    };

    for (const auto &r : resolvers) {
        if (!CommandUtil::commandExists(r.probe)) continue;
        QStringList args = r.args;
        const QString prog = args.takeFirst();
        // These all need root.
        QStringList pkexecArgs = QStringList{prog} + args;
        int rc = CommandUtil::execProgram("pkexec", pkexecArgs, 15000);
        if (rc == 0) {
            QMessageBox::information(this, tr("DNS cache"),
                tr("Flushed %1 successfully.").arg(QLatin1String(r.label)));
            return;
        }
    }
    QMessageBox::warning(this, tr("DNS cache"),
        tr("No supported DNS resolver was found (systemd-resolved, nscd, unbound)."));
}

// ── Swappiness ──────────────────────────────────────────────────────────
void HelpersPage::loadSwappiness()
{
    QString val = FileUtil::readFile("/proc/sys/vm/swappiness").trimmed();
    int s = val.toInt();
    if (s < 0)   s = 60;
    if (s > 200) s = 200;
    ui->swappinessSpin->setValue(s);
}

void HelpersPage::applySwappiness()
{
    int v = ui->swappinessSpin->value();
    // sysctl uses safe argv-style invocation, no shell.
    int rc = CommandUtil::execProgram("pkexec",
        {"sysctl", "-w", QString("vm.swappiness=%1").arg(v)}, 15000);
    if (rc == 0) {
        QMessageBox::information(this, tr("Swappiness"),
            tr("vm.swappiness set to %1.\n"
               "Note: this is a runtime change. To persist across reboots, "
               "add it to /etc/sysctl.conf or /etc/sysctl.d/.").arg(v));
        loadSwappiness();
    } else {
        QMessageBox::warning(this, tr("Swappiness"),
            tr("Could not change vm.swappiness (authorization denied?)."));
    }
}

// ── /proc/cmdline ───────────────────────────────────────────────────────
void HelpersPage::loadBootParams()
{
    QString line = FileUtil::readFile("/proc/cmdline");
    if (line.isEmpty()) line = tr("(unavailable)");
    ui->bootCmdLabel->setText(line);
}

// ── Locale ──────────────────────────────────────────────────────────────
void HelpersPage::loadLocale()
{
    auto env = QProcessEnvironment::systemEnvironment();
    QString lang = env.value("LANG", env.value("LC_ALL", "C"));
    ui->localeLabel->setText(lang);
}

void HelpersPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
