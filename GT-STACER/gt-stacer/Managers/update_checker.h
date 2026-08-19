#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;

// Checks whether a newer GT-STACER release exists by querying the public GitHub
// Releases API (no account, no personal data — just the latest stable tag). The
// check is opt-in via Settings and can be run manually. Results arrive on the
// signals below; nothing is downloaded or installed automatically.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);

    void checkNow();   // async; emits exactly one of the signals below

    // Is `candidate` (e.g. "26.10") a newer YY.MM than `current` (e.g. "26.09")?
    static bool isNewer(const QString &candidate, const QString &current);

signals:
    void updateAvailable(const QString &version, const QString &url);
    void upToDate(const QString &currentVersion);
    void checkFailed(const QString &error);

private:
    QNetworkAccessManager *m_net = nullptr;
};
