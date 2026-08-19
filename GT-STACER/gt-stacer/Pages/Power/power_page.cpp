#include "power_page.h"
#include "../../../gt-stacer-core/Tools/power_profile_tool.h"
#include "../../../gt-stacer-core/Tools/battery_tool.h"
#include "../../../gt-stacer-core/Tools/power_tool.h"
#include "../../../gt-stacer-core/Info/battery_info.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include "../../../gt-stacer-core/Tools/notification_tool.h"
#include "../../Managers/app_manager.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString profileLabel(const QString &id)
{
    if (id == "power-saver")  return PowerPage::tr("Power Saver");
    if (id == "balanced")     return PowerPage::tr("Balanced");
    if (id == "performance")  return PowerPage::tr("Performance");
    QString s = id; if (!s.isEmpty()) s[0] = s[0].toUpper();
    return s;
}
QString profileDesc(const QString &id)
{
    if (id == "power-saver")  return PowerPage::tr("Lowest power use — quieter and cooler.");
    if (id == "balanced")     return PowerPage::tr("The default trade-off between speed and power.");
    if (id == "performance")  return PowerPage::tr("Maximum performance — more power and heat.");
    return {};
}
}

PowerPage::PowerPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Power"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto *intro = new QLabel(tr(
        "Switch the system power profile, and — on laptops — cap the battery "
        "charge to preserve its long-term health."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    buildProfileSection(this);
    root->addWidget(m_profileBox);

    buildBatterySection(this);
    root->addWidget(m_batteryBox);

    buildSleepSection(this);
    if (m_sleepBox) root->addWidget(m_sleepBox);

    buildPowerTimerSection(this);
    root->addWidget(m_ptBox);

    root->addStretch();

    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &PowerPage::refreshState);

    refreshState();
}

void PowerPage::buildProfileSection(QWidget *parent)
{
    auto *box = new QGroupBox(tr("Power profile"), parent);
    box->setObjectName("reliefGroup");
    m_profileBox = box;
    auto *v = new QVBoxLayout(box);

    QStringList options;
    if (PowerProfileTool::ppdAvailable() && !PowerProfileTool::profiles().isEmpty()) {
        m_usePpd = true;
        options = PowerProfileTool::profiles();
    } else if (PowerProfileTool::cpufreqAvailable()) {
        m_usePpd = false;
        options = PowerProfileTool::governors();
    }

    if (options.isEmpty()) {
        v->addWidget(new QLabel(tr("No power-profile control is available on this system."), box));
        m_profileStatus = new QLabel(box);
        v->addWidget(m_profileStatus);
        return;
    }

    auto *row = new QHBoxLayout;
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    // Present in a friendly order for PPD; governors keep kernel order.
    const QStringList ordered = m_usePpd
        ? QStringList{"power-saver", "balanced", "performance"}
        : options;
    for (const QString &id : ordered) {
        if (m_usePpd && !options.contains(id)) continue;
        auto *b = new QPushButton(m_usePpd ? profileLabel(id) : id, box);
        b->setCheckable(true);
        b->setToolTip(m_usePpd ? profileDesc(id) : tr("CPU governor: %1").arg(id));
        b->setMinimumHeight(34);
        m_profileBtns.insert(id, b);
        group->addButton(b);
        row->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, id]{ selectProfile(id); });
    }
    row->addStretch();
    v->addLayout(row);

    m_profileStatus = new QLabel(box);
    m_profileStatus->setObjectName("infoValue");
    m_profileStatus->setWordWrap(true);
    v->addWidget(m_profileStatus);

    if (PowerProfileTool::tlpAvailable())
        v->addWidget(new QLabel(tr("TLP is installed and manages additional power settings."), box));
}

