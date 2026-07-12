#include "settings_page.h"
#include "ui_settings_page.h"
#include "../../Managers/setting_manager.h"
#include "../../Managers/app_manager.h"
#include "../../Managers/alert_manager.h"
#include "../../../gt-stacer-core/Tools/startup_tool.h"
#include "../../../gt-stacer-core/Tools/power_tool.h"
#include <QCoreApplication>
#include <QFile>
#include <QMessageBox>
#include <QShowEvent>
#include <QTimer>

struct LangEntry {
    QString code;
    QString label;   // Native name
    QString flag;    // emoji or path
};

static const QVector<LangEntry> LANGUAGES = {
    {"en",    "English",                     "🇬🇧"},
    {"ar",    "العربية (المغرب)",             "🇲🇦"},  // ar_MA - Moroccan Arabic
    {"de",    "Deutsch",                     "🇩🇪"},
    {"fr",    "Français",                    "🇫🇷"},
    {"hi",    "हिन्दी",                      "🇮🇳"},
    {"it",    "Italiano",                    "🇮🇹"},
    {"kn",    "ಕನ್ನಡ",                      "🇮🇳"},
    {"ml",    "മലയാളം",                      "🇮🇳"},
    {"nl",    "Nederlands",                  "🇳🇱"},
    {"oc",    "Occitan",                     "🏳️"},
    {"pl",    "Polski",                      "🇵🇱"},
    {"pt",    "Português",                   "🇧🇷"},
    {"ru",    "Русский",                     "🇷🇺"},
    {"sv",    "Svenska",                     "🇸🇪"},
    {"tr",    "Türkçe",                      "🇹🇷"},
    {"uk",    "Українська",                  "🇺🇦"},
    {"vi",    "Tiếng Việt",                  "🇻🇳"},
    {"zh_CN", "简体中文",                     "🇨🇳"},
    {"zh_TW", "繁體中文",                     "🇹🇼"},
};

namespace {
// The autostart .desktop we manage for GT-STACER itself.
QString selfAutostartPath()
{
    return StartupTool::autostartDir() + "/GT-STACER.desktop";
}

// Command that survives a reboot: for an AppImage the mount path is ephemeral,
// so prefer $APPIMAGE; otherwise the real binary path (usually /usr/bin/gt-stacer).
QString selfExec()
{
    QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty()) return appImage;
    return QCoreApplication::applicationFilePath();
}

bool selfAutostartActive()
{
    for (const auto &e : StartupTool::entries())
        if (e.filePath == selfAutostartPath())
            return e.enabled;
    return false;
}
} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::SettingsPage)
{
    ui->setupUi(this);

    // Fill language ComboBox
    for (const auto &lang : LANGUAGES)
        ui->languageCombo->addItem(lang.flag + "  " + lang.label, lang.code);

    // Set version label
    ui->versionLabel->setText(APP_VERSION);

    loadSettings();
    connect(ui->applyButton, &QPushButton::clicked, this, &SettingsPage::applySettings);

    // Power timer — a live in-app countdown. Keeping the timer in the app (vs.
    // an OS-scheduled job) lets the user watch and cancel it; the trade-off is
    // that GT-STACER must stay running (it can sit in the tray).
    m_powerTimer = new QTimer(this);
    m_powerTimer->setInterval(1000);
    connect(m_powerTimer, &QTimer::timeout, this, &SettingsPage::tickPowerTimer);
    ui->powerCancelButton->setEnabled(false);
    connect(ui->powerStartButton,  &QPushButton::clicked, this, &SettingsPage::startPowerTimer);
    connect(ui->powerCancelButton, &QPushButton::clicked, this, &SettingsPage::cancelPowerTimer);
}

void SettingsPage::startPowerTimer()
{
    const int action = ui->powerActionCombo->currentIndex();
    if (!PowerTool::isAvailable(static_cast<PowerTool::Action>(action))) {
        QMessageBox::warning(this, tr("Unavailable"),
            tr("This power mode is not supported on this system "
               "(hibernate needs a swap area at least as large as your RAM)."));
        return;
    }

    // Guard against an accidental shutdown scheduled with a fat-finger.
    const QString label = ui->powerActionCombo->currentText();
    const int mins = ui->powerMinutesSpin->value();
    if (QMessageBox::question(this, tr("Schedule power action"),
            tr("Schedule <b>%1</b> in <b>%2 minutes</b>?<br><br>"
               "You may be asked to authorize the action when the timer fires.")
                .arg(label).arg(mins)) != QMessageBox::Yes)
        return;

    m_powerAction    = action;
    m_powerRemaining = mins * 60;
    m_powerTimer->start();
    ui->powerStartButton->setEnabled(false);
    ui->powerActionCombo->setEnabled(false);
    ui->powerMinutesSpin->setEnabled(false);
    ui->powerCancelButton->setEnabled(true);
    tickPowerTimer();   // paint the initial countdown immediately
}

