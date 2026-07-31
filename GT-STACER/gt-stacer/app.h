#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>
#include <QVector>

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

    // Dev/website tool: navigate every page and save it as <dir>/<index>.png at
    // the website mockup size. Quits the app when done. Invoked via --capture.
    // onlyPage >= 0 captures just that one page index (e.g. to redo a slow one).
    void captureAllPages(const QString &dir, int onlyPage = -1);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event)     override;
    void hideEvent(QHideEvent *event)   override;
    void showEvent(QShowEvent *event)   override;

private slots:
    void navigateTo(int index);

private:
    void setupUi();
    void setupPages();
    void setupSidebar();
    void setupSettingsConnections();
    QWidget *materializePage(int index);

    QWidget        *m_centralWidget = nullptr;
    Sidebar        *m_sidebar       = nullptr;
    QStackedWidget *m_pageStack     = nullptr;

    // Pages are lazy-constructed on first navigation to that index.
    QVector<QWidget*> m_pages;       // size == NumPages, nullptr until built
    QVector<QWidget*> m_placeholders;// stack widgets shown until materialized

    int m_currentPage = 0;
};