void PowerPage::buildBatterySection(QWidget *parent)
{
    m_batteryBox = new QGroupBox(tr("Battery"), parent);
    m_batteryBox->setObjectName("reliefGroup");
    auto *v = new QVBoxLayout(m_batteryBox);

    m_batteryInfo = new QLabel(m_batteryBox);
    m_batteryInfo->setObjectName("infoValue");
    v->addWidget(m_batteryInfo);

    const ChargeThresholds t = BatteryTool::thresholds();
    if (t.supported) {
        m_limitBox = new QGroupBox(tr("Charge limit (battery health)"), m_batteryBox);
        auto *lv = new QVBoxLayout(m_limitBox);
        lv->addWidget(new QLabel(tr(
            "Stop charging below 100% to slow battery wear. 80% is a common choice."),
            m_limitBox));

        auto *row = new QHBoxLayout;
        if (t.hasStart) {
            row->addWidget(new QLabel(tr("Start at"), m_limitBox));
            m_startSpin = new QSpinBox(m_limitBox);
            m_startSpin->setRange(0, 99); m_startSpin->setSuffix(tr(" %"));
            row->addWidget(m_startSpin);
        }
        row->addWidget(new QLabel(tr("Stop at"), m_limitBox));
        m_endSpin = new QSpinBox(m_limitBox);
        m_endSpin->setRange(1, 100); m_endSpin->setSuffix(tr(" %"));
        row->addWidget(m_endSpin);
        auto *apply = new QPushButton(tr("Apply"), m_limitBox);
        apply->setObjectName("primaryButton");
        connect(apply, &QPushButton::clicked, this, &PowerPage::applyChargeLimit);
        row->addWidget(apply);
        row->addStretch();
        lv->addLayout(row);

        m_limitStatus = new QLabel(m_limitBox);
        lv->addWidget(m_limitStatus);
        v->addWidget(m_limitBox);
    }
}

PowerPage::~PowerPage()
{
    // Release our keep-awake inhibitor and clear the tray badge on exit.
    m_inhibitor.unblock();
    AppManager::instance()->setKeepAwake(false);
}

void PowerPage::buildSleepSection(QWidget *parent)
{
    if (!SleepInhibitor::available()) return;

    m_sleepBox = new QGroupBox(tr("Sleep & screen locking"), parent);
    m_sleepBox->setObjectName("reliefGroup");
    auto *v = new QVBoxLayout(m_sleepBox);

    auto *row = new QHBoxLayout;
    m_sleepStatus = new QLabel(m_sleepBox);
    m_sleepStatus->setObjectName("infoValue");
    m_sleepStatus->setWordWrap(true);
    m_blockBtn = new QPushButton(m_sleepBox);
    row->addWidget(m_sleepStatus, 1);
    row->addSpacing(12);
    row->addWidget(m_blockBtn);
    v->addLayout(row);

    connect(m_blockBtn, &QPushButton::clicked, this, &PowerPage::toggleSleepBlock);
    updateSleepUi();
}

void PowerPage::updateSleepUi()
{
    if (!m_sleepBox) return;
    const bool ourBlock = m_inhibitor.blockedByUs();
    // Reflect the real system state, not just our own action — this now also
    // catches the desktop's own manual block (e.g. KDE), via HasInhibit().
    const bool sysInhibited = SleepInhibitor::systemInhibited();

    if (ourBlock)
        m_sleepStatus->setText(tr("Blocked by GT-STACER — automatic sleep and "
            "screen locking are prevented (this raises energy use)."));
    else if (sysInhibited)
        m_sleepStatus->setText(tr("Blocked by your desktop or another app — "
            "automatic sleep is currently prevented."));
    else
        m_sleepStatus->setText(tr("Automatic — the system may sleep and lock "
            "the screen when idle."));

    m_blockBtn->setText(ourBlock ? tr("Unblock") : tr("Manually block"));
    // The tray badge reflects only OUR keep-awake, so closing the window still
    // shows the user that GT-STACER is the one holding the system awake.
    AppManager::instance()->setKeepAwake(ourBlock);
}

void PowerPage::toggleSleepBlock()
{
    const bool wasBlocked = m_inhibitor.blockedByUs();
    if (wasBlocked) {
        m_inhibitor.unblock();
    } else {
        m_inhibitor.block();
        if (!m_inhibitor.blockedByUs())
            QMessageBox::warning(this, tr("Power"), tr("Could not block automatic sleep."));
    }

    // Notify the desktop when the state actually changed.
    const bool nowBlocked = m_inhibitor.blockedByUs();
    if (nowBlocked != wasBlocked)
        NotificationTool::notify(tr("Keep awake"),
            nowBlocked ? tr("Automatic sleep and screen locking are now blocked.")
                       : tr("Automatic sleep and screen locking are allowed again."),
            NotificationTool::Urgency::Normal, "gt-stacer");

    updateSleepUi();
}

