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
    // Central hub for a runtime language switch: persist the choice, install the
    // translator, and emit languageChanged() so the whole app (sidebar, pages,
    // any open dialog) re-translates. Called from the Settings combo and the
    // Welcome / What's-new dialogs alike. `lang` is the raw choice ("auto" or a code).
    void changeLanguage(const QString &lang);
    void initTray(QWidget *mainWindow);
    void showTray();
    void hideTray();

    // Reflect the "keep awake" (sleep/lock inhibitor) state in the panel: badges
    // the tray icon and adds a tooltip line, so the active feature is visible
    // even when the window is closed.
    void setKeepAwake(bool on);
    bool keepAwake() const { return m_keepAwake; }

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
    bool             m_keepAwake  = false;

    QString loadStylesheet(const QString &theme);
};
