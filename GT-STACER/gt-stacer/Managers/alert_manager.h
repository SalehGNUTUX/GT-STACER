#pragma once
#include <QObject>
#include <QTimer>
#include <QHash>
#include <QDateTime>

// Polls system state on a timer and fires desktop notifications when any of
// the user-configurable thresholds is crossed. Each alert has a cool-down
// (default 5 min) so a stuck-hot CPU doesn't spam the notification daemon.
//
// Thresholds are persisted via SettingManager and read on each tick — no
// restart needed after the user adjusts them.
class AlertManager : public QObject {
    Q_OBJECT
public:
    static AlertManager *instance();

    // Stop/start polling without losing settings (called from Settings page).
    void  setEnabled(bool on);
    bool  isEnabled() const { return m_enabled; }

private slots:
    void tick();

private:
    explicit AlertManager(QObject *parent = nullptr);

    // Returns true if enough time has passed since the last alert with this key.
    bool readyToFire(const QString &key);
    void fire(const QString &key,
              const QString &title,
              const QString &body,
              bool critical = false,
              const QString &icon = QString());

    QTimer *m_timer = nullptr;
    bool    m_enabled = true;
    QHash<QString, qint64> m_lastFiredMs;
    static AlertManager *s_instance;

    static constexpr qint64 kCooldownMs = 5 * 60 * 1000; // 5 minutes
};
