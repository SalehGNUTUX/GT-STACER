#include "helpers_page.h"
#include "disk_image_tab.h"
#include "removable_tab.h"
#include "boot_tab.h"
#include "../../Widgets/failure_text.h"
#include "ui_helpers_page.h"
#include "../../../gt-stacer-core/Utils/file_util.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include <QMessageBox>
#include <QTabWidget>
#include <QProcessEnvironment>

HelpersPage::HelpersPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::HelpersPage)
{
    ui->setupUi(this);
    // 26.12: Helpers is tabbed — the user's tools are rewritten natively here
    // (Disk images = GT-IMT; USB/SD = gt-usdr and Bootloader = GT-GMT follow).
    m_diskImageTab = ui->helpersTabs->addTab(new DiskImageTab(ui->helpersTabs), QString());
    m_removableTab = ui->helpersTabs->addTab(new RemovableTab(ui->helpersTabs), QString());
    m_bootTab      = ui->helpersTabs->addTab(new BootTab(ui->helpersTabs), QString());
    retranslateTabs();
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
    CommandUtil::clearLastResult();
    bool ok = CommandUtil::pkexecWriteFile("/etc/hosts", content.toUtf8(),
                                            "root", "root", "0644");
    if (ok) QMessageBox::information(this, tr("Saved"), tr("/etc/hosts saved successfully."));
    else    FailureText::show(this, tr("Error"), tr("/etc/hosts was not saved."),
                              CommandUtil::lastResult());
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
        FailureText::show(this, tr("Swappiness"), tr("vm.swappiness was not changed."),
                          CommandUtil::lastResult());
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

// Short, clear tab names; the details are in each tab's tooltip.
void HelpersPage::retranslateTabs()
{
    QTabWidget *t = ui->helpersTabs;
    t->setTabText(0, tr("System"));
    t->setTabToolTip(0, tr("hosts file, DNS cache, swappiness, kernel boot parameters, locale"));
    t->setTabText(m_diskImageTab, tr("Disk images"));
    t->setTabToolTip(m_diskImageTab, tr("Mount, browse and extract ISO and IMG disk images"));
    t->setTabText(m_removableTab, tr("USB drives"));
    t->setTabToolTip(m_removableTab, tr("Check, repair, format and back up USB sticks and memory cards"));
    t->setTabText(m_bootTab, tr("Boot"));
    t->setTabToolTip(m_bootTab, tr("Boot menu (GRUB): settings, restore points and repair"));
}

void HelpersPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui) {
        ui->retranslateUi(this);
        retranslateTabs();
    }
    QWidget::changeEvent(event);
}
