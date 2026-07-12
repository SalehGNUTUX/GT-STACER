#pragma once
#include <QWidget>

namespace Ui { class SettingsPage; }
class QTimer;

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

private slots:
    void applySettings();

private:
    void loadSettings();
    void startPowerTimer();
    void cancelPowerTimer();
    void tickPowerTimer();

    Ui::SettingsPage *ui;
    QTimer *m_powerTimer   = nullptr;
    int     m_powerRemaining = 0;   // seconds left until the scheduled action
    int     m_powerAction    = 0;   // PowerTool::Action index
};
