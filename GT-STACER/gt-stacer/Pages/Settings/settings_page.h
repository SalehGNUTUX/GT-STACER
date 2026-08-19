#pragma once
#include <QWidget>

namespace Ui { class SettingsPage; }
class QTimer;
class UpdateChecker;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

signals:
    void themeChanged(const QString &theme);
    void languageChanged(const QString &lang);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(class QShowEvent *event) override;

private slots:
    void applySettings();

private:
    void populateLanguageCombo();   // (re)fill with flag icons; call on lang change
    void loadSettings();
    void startPowerTimer();
    void cancelPowerTimer();
    void tickPowerTimer();
    void showWelcomeTour();
    void showWhatsNew();
    void copyShareText();
    void runUpdateCheck();          // manual "Check now"
    QString shareText() const;      // description + link + hashtags for sharing

    Ui::SettingsPage *ui;
    QTimer *m_powerTimer   = nullptr;
    int     m_powerRemaining = 0;   // seconds left until the scheduled action
    int     m_powerAction    = 0;   // PowerTool::Action index
    UpdateChecker *m_updateChecker = nullptr;
};
