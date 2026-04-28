#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>

class Sidebar;
class DashboardPage;
class ResourcesPage;
class ProcessesPage;
class ServicesPage;
class StartupAppsPage;
class SystemCleanerPage;
class UninstallerPage;
class AptSourcePage;
class SettingsPage;
class HelpersPage;

class App : public QMainWindow {
    Q_OBJECT
public:
    explicit App(QWidget *parent = nullptr);
    ~App() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event)     override;

private slots:
    void navigateTo(int index);

private:
    void setupUi();
    void setupPages();
    void setupSidebar();
    void setupConnections();
    void applyThemeToStyle();

    QWidget        *m_centralWidget = nullptr;
    Sidebar        *m_sidebar       = nullptr;
    QStackedWidget *m_pageStack     = nullptr;

    DashboardPage    *m_dashboard    = nullptr;
    ResourcesPage    *m_resources    = nullptr;
    ProcessesPage    *m_processes    = nullptr;
    ServicesPage     *m_services     = nullptr;
    StartupAppsPage  *m_startupApps  = nullptr;
    SystemCleanerPage *m_cleaner     = nullptr;
    UninstallerPage  *m_uninstaller  = nullptr;
    AptSourcePage    *m_aptSource    = nullptr;
    SettingsPage     *m_settings     = nullptr;
    HelpersPage      *m_helpers      = nullptr;

    int m_currentPage = 0;
};
