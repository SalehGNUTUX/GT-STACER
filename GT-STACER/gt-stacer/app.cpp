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
#include "Pages/Relief/relief_page.h"
#include "Pages/Connections/connections_page.h"
#include "Pages/Power/power_page.h"
#include "Pages/Firewall/firewall_page.h"
#include "Pages/Backup/backup_page.h"
#include "Pages/Recovery/recovery_page.h"
#include "Dialogs/about_dialog.h"

#include <QHBoxLayout>
#include <QCloseEvent>
#include <QApplication>
#include <QSet>
#include <QShortcut>
#include <QKeySequence>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDir>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>

static constexpr int kNumPages = 16;

App::App(QWidget *parent) : QMainWindow(parent)
{
    auto *s = SettingManager::instance();
    // Apply language BEFORE building any widgets so tr() resolves to the
    // correct strings on first construction (avoids the previous workaround of
    // calling setupSidebar() twice and broadcasting LanguageChange events).
    AppManager::instance()->applyLanguage(s->effectiveLanguage());
    AppManager::instance()->applyTheme(s->theme());

    setupUi();
    setupPages();
    setupSidebar();
    AppManager::instance()->initTray(this);

    // ── Keyboard shortcuts ─────────────────────────────────────────────
    // Ctrl+1..9,0 jump to the first ten sidebar pages (0 = page 10, Helpers).
    // Pages beyond the tenth (System Relief) have no digit shortcut — mapping
    // an 11th would collide with Ctrl+1.
    for (int i = 0; i < qMin(kNumPages, 10); ++i) {
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

    // If System Relief's automatic mode is enabled, build its page now (without
    // showing it) so its background watchdog starts with the app instead of only
    // once the user first opens the page. Its monitor is tagged keepAlive while
    // watching, so it keeps running when minimized to the tray.
    if (s->reliefAutoMode())
        materializePage(10);

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
    case 10: page = new ReliefPage;       break;
    case 11: page = new ConnectionsPage;  break;
    case 12: page = new PowerPage;        break;
    case 13: page = new FirewallPage;     break;
    case 14: page = new BackupPage;       break;
    case 15: page = new RecoveryPage;     break;
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
    // Visual order is grouped by workflow (Monitor → Maintenance → Backup/
    // Recovery → Control → Config → App). The trailing number is the FIXED page
    // index in the QStackedWidget — it must not change (Settings stays 8, etc.);
    // only the display order here changes.
    const QVector<SI> items = {
        // Monitor
        {SidebarIcons::dashboard(),   tr("Dashboard"),      tr("System overview"),            0},
        {SidebarIcons::resources(),   tr("Resources"),      tr("CPU, RAM, GPU, Network"),     1},
        {SidebarIcons::processes(),   tr("Processes"),      tr("Running processes"),          2},
        {SidebarIcons::connections(), tr("Connections"),    tr("Live network connections"),  11},
        // Maintenance
        {SidebarIcons::cleaner(),     tr("System Cleaner"), tr("Free up disk space"),         5},
        {SidebarIcons::uninstaller(), tr("Uninstaller"),    tr("Remove packages"),            6},
        {SidebarIcons::relief(),      tr("System Relief"),  tr("Relieve RAM/CPU pressure"),  10},
        // Backup & recovery
        {SidebarIcons::backup(),      tr("Backup"),         tr("Snapshots & home backup"),   14},
        {SidebarIcons::recovery(),    tr("Recovery"),       tr("Recover deleted files"),     15},
        // Control
        {SidebarIcons::services(),    tr("Services"),       tr("System services"),            3},
        {SidebarIcons::startup(),     tr("Startup Apps"),   tr("Autostart applications"),     4},
        {SidebarIcons::power(),       tr("Power"),          tr("Power profile & battery"),   12},
        // Config
        {SidebarIcons::aptSources(),  tr("APT Sources"),    tr("Package repositories"),       7},
        {SidebarIcons::firewall(),    tr("Firewall"),       tr("Manage firewall rules"),     13},
        // App
        {SidebarIcons::helpers(),     tr("Helpers"),        tr("System utilities"),           9},
        {SidebarIcons::settings(),    tr("Settings"),       tr("Application settings"),       8},
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
        // `lang` is the raw selection ("auto" or a code); store it verbatim and
        // apply the resolved language so "auto" follows the system locale.
        SettingManager::instance()->setLanguage(lang);
        AppManager::instance()->applyLanguage(SettingManager::instance()->effectiveLanguage());
        // Rebuild sidebar with translated strings.
        m_sidebar->clearItems();
        setupSidebar();
        m_sidebar->setActiveIndex(m_currentPage);
        // Retranslate every page that has actually been materialized.
        // .ui-backed pages catch LanguageChange and call ui->retranslateUi().
        QEvent langEvent(QEvent::LanguageChange);
        for (QWidget *page : m_pages)
            if (page) QApplication::sendEvent(page, &langEvent);
        // Programmatic pages build their static text with tr() in the ctor and
        // have no retranslateUi(), so LanguageChange leaves them in the old
        // language — rebuild those fresh in the new language.
        rebuildProgrammaticPages();
    });
}

