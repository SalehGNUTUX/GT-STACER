#include "app.h"
#include "Widgets/sidebar.h"
#include "Widgets/sidebar_icons.h"
#include "Managers/app_manager.h"
#include "Managers/setting_manager.h"
#include "Pages/Dashboard/dashboard_page.h"
#include "Pages/Resources/resources_page.h"
#include "Pages/Processes/processes_page.h"
#include "Pages/Services/services_page.h"
#include "Pages/StartupApps/startup_apps_page.h"
#include "Pages/SystemCleaner/system_cleaner_page.h"
#include "Pages/Uninstaller/uninstaller_page.h"
#include "Pages/AptSourceManager/apt_source_page.h"
#include "Pages/Settings/settings_page.h"
#include "Pages/Helpers/helpers_page.h"

#include <QHBoxLayout>
#include <QCloseEvent>
#include <QApplication>

App::App(QWidget *parent) : QMainWindow(parent)
{
    setupUi();
    setupPages();
    setupSidebar();
    setupConnections();

    auto *s = SettingManager::instance();
    AppManager::instance()->applyTheme(s->theme());
    AppManager::instance()->applyLanguage(s->language());
    AppManager::instance()->initTray(this);

    // إذا كانت اللغة غير الإنجليزية، أعد بناء الشريط الجانبي وأجبر الصفحات على الترجمة
    if (s->language() != "en" && !s->language().isEmpty()) {
        m_sidebar->clearItems();
        setupSidebar();
        QEvent langEvent(QEvent::LanguageChange);
        for (int i = 0; i < m_pageStack->count(); ++i)
            QApplication::sendEvent(m_pageStack->widget(i), &langEvent);
    }

    navigateTo(0);

    setWindowTitle("GT-STACER");
    setWindowIcon(QIcon(":/static/icons/gt-stacer.png"));
    resize(1100, 720);
    setMinimumSize(860, 580);
}

App::~App() {}

void App::setupUi()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    auto *layout = new QHBoxLayout(m_centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_sidebar = new Sidebar(m_centralWidget);
    layout->addWidget(m_sidebar);

    // Thin separator
    auto *sep = new QWidget; sep->setFixedWidth(1);
    sep->setStyleSheet("background:#1e1e2e;");
    layout->addWidget(sep);

    m_pageStack = new QStackedWidget(m_centralWidget);
    m_pageStack->setObjectName("pageStack");
    layout->addWidget(m_pageStack, 1);
}

void App::setupPages()
{
    m_dashboard   = new DashboardPage;
    m_resources   = new ResourcesPage;
    m_processes   = new ProcessesPage;
    m_services    = new ServicesPage;
    m_startupApps = new StartupAppsPage;
    m_cleaner     = new SystemCleanerPage;
    m_uninstaller = new UninstallerPage;
    m_aptSource   = new AptSourcePage;
    m_settings    = new SettingsPage;
    m_helpers     = new HelpersPage;

    m_pageStack->addWidget(m_dashboard);
    m_pageStack->addWidget(m_resources);
    m_pageStack->addWidget(m_processes);
    m_pageStack->addWidget(m_services);
    m_pageStack->addWidget(m_startupApps);
    m_pageStack->addWidget(m_cleaner);
    m_pageStack->addWidget(m_uninstaller);
    m_pageStack->addWidget(m_aptSource);
    m_pageStack->addWidget(m_settings);
    m_pageStack->addWidget(m_helpers);
}

void App::setupSidebar()
{
    using SI = SidebarItem;
    const QVector<SI> items = {
        {SidebarIcons::dashboard(),   tr("Dashboard"),        tr("System overview")},
        {SidebarIcons::resources(),   tr("Resources"),        tr("CPU, RAM, GPU, Network")},
        {SidebarIcons::processes(),   tr("Processes"),        tr("Running processes")},
        {SidebarIcons::services(),    tr("Services"),         tr("System services")},
        {SidebarIcons::startup(),     tr("Startup Apps"),     tr("Autostart applications")},
        {SidebarIcons::cleaner(),     tr("System Cleaner"),   tr("Free up disk space")},
        {SidebarIcons::uninstaller(), tr("Uninstaller"),      tr("Remove packages")},
        {SidebarIcons::aptSources(),  tr("APT Sources"),      tr("Package repositories")},
        {SidebarIcons::settings(),    tr("Settings"),         tr("Application settings")},
        {SidebarIcons::helpers(),     tr("Helpers"),          tr("System utilities")},
    };
    for (const auto &item : items)
        m_sidebar->addItem(item);

    // نربط إشارة الصفحة مرة واحدة فقط — نتحقق قبل الربط
    disconnect(m_sidebar, &Sidebar::pageRequested, this, &App::navigateTo);
    connect(m_sidebar,    &Sidebar::pageRequested, this, &App::navigateTo);
}

void App::setupConnections()
{
    connect(m_settings, &SettingsPage::themeChanged, this, [this](const QString &t){
        SettingManager::instance()->setTheme(t);
        AppManager::instance()->applyTheme(t);
    });

    connect(m_settings, &SettingsPage::languageChanged, this, [this](const QString &lang){
        SettingManager::instance()->setLanguage(lang);
        AppManager::instance()->applyLanguage(lang);
        // مسح الأزرار القديمة ثم إعادة بناء الشريط بالنصوص المترجمة
        m_sidebar->clearItems();
        setupSidebar();
        m_sidebar->setActiveIndex(m_currentPage);
        // إعادة ترجمة الصفحات التي تستخدم .ui
        QEvent langEvent(QEvent::LanguageChange);
        for (int i = 0; i < m_pageStack->count(); ++i)
            QApplication::sendEvent(m_pageStack->widget(i), &langEvent);
    });
}

void App::navigateTo(int index)
{
    if (index < 0 || index >= m_pageStack->count()) return;
    m_sidebar->setActiveIndex(index);
    m_pageStack->setCurrentIndex(index);
    m_currentPage = index;
}

void App::closeEvent(QCloseEvent *event)
{
    if (SettingManager::instance()->minimizeToTray()
        && AppManager::instance()->trayIcon()) {
        hide();
        event->ignore();
    } else {
        // setQuitOnLastWindowClosed(false) → نحتاج استدعاء quit() صراحةً
        event->accept();
        qApp->quit();
    }
}

void App::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange
        && isMinimized()
        && SettingManager::instance()->minimizeToTray()
        && AppManager::instance()->trayIcon()) {
        hide();
    }
    QMainWindow::changeEvent(event);
}