void SettingsPage::cancelPowerTimer()
{
    m_powerTimer->stop();
    m_powerRemaining = 0;
    ui->powerCountdownLabel->setText(tr("Timer cancelled."));
    ui->powerStartButton->setEnabled(true);
    ui->powerActionCombo->setEnabled(true);
    ui->powerMinutesSpin->setEnabled(true);
    ui->powerCancelButton->setEnabled(false);
}

void SettingsPage::tickPowerTimer()
{
    if (m_powerRemaining <= 0) {
        m_powerTimer->stop();
        ui->powerCountdownLabel->setText(tr("Running the scheduled action now…"));
        PowerTool::perform(static_cast<PowerTool::Action>(m_powerAction));
        // If the action was refused/unavailable, re-enable the controls so the
        // user isn't stuck with a dead timer.
        cancelPowerTimer();
        ui->powerCountdownLabel->setText(QString());
        return;
    }
    const int h = m_powerRemaining / 3600;
    const int m = (m_powerRemaining % 3600) / 60;
    const int s = m_powerRemaining % 60;
    ui->powerCountdownLabel->setText(
        tr("%1 in %2")
            .arg(ui->powerActionCombo->currentText(),
                 QString::asprintf("%02d:%02d:%02d", h, m, s)));
    --m_powerRemaining;
}

SettingsPage::~SettingsPage() { delete ui; }

void SettingsPage::loadSettings()
{
    auto *s = SettingManager::instance();

    // Theme
    if      (s->theme() == "dark")  ui->themeCombo->setCurrentIndex(0);
    else if (s->theme() == "light") ui->themeCombo->setCurrentIndex(1);
    else                            ui->themeCombo->setCurrentIndex(2); // auto

    // Language
    QString currentLang = s->language();
    for (int i = 0; i < LANGUAGES.size(); ++i) {
        if (LANGUAGES[i].code == currentLang) {
            ui->languageCombo->setCurrentIndex(i);
            break;
        }
    }

    ui->autoStartCheck->setChecked(selfAutostartActive());
    ui->startMinimizedCheck->setChecked(s->startMinimized());
    ui->showTrayCheck->setChecked(s->showTrayIcon());
    ui->minimizeToTrayCheck->setChecked(s->minimizeToTray());
    ui->updateIntervalSpin->setValue(s->updateIntervalMs() / 1000);

    // Alerts
    ui->alertsEnabledCheck->setChecked(s->alertsEnabled());
    ui->cpuTempSpin->setValue(s->cpuTempThresholdC());
    ui->memSpin->setValue(s->memThresholdPercent());
    ui->diskSpin->setValue(s->diskThresholdPercent());
    ui->batterySpin->setValue(s->batteryThresholdPercent());
}

void SettingsPage::applySettings()
{
    auto *s = SettingManager::instance();

    // Theme — index 0=dark, 1=light, 2=auto.
    QString theme;
    switch (ui->themeCombo->currentIndex()) {
    case 1:  theme = "light"; break;
    case 2:  theme = "auto";  break;
    default: theme = "dark";  break;
    }
    s->setTheme(theme);

    // Language
    int li = ui->languageCombo->currentIndex();
    QString lang = (li >= 0 && li < LANGUAGES.size()) ? LANGUAGES[li].code : "en";
    bool langChanged = (lang != s->language());
    s->setLanguage(lang);

    // Autostart on login — reflected by the presence of our managed .desktop.
    const bool wantAutostart = ui->autoStartCheck->isChecked();
    if (wantAutostart != selfAutostartActive()) {
        if (wantAutostart) {
            StartupEntry e;
            e.name    = "GT-STACER";
            e.exec    = selfExec();
            e.comment = tr("GNU/Linux system optimizer & monitor");
            e.icon    = "gt-stacer";
            e.enabled = true;
            StartupTool::add(e);
        } else {
            QFile::remove(selfAutostartPath());
        }
    }

    s->setStartMinimized(ui->startMinimizedCheck->isChecked());
    s->setShowTrayIcon(ui->showTrayCheck->isChecked());
    s->setMinimizeToTray(ui->minimizeToTrayCheck->isChecked());
    s->setUpdateIntervalMs(ui->updateIntervalSpin->value() * 1000);

    // Alerts
    s->setAlertsEnabled(ui->alertsEnabledCheck->isChecked());
    s->setCpuTempThresholdC(ui->cpuTempSpin->value());
    s->setMemThresholdPercent(ui->memSpin->value());
    s->setDiskThresholdPercent(ui->diskSpin->value());
    s->setBatteryThresholdPercent(ui->batterySpin->value());
    AlertManager::instance()->setEnabled(ui->alertsEnabledCheck->isChecked());

    emit themeChanged(theme);

    if (langChanged)
        emit languageChanged(lang);

    if (s->showTrayIcon()) AppManager::instance()->showTray();
    else AppManager::instance()->hideTray();
}

void SettingsPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}

void SettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // The "start on login" state lives in a .desktop file that can change while
    // this page is alive (added/removed from the Startup page). Re-sync the
    // checkbox each time the page is shown so it always reflects reality.
    if (ui) ui->autoStartCheck->setChecked(selfAutostartActive());
}