void App::rebuildProgrammaticPages()
{
    // Pages built entirely in code (no .ui, so no ui->retranslateUi()) don't
    // pick up a runtime language switch. They always construct in the *current*
    // language, so the robust fix is to recreate them rather than hand-maintain
    // a retranslate list. .ui-backed pages are excluded — they retranslate fine.
    static const QSet<int> programmatic = {0, 10, 11, 12, 13, 14, 15};
    for (int i : programmatic) {
        if (!m_pages[i]) continue;             // not materialized → builds fresh on first visit
        QWidget *old = m_pages[i];
        auto *ph = new QWidget;                // fresh placeholder to hold the stack slot
        m_pageStack->insertWidget(i, ph);
        m_pageStack->removeWidget(old);
        old->deleteLater();
        m_placeholders[i] = ph;
        m_pages[i] = nullptr;
    }
    // Rebuild the page on screen now so the switch is visible immediately.
    materializePage(m_currentPage);
    m_pageStack->setCurrentIndex(m_currentPage);
    // Keep System Relief's background watchdog alive across the switch.
    if (SettingManager::instance()->reliefAutoMode())
        materializePage(10);
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

void App::captureAllPages(const QString &dir, int onlyPage)
{
    QDir().mkpath(dir);
    // Two constraints:
    //  1. Quality — render big and crisp. We keep the on-screen window inside
    //     the available work area (a window sized == screen hangs on show()
    //     under Wayland while the compositor negotiates fullscreen/maximize),
    //     then render each page at SS× supersampling for a high-res image.
    //  2. Aspect — lock the mockup's exact 993:610 so object-fit:fill on the
    //     site introduces zero distortion.
    // The on-screen window stays at the mockup's exact 993:610 size — a large or
    // near-fullscreen window is unreliable to realize/grab on Wayland (it hangs
    // while the compositor negotiates maximize/fullscreen). Quality comes from
    // rendering each page at SS× supersampling onto a high-DPR pixmap, which is
    // crisper than a native fullscreen grab would be.
    const int W = 993, H = 610;
    const double SS = 3.0;                  // 993×610 → 2979×1830 output
    resize(W, H);
    show();
    raise();

    // Pump events for `ms` while letting timers, gauge animations and page
    // fade-ins run — a grab would otherwise catch a half-populated page.
    auto settle = [](int ms) {
        QElapsedTimer t; t.start();
        while (t.elapsed() < ms) {
            QApplication::processEvents(QEventLoop::AllEvents, 30);
            QThread::msleep(15);
        }
    };
    settle(700);   // first layout + Info/Setting managers warm up

    for (int i = 0; i < kNumPages; ++i) {
        if (onlyPage >= 0 && i != onlyPage) continue;
        navigateTo(i);
        // Base 5s lets most pages settle; the two that load a big list off-thread
        // get much longer so nothing is captured mid-"Loading…". The Uninstaller
        // scans 4000+ packages on a cold cache (>5s first-run), so give it 30s.
        int waitMs = 5000;
        if      (i == 6) waitMs = 30000;  // Uninstaller
        else if (i == 5) waitMs = 10000;  // System Cleaner
        settle(waitMs);
        // Render at SS× onto a high-DPR pixmap — text/gauges stay vector-crisp
        // (true supersampling, not an upscale of a small grab).
        QPixmap pm(int(W * SS), int(H * SS));
        pm.setDevicePixelRatio(SS);
        pm.fill(Qt::transparent);
        render(&pm);
        QImage img = pm.toImage();
        img.setDevicePixelRatio(1.0);
        const QString out = QString("%1/%2.png").arg(dir).arg(i);
        if (!img.save(out))
            qWarning() << "capture: failed to write" << out;
    }
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
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

// When the window is hidden (minimised to tray), pause every page's QTimer.
// Refresh data in the background while the user can't see it is pure waste —
// CPU samples, network polls and table rebuilds chew through battery without
// anyone benefiting. We restart them on showEvent().
//
// The CpuSampler worker thread (in gt-stacer-core) keeps running because the
// tray tooltip still needs current values; it's already throttled to one
// sample per second.
void App::hideEvent(QHideEvent *event)
{
    for (QWidget *page : m_pages) {
        if (!page) continue;
        for (QTimer *t : page->findChildren<QTimer*>()) {
            // keepAlive timers (e.g. the System Relief auto-mode watchdog) must
            // keep running in the tray — background protection is their purpose.
            if (t->property("keepAlive").toBool()) continue;
            if (t->isActive()) { t->setProperty("wasActive", true); t->stop(); }
        }
    }
    QMainWindow::hideEvent(event);
}

void App::showEvent(QShowEvent *event)
{
    for (QWidget *page : m_pages) {
        if (!page) continue;
        for (QTimer *t : page->findChildren<QTimer*>())
            if (t->property("wasActive").toBool()) { t->setProperty("wasActive", false); t->start(); }
    }
    QMainWindow::showEvent(event);
}
