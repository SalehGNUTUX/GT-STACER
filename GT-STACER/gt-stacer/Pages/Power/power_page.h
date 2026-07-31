#pragma once
#include <QWidget>
#include <QHash>
#include "../../Managers/sleep_inhibitor.h"

class QLabel;
class QPushButton;
class QButtonGroup;
class QSpinBox;
class QGroupBox;
class QTimer;

// Power management page (26.08 "network & power tools"). Adapts to the device:
//   • Power profile — power-profiles-daemon if present, else cpufreq governors.
//   • Battery — only on laptops: live status plus charge-limit thresholds to
//     preserve battery health. Hidden entirely on desktops.
class PowerPage : public QWidget {
    Q_OBJECT
public:
    explicit PowerPage(QWidget *parent = nullptr);
    ~PowerPage() override;

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void refreshState();
    void applyChargeLimit();
    void toggleSleepBlock();                  // keep-awake inhibitor on/off

private:
    void buildProfileSection(QWidget *parent);
    void buildBatterySection(QWidget *parent);
    void buildSleepSection(QWidget *parent);
    void updateSleepUi();
    void selectProfile(const QString &id);   // user clicked a profile/governor

    bool m_usePpd = false;                    // profiles via powerprofilesctl vs cpufreq
    QGroupBox   *m_profileBox    = nullptr;
    QHash<QString, QPushButton*> m_profileBtns;
    QLabel      *m_profileStatus = nullptr;

    QGroupBox   *m_batteryBox   = nullptr;
    QLabel      *m_batteryInfo  = nullptr;
    QGroupBox   *m_limitBox     = nullptr;
    QSpinBox    *m_startSpin    = nullptr;
    QSpinBox    *m_endSpin      = nullptr;
    QLabel      *m_limitStatus  = nullptr;

    QGroupBox    *m_sleepBox    = nullptr;
    QLabel       *m_sleepStatus = nullptr;
    QPushButton  *m_blockBtn    = nullptr;
    SleepInhibitor m_inhibitor;             // cross-desktop keep-awake (D-Bus/logind)

    QTimer      *m_timer = nullptr;   // visible-only live readout
};
