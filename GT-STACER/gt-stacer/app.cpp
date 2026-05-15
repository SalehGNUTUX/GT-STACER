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
#include "Dialogs/about_dialog.h"

#include <QHBoxLayout>
#include <QCloseEvent>
#include <QApplication>
#include <QShortcut>
#include <QKeySequence>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

static constexpr int kNumPages = 10;

App::App(QWidget *parent) : QMainWindow(parent)
{
    auto *s = SettingManager::instance();
    // Apply language BEFORE building any widgets so tr() resolves to the
    // correct strings on first construction (avoids the previous workaround of
    // calling setupSidebar() twice and broadcasting LanguageChange events).
    AppManager::instance()->applyLanguage(s->language());
    AppManager::instance()->applyTheme(s->theme());

    setupUi();
    setupPages();
    setupSidebar();
    AppManager::instance()->initTray(this);

    // ── Keyboard shortcuts ─────────────────────────────────────────────
    // Ctrl+1..9,0 jump to the corresponding sidebar page (0 = page 10, Helpers).
    for (int i = 0; i < kNumPages; ++i) {
        QKeySequence keys(QString("Ctrl+%1").arg((i + 1) % 10));
        auto *sc = new QShortcut(keys, this);
        connect(sc, &QShortcut::activated, this, [this, i]{ navigateTo(i); });
    }
    // Ctrl+R: jump to dashboard (acts as a quick refresh by re-entering).
    {
        auto *sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
        connect(sc, &QShortcut::activated, this, [this]{ navigateTo(m_currentPage); });
    }
    // Ctrl+Q: quit unconditionally (bypasses minimize-to-tray).
    {
        auto *sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this);
        connect(sc, &QShortcut::activated, qApp, &QApplication::quit);
    }
    // Ctrl+, : open Settings (matches GNOME/macOS convention).
    {
        auto *sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
        connect(sc, &QShortcut::activated, this, [this]{ navigateTo(8); });
    }
    // F1: About dialog.
    {
        auto *sc = new QShortcut(QKeySequence(Qt::Key_F1), this);
        connect(sc, &QShortcut::activated, this, [this]{
            AboutDialog dlg(this);
            dlg.exec();
        });
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
    m_pages.fill(nullptr, kNumPages);
    m_placeholders.resize(kNumPages);
    for (int i = 0; i < kNumPages; ++i) {
        auto *ph = new QWidget; // empty placeholder
        m_placeholders[i] = ph;
        m_pageStack->addWidget(ph);
    }
    // Dashboard is the first view — build it now so the user doesn't see a
    // blank pane between startup and the first frame.
    materializePage(0);
}

QWidget *App::materializePage(int index)
{
    if (index < 0 || index >= kNumPages) return nullptr;
    if (m_pages[index]) return m_pages[index];

    QWidget *page = nullptr;
    switch (index) {
    case 0: page = new DashboardPage;     break;
    case 1: page = new ResourcesPage;     break;
    case 2: page = new ProcessesPage;     break;
    case 3: page = new ServicesPage;      break;
    case 4: page = new StartupAppsPage;   break;
    case 5: page = new SystemCleanerPage; break;
    case 6: page = new UninstallerPage;   break;
    case 7: page = new AptSourcePage;     break;
    case 8: page = new SettingsPage;      break;
    case 9: page = new HelpersPage;       break;
    default: return nullptr;
    }

    // Replace placeholder at the same stack index.
    QWidget *ph = m_placeholders[index];
    m_pageStack->insertWidget(index, page);
    m_pageStack->removeWidget(ph);
    ph->deleteLater();
    m_placeholders[index] = nullptr;
    m_pages[index] = page;

    if (index == 8) setupSettingsConnections();
    return page;
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

    disconnect(m_sidebar, &Sidebar::pageRequested, this, &App::navigateTo);
    connect(m_sidebar,    &Sidebar::pageRequested, this, &App::navigateTo);
}

void App::setupSettingsConnections()
{
    auto *settings = qobject_cast<SettingsPage*>(m_pages[8]);
    if (!settings) return;

    connect(settings, &SettingsPage::themeChanged, this, [](const QString &t){
        SettingManager::instance()->setTheme(t);
        AppManager::instance()->applyTheme(t);
    });

    connect(settings, &SettingsPage::languageChanged, this, [this](const QString &lang){
        SettingManager::instance()->setLanguage(lang);
        AppManager::instance()->applyLanguage(lang);
        // Rebuild sidebar with translated strings.
        m_sidebar->clearItems();
        setupSidebar();
        m_sidebar->setActiveIndex(m_currentPage);
        // Retranslate every page that has actually been materialized.
        QEvent langEvent(QEvent::LanguageChange);
        for (QWidget *page : m_pages)
            if (page) QApplication::sendEvent(page, &langEvent);
    });
}

void App::navigateTo(int index)
{
    if (index < 0 || index >= kNumPages) return;
    materializePage(index);
    m_sidebar->setActiveIndex(index);

    QWidget *page = m_pageStack->widget(index);
    m_pageStack->setCurrentIndex(index);
    m_currentPage = index;

    // Brief fade-in (150 ms) on every navigation. Cheap visual cue that the
    // page actually changed, and helps mask the lazy-construction work of
    // first-time-visited pages. We attach a fresh QGraphicsOpacityEffect so a
    // dangling animation from a previous nav can't outlive its target.
    if (!page) return;
    auto *effect = new QGraphicsOpacityEffect(page);
    effect->setOpacity(0.0);
    page->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", page);
    anim->setDuration(150);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    // Detach the effect once the animation finishes — leaving an effect in
    // place hurts repaint cost on every redraw of that page afterwards.
    connect(anim, &QPropertyAnimation::finished, page, [page]() {
        page->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void App::closeEvent(QCloseEvent *event)
{
    if (SettingManager::instance()->minimizeToTray()
        && AppManager::instance()->trayIcon()) {
        hide();
        event->ignore();
    } else {
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
