#include "app_manager.h"
#include "setting_manager.h"
#include "../Dialogs/about_dialog.h"
#include "../Dialogs/quit_confirm_dialog.h"
#include "../../gt-stacer-core/Info/cpu_info.h"
#include "../../gt-stacer-core/Info/memory_info.h"
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QProcess>
#include <QLocale>
#include <QStandardPaths>
#include <QStyleHints>
#include <QCoreApplication>
#include <QLibraryInfo>

AppManager *AppManager::m_instance = nullptr;

AppManager *AppManager::instance()
{
    if (!m_instance) m_instance = new AppManager;
    return m_instance;
}

AppManager::AppManager(QObject *parent) : QObject(parent)
{
    // Re-apply theme when the desktop colorScheme changes, but only when the
    // user has selected "auto" — otherwise we'd override their explicit choice.
    if (auto *hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this]() {
            if (SettingManager::instance()->theme() == "auto")
                applyTheme("auto");
        });
    }
}

void AppManager::applyTheme(const QString &theme)
{
    // "auto" follows the desktop preference (gsettings color-scheme on GNOME,
    // QStyleHints::colorScheme on KDE 6+). Falls back to "dark" if undetectable.
    QString effective = theme;
    if (theme == "auto") {
        effective = isDarkSystemTheme() ? "dark" : "light";
    }
    m_theme = effective;
    QString css = loadStylesheet(effective);
    if (!css.isEmpty()) qApp->setStyleSheet(css);
    emit themeChanged(effective);
}

void AppManager::applyLanguage(const QString &lang)
{
    // Remove previous translator
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    if (lang != "en" && !lang.isEmpty()) {
        m_translator = new QTranslator(this);
        QString qmFile = QString("gt-stacer_%1.qm").arg(lang);

        // Translator search order. The build-time directory comes FIRST so
        // dev binaries see freshly-compiled .qm files instead of stale ones
        // from a previous system install (which would otherwise win).
        QStringList searchDirs = {
#ifdef APP_TRANSLATIONS_BUILD_DIR
            QString(APP_TRANSLATIONS_BUILD_DIR),
#endif
            // Installed (system-wide)
            "/usr/share/gt-stacer/translations",
            "/usr/local/share/gt-stacer/translations",
            // AppImage: binary in $APPDIR/usr/bin, translations in $APPDIR/usr/share/...
            QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../share/gt-stacer/translations"),
            // Beside binary
            QCoreApplication::applicationDirPath() + "/translations",
            QCoreApplication::applicationDirPath(),
            // XDG
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/translations",
        };

        bool loaded = false;
        for (const QString &dir : searchDirs) {
            QString path = dir + "/" + qmFile;
            if (m_translator->load(path)) {
                qApp->installTranslator(m_translator);
                loaded = true;
                qInfo() << "[GT-STACER] Loaded translation:" << path;
                break;
            }
        }
        if (!loaded) {
            qWarning() << "[GT-STACER] Translation not found:" << qmFile;
            delete m_translator;
            m_translator = nullptr;
        }
    }

    // Arabic Morocco: Western numerals + RTL
    if (lang == "ar") {
        QLocale::setDefault(QLocale(QLocale::Arabic, QLocale::Morocco));
        qApp->setLayoutDirection(Qt::RightToLeft);
    } else {
        QLocale::setDefault(QLocale::system());
        qApp->setLayoutDirection(Qt::LeftToRight);
    }

    emit languageChanged();
}

void AppManager::initTray(QWidget *mainWindow)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(QIcon(":/static/icons/gt-stacer.png"), this);
    m_tray->setToolTip("GT-STACER");

    auto *menu  = new QMenu;
    auto *show  = new QAction(tr("Show"), menu);
    auto *about = new QAction(tr("About GT-STACER"), menu);
    auto *quit  = new QAction(tr("Quit"), menu);
    menu->addAction(show);
    menu->addSeparator();
    menu->addAction(about);
    menu->addAction(quit);

    connect(show,  &QAction::triggered, mainWindow, &QWidget::show);
    connect(about, &QAction::triggered, mainWindow, [mainWindow]() {
        AboutDialog dlg(mainWindow);
        dlg.exec();
    });
    // Quit asks for confirmation the first time. If the user previously ticked
    // "Don't ask again", we honour their remembered choice silently.
    connect(quit, &QAction::triggered, mainWindow, [mainWindow]() {
        if (!QuitConfirmDialog::shouldShow()) {
            if (QuitConfirmDialog::rememberedAction() == "continue") return;
            qApp->quit();
            return;
        }
        QuitConfirmDialog dlg(mainWindow);
        // Accepted = Continue (stay in tray), Rejected = Quit.
        if (dlg.exec() == QDialog::Rejected) qApp->quit();
    });
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [mainWindow](QSystemTrayIcon::ActivationReason r){
        if (r == QSystemTrayIcon::Trigger)
            mainWindow->isVisible() ? mainWindow->hide() : mainWindow->show();
    });
    m_tray->setContextMenu(menu);
    if (SettingManager::instance()->showTrayIcon()) m_tray->show();

    // تحديث Tooltip كل 3 ثوانٍ بإحصائيات النظام
    m_trayTimer = new QTimer(this);
    m_trayTimer->setInterval(3000);
    connect(m_trayTimer, &QTimer::timeout, this, &AppManager::updateTrayTooltip);
    m_trayTimer->start();
    updateTrayTooltip();
}

void AppManager::updateTrayTooltip()
{
    if (!m_tray || !m_tray->isVisible()) return;
    auto cpu = CpuInfo::usage();
    auto mem = MemoryInfo::memory();
    m_tray->setToolTip(
        QString("GT-STACER\nCPU: %1%  |  RAM: %2%")
            .arg(static_cast<int>(cpu.total))
            .arg(static_cast<int>(mem.ramPercent())));
}

void AppManager::showTray() { if (m_tray) m_tray->show(); }
void AppManager::hideTray() { if (m_tray) m_tray->hide(); }

bool AppManager::isDarkSystemTheme()
{
    // Qt 6.5+ exposes the desktop color scheme directly.
    if (auto *hints = QGuiApplication::styleHints();
        hints && hints->colorScheme() != Qt::ColorScheme::Unknown) {
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    }
    // Fallbacks for older Qt or sessions where the hint isn't populated.
    if (qEnvironmentVariable("GTK_THEME").toLower().contains("dark")) return true;
    QProcess p;
    p.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
    p.waitForFinished(2000);
    if (p.exitCode() == 0 && p.readAllStandardOutput().contains("dark")) return true;
    return qApp->palette().window().color().lightness() < 128;
}

QString AppManager::loadStylesheet(const QString &theme)
{
    QFile f(QString(":/static/themes/%1/style.qss").arg(theme));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}
