#pragma once
#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;

// One downloadable file attached to a GitHub release.
struct ReleaseAsset {
    QString name;   // e.g. GT-STACER-26.12-x86_64.AppImage
    QString url;    // browser_download_url
    qint64  size = 0;
};

// Checks whether a newer GT-STACER release exists by querying the public GitHub
// Releases API (no account, no personal data — just the latest stable tag). The
// check is opt-in via Settings and can be run manually. Results arrive on the
// signals below; nothing is downloaded or installed automatically.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);

    void checkNow();   // async; emits exactly one of the signals below

    // Populated after a successful check (valid inside/after updateAvailable).
    QString latestVersion() const { return m_latestVersion; }
    QString latestTag()     const { return m_latestTag; }
    QVector<ReleaseAsset> assets() const { return m_assets; }

    // Is `candidate` (e.g. "26.10") a newer YY.MM than `current` (e.g. "26.09")?
    static bool isNewer(const QString &candidate, const QString &current);

signals:
    void updateAvailable(const QString &version, const QString &url);
    void upToDate(const QString &currentVersion);
    void checkFailed(const QString &error);

private:
    QNetworkAccessManager *m_net = nullptr;
    QString m_latestVersion;
    QString m_latestTag;
    QVector<ReleaseAsset> m_assets;
};
