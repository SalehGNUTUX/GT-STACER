#include "settings_page.h"
#include "ui_settings_page.h"
#include "../../Managers/setting_manager.h"
#include "../../Managers/app_manager.h"
#include "../../Managers/alert_manager.h"
#include "../../Managers/update_checker.h"
#include "../../Dialogs/welcome_dialog.h"
#include "../../Dialogs/whats_new_dialog.h"
#include <QClipboard>
#include <QGuiApplication>
#include "../../../gt-stacer-core/Tools/startup_tool.h"
#include "../../../gt-stacer-core/Tools/power_tool.h"
#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>

struct LangEntry {
    QString code;
    QString label;   // Native name
    QString flag;    // emoji or path
};

static const QVector<LangEntry> LANGUAGES = {
    {"en",    "English",                     "🇬🇧"},
    {"ar",    "العربية",                      "🇲🇦"},  // ar_MA - Moroccan Arabic (Western numerals, RTL)
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
// Pick an installed colour-emoji font. Flag emoji (regional-indicator pairs)
// only render as flags through such a font; KDE's default UI font draws them as
// bare letter boxes, which is why the picker looked wrong there. Cached.
QString pickEmojiFont()
{
    static const QString chosen = [] {
        const QStringList prefer = {"Noto Color Emoji", "Twemoji", "EmojiOne Color",
                                    "Segoe UI Emoji", "Apple Color Emoji"};
        const QStringList fams = QFontDatabase::families();
        for (const QString &p : prefer)
            if (fams.contains(p)) return p;
        for (const QString &f : fams)
            if (f.contains("Emoji", Qt::CaseInsensitive)) return f;
        return QString();
    }();
    return chosen;
}

// Render a flag emoji into an icon using the colour-emoji font, so it looks the
// same on every desktop. Returns a null icon when no emoji font exists, letting
// the caller fall back to text.
QIcon flagIcon(const QString &emoji)
{
    const QString font = pickEmojiFont();
    if (font.isEmpty()) return QIcon();
    const int w = 24, h = 18, scale = 2;   // 2× for crispness
    QPixmap pm(w * scale, h * scale);
    pm.setDevicePixelRatio(scale);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f(font);
    f.setPixelSize(h);
    p.setFont(f);
    p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, emoji);
    p.end();
    return QIcon(pm);
}

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

    populateLanguageCombo();

    // Set version label
    ui->versionLabel->setText(QStringLiteral("GT-STACER v%1  —  GNUTUX").arg(APP_VERSION));

    loadSettings();
    connect(ui->applyButton, &QPushButton::clicked, this, &SettingsPage::applySettings);

    // Power timer — a live in-app countdown. Keeping the timer in the app (vs.
    // an OS-scheduled job) lets the user watch and cancel it; the trade-off is
    // that GT-STACER must stay running (it can sit in the tray).
    m_powerTimer = new QTimer(this);
    m_powerTimer->setInterval(1000);
    // Keep counting down when minimized to the tray — a scheduled shutdown must
    // still fire on time instead of freezing until the window is restored.
    m_powerTimer->setProperty("keepAlive", true);
    connect(m_powerTimer, &QTimer::timeout, this, &SettingsPage::tickPowerTimer);
    ui->powerCancelButton->setEnabled(false);
    connect(ui->powerStartButton,  &QPushButton::clicked, this, &SettingsPage::startPowerTimer);
    connect(ui->powerCancelButton, &QPushButton::clicked, this, &SettingsPage::cancelPowerTimer);

    // Onboarding, updates & sharing.
    connect(ui->welcomeButton,   &QPushButton::clicked, this, &SettingsPage::showWelcomeTour);
    connect(ui->whatsNewButton,  &QPushButton::clicked, this, &SettingsPage::showWhatsNew);
    connect(ui->shareButton,     &QPushButton::clicked, this, &SettingsPage::copyShareText);
    connect(ui->checkNowButton,  &QPushButton::clicked, this, &SettingsPage::runUpdateCheck);
    connect(ui->checkUpdatesCheck, &QCheckBox::toggled, this, [](bool v){
        SettingManager::instance()->setCheckUpdatesOnStartup(v);
    });
}

void SettingsPage::showWelcomeTour()
{
    WelcomeDialog dlg(this);
    dlg.exec();
}