void PowerPage::selectProfile(const QString &id)
{
    const bool ok = m_usePpd ? PowerProfileTool::setProfile(id)
                             : PowerProfileTool::setGovernor(id);
    if (!ok)
        QMessageBox::warning(this, tr("Power"),
            tr("Could not change the power profile. It may require authorization "
               "that was declined."));
    refreshState();
}

void PowerPage::refreshState()
{
    // Profile / governor active state.
    const QString active = m_usePpd ? PowerProfileTool::activeProfile()
                                    : PowerProfileTool::activeGovernor();
    for (auto it = m_profileBtns.constBegin(); it != m_profileBtns.constEnd(); ++it)
        it.value()->setChecked(it.key() == active);
    if (m_profileStatus) {
        if (active.isEmpty())
            m_profileStatus->clear();
        else
            m_profileStatus->setText(m_usePpd
                ? tr("Active profile: %1").arg(profileLabel(active))
                : tr("Active CPU governor: %1").arg(active));
    }

    // Battery — hide the whole section on machines without one.
    const bool hasBat = BatteryInfo::hasBattery();
    m_batteryBox->setVisible(hasBat);
    if (hasBat) {
        const BatteryData b = BatteryInfo::primaryBattery();
        QString line = tr("Charge: %1%  ·  %2").arg(b.percent).arg(b.statusString());
        if (b.timeRemainingMin && *b.timeRemainingMin > 0)
            line += tr("  ·  %1h %2m remaining")
                        .arg(*b.timeRemainingMin / 60).arg(*b.timeRemainingMin % 60);
        line += BatteryInfo::isOnAC() ? tr("  ·  on AC power") : tr("  ·  on battery");
        m_batteryInfo->setText(line);

        if (m_endSpin) {
            const ChargeThresholds t = BatteryTool::thresholds();
            // Don't stomp a value the user is editing; only seed once.
            if (!m_endSpin->hasFocus()) m_endSpin->setValue(t.end);
            if (m_startSpin && !m_startSpin->hasFocus() && t.start >= 0)
                m_startSpin->setValue(t.start);
        }
    }

    updateSleepUi();   // keep the sleep/inhibitor readout live with the system
}

void PowerPage::applyChargeLimit()
{
    const int end   = m_endSpin ? m_endSpin->value() : 100;
    const int start = m_startSpin ? m_startSpin->value() : -1;
    if (BatteryTool::setThresholds(start, end)) {
        m_limitStatus->setText(tr("Charge limit applied."));
    } else {
        m_limitStatus->setText(tr("Could not apply the charge limit (authorization declined?)."));
    }
    refreshState();
}

