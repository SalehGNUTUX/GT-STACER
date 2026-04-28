#pragma once
#include <QObject>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QTimer>
#include <QString>
#include <QStringList>

class AppManager : public QObject {
    Q_OBJECT
public:
    static AppManager *instance();

    void applyTheme(const QString &theme);
    void applyLanguage(const QString &lang);
    void initTray(QWidget *mainWindow);
    void showTray();
    void hideTray();

    QSystemTrayIcon *trayIcon() const { return m_tray; }
    QString currentTheme() const { return m_theme; }

    static bool isDarkSystemTheme();

signals:
    void themeChanged(const QString &theme);
    void languageChanged();

private slots:
    void updateTrayTooltip();

private:
    explicit AppManager(QObject *parent = nullptr);
    static AppManager *m_instance;

    QSystemTrayIcon *m_tray       = nullptr;
    QTranslator     *m_translator = nullptr;
    QTimer          *m_trayTimer  = nullptr;
    QString          m_theme;

    QString loadStylesheet(const QString &theme);
};
