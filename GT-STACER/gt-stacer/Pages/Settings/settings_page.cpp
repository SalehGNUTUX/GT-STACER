#include "settings_page.h"
#include "ui_settings_page.h"
#include "../../Managers/setting_manager.h"
#include "../../Managers/app_manager.h"

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

    ui->startMinimizedCheck->setChecked(s->startMinimized());
    ui->showTrayCheck->setChecked(s->showTrayIcon());
    ui->minimizeToTrayCheck->setChecked(s->minimizeToTray());
    ui->updateIntervalSpin->setValue(s->updateIntervalMs() / 1000);
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

    s->setStartMinimized(ui->startMinimizedCheck->isChecked());
    s->setShowTrayIcon(ui->showTrayCheck->isChecked());
    s->setMinimizeToTray(ui->minimizeToTrayCheck->isChecked());
    s->setUpdateIntervalMs(ui->updateIntervalSpin->value() * 1000);

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
