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

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event)     override;

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