void SettingsPage::showWhatsNew()
{
    WhatsNewDialog dlg(this);
    dlg.exec();
}

QString SettingsPage::shareText() const
{
    // Description + version + site + hashtags — a ready-to-paste social post.
    return tr("GT-STACER — a free GNU/Linux system optimizer & monitor with a modern Qt6 interface.\n\n"
              "Monitors CPU, memory, disk, network and temperatures; manages services and startup apps; "
              "cleans the system; and creates backups & snapshots and recovers deleted files — no ads, no tracking.\n\n"
              "Version %1 stable\n"
              "https://salehgnutux.github.io/GT-STACER/\n\n"
              "#GT_STACER #GNUTUX #GNULinux #FreeSoftware #FOSS #OpenSource #Linux #Qt6").arg(APP_VERSION);
}

void SettingsPage::copyShareText()
{
    QGuiApplication::clipboard()->setText(shareText());
    ui->updateStatusLabel->setText(tr("✓ Share text copied to the clipboard — paste it anywhere."));
}

void SettingsPage::runUpdateCheck()
{
    if (!m_updateChecker) {
        m_updateChecker = new UpdateChecker(this);
        connect(m_updateChecker, &UpdateChecker::updateAvailable, this, [this](const QString &v, const QString &url){
            ui->updateStatusLabel->setText(
                tr("A newer version is available: <b>%1</b> — "
                   "<a href=\"%2\">open the release page</a>.").arg(v, url));
            ui->updateStatusLabel->setOpenExternalLinks(true);
        });
        connect(m_updateChecker, &UpdateChecker::upToDate, this, [this](const QString &v){
            ui->updateStatusLabel->setText(tr("You're on the latest version (%1).").arg(v));
        });
        connect(m_updateChecker, &UpdateChecker::checkFailed, this, [this](const QString &e){
            ui->updateStatusLabel->setText(tr("Could not check for updates: %1").arg(e));
        });
    }
    ui->updateStatusLabel->setText(tr("Checking for updates…"));
    m_updateChecker->checkNow();
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

    // Language — match by item data ("auto" or a code), not row index, since the
    // combo has an extra "Auto" entry ahead of the LANGUAGES list.
    const int langIdx = ui->languageCombo->findData(s->language());
    ui->languageCombo->setCurrentIndex(langIdx >= 0 ? langIdx : 0);

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

    ui->checkUpdatesCheck->setChecked(s->checkUpdatesOnStartup());
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

    // Language — read the selected item's data ("auto" or a concrete code).
    QString lang = ui->languageCombo->currentData().toString();
    if (lang.isEmpty()) lang = "auto";
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

void SettingsPage::populateLanguageCombo()
{
    // Preserve the current choice across a rebuild (called again on language
    // change so the "Auto" label follows the new UI language).
    const QString keep = ui->languageCombo->count()
        ? ui->languageCombo->currentData().toString()
        : SettingManager::instance()->language();

    QSignalBlocker block(ui->languageCombo);
    ui->languageCombo->clear();

    // Draw the flag as an icon (renders identically on GNOME/KDE); if no emoji
    // font is present, fall back to the emoji in text.
    ui->languageCombo->setIconSize(QSize(24, 18));
    // "Auto" first — the default; follows the system locale (English if that
    // locale has no translation). Its label is translatable, so it is refreshed
    // here on every language change; the native language names are not.
    const QIcon globe = flagIcon(QString::fromUtf8("\xF0\x9F\x8C\x90")); // 🌐
    if (globe.isNull()) ui->languageCombo->addItem(tr("Auto (system language)"), "auto");
    else                ui->languageCombo->addItem(globe, tr("Auto (system language)"), "auto");
    for (const auto &lang : LANGUAGES) {
        const QIcon ic = flagIcon(lang.flag);
        if (ic.isNull()) ui->languageCombo->addItem(lang.flag + "  " + lang.label, lang.code);
        else             ui->languageCombo->addItem(ic, lang.label, lang.code);
    }

    const int idx = ui->languageCombo->findData(keep);
    ui->languageCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void SettingsPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui) {
        ui->retranslateUi(this);
        populateLanguageCombo();   // retranslate the programmatic "Auto" item too
    }
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