// ── Power timer ───────────────────────────────────────────────────────────────
// A mirror of the Settings power timer, so a scheduled shutdown/suspend can be
// set right here on the Power page. The countdown lives in-app (so it can be
// watched and cancelled) and keeps ticking when minimised to the tray.
void PowerPage::buildPowerTimerSection(QWidget *parent)
{
    m_ptBox = new QGroupBox(tr("Power timer"), parent);
    m_ptBox->setObjectName("reliefGroup");
    auto *v = new QVBoxLayout(m_ptBox);

    auto *hint = new QLabel(tr("Schedule an automatic power action after a set time. "
                               "Requires administrator authorization."), m_ptBox);
    hint->setWordWrap(true);
    hint->setObjectName("introText");
    v->addWidget(hint);

    auto *row = new QHBoxLayout;
    m_ptActionCombo = new QComboBox(m_ptBox);
    m_ptActionCombo->addItem(tr("Shut down"));
    m_ptActionCombo->addItem(tr("Restart"));
    m_ptActionCombo->addItem(tr("Suspend (to RAM)"));
    m_ptActionCombo->addItem(tr("Hibernate (to disk)"));
    m_ptMinutesSpin = new QSpinBox(m_ptBox);
    m_ptMinutesSpin->setRange(1, 1440);
    m_ptMinutesSpin->setValue(30);
    m_ptMinutesSpin->setSuffix(tr(" min"));
    m_ptStartBtn  = new QPushButton(tr("Start timer"), m_ptBox);
    m_ptStartBtn->setObjectName("primaryButton");
    m_ptCancelBtn = new QPushButton(tr("Cancel"), m_ptBox);
    m_ptCancelBtn->setObjectName("dangerButton");
    m_ptCancelBtn->setEnabled(false);
    row->addWidget(m_ptActionCombo);
    row->addWidget(new QLabel(tr("after"), m_ptBox));
    row->addWidget(m_ptMinutesSpin);
    row->addWidget(m_ptStartBtn);
    row->addWidget(m_ptCancelBtn);
    row->addStretch();
    v->addLayout(row);

    m_ptCountdown = new QLabel(m_ptBox);
    m_ptCountdown->setStyleSheet("color:#f9e2af;font-weight:bold;");
    v->addWidget(m_ptCountdown);

    m_ptTimer = new QTimer(this);
    m_ptTimer->setInterval(1000);
    // Keep counting while minimised so a scheduled action still fires on time.
    m_ptTimer->setProperty("keepAlive", true);
    connect(m_ptTimer, &QTimer::timeout, this, &PowerPage::tickPowerTimer);
    connect(m_ptStartBtn,  &QPushButton::clicked, this, &PowerPage::startPowerTimer);
    connect(m_ptCancelBtn, &QPushButton::clicked, this, &PowerPage::cancelPowerTimer);
}

void PowerPage::startPowerTimer()
{
    const int action = m_ptActionCombo->currentIndex();
    if (!PowerTool::isAvailable(static_cast<PowerTool::Action>(action))) {
        QMessageBox::warning(this, tr("Unavailable"),
            tr("This power mode is not supported on this system "
               "(hibernate needs a swap area at least as large as your RAM)."));
        return;
    }
    const QString label = m_ptActionCombo->currentText();
    const int mins = m_ptMinutesSpin->value();
    if (QMessageBox::question(this, tr("Schedule power action"),
            tr("Schedule <b>%1</b> in <b>%2 minutes</b>?<br><br>"
               "You may be asked to authorize the action when the timer fires.")
                .arg(label).arg(mins)) != QMessageBox::Yes)
        return;

    m_ptAction    = action;
    m_ptRemaining = mins * 60;
    m_ptTimer->start();
    m_ptStartBtn->setEnabled(false);
    m_ptActionCombo->setEnabled(false);
    m_ptMinutesSpin->setEnabled(false);
    m_ptCancelBtn->setEnabled(true);
    tickPowerTimer();   // paint the initial countdown immediately
}

void PowerPage::cancelPowerTimer()
{
    m_ptTimer->stop();
    m_ptRemaining = 0;
    m_ptCountdown->setText(tr("Timer cancelled."));
    m_ptStartBtn->setEnabled(true);
    m_ptActionCombo->setEnabled(true);
    m_ptMinutesSpin->setEnabled(true);
    m_ptCancelBtn->setEnabled(false);
}

void PowerPage::tickPowerTimer()
{
    if (m_ptRemaining <= 0) {
        m_ptTimer->stop();
        m_ptCountdown->setText(tr("Running the scheduled action now…"));
        PowerTool::perform(static_cast<PowerTool::Action>(m_ptAction));
        cancelPowerTimer();
        m_ptCountdown->setText(QString());
        return;
    }
    const int h = m_ptRemaining / 3600;
    const int m = (m_ptRemaining % 3600) / 60;
    const int s = m_ptRemaining % 60;
    m_ptCountdown->setText(tr("%1 in %2").arg(m_ptActionCombo->currentText(),
                           QString::asprintf("%02d:%02d:%02d", h, m, s)));
    --m_ptRemaining;
}

void PowerPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
}

void PowerPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshState();
    m_timer->start();
}

void PowerPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();
}
