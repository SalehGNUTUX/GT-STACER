#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include "update_checker.h"

class QNetworkAccessManager;
class QNetworkReply;

// Downloads and installs a newer GT-STACER release — the check→download→verify→
// install flow used by GMD / GT-SIRM / GT-SALAT. It is ENTIRELY opt-in: nothing
// here runs unless the user explicitly asks (a "Download & install" action). The
// install method matches how this instance was installed:
//   • AppImage  → replace the running $APPIMAGE file (no root)
//   • DEB / RPM → hand the package to the system package manager via pkexec
//   • Flatpak   → tell the user to update the bundle (host boundary)
//   • Source/unknown → tell the user, open the release page
// The downloaded file's SHA-256 is verified against the release's SHA256SUMS.txt
// before anything is installed.
class SelfUpdater : public QObject {
    Q_OBJECT
public:
    enum class Method { AppImage, Deb, Rpm, Flatpak, Unsupported };

    explicit SelfUpdater(QObject *parent = nullptr);

    static Method  installMethod();          // how the running instance was installed
    static QString methodLabel(Method m);    // human, untranslated (for logs)
    static bool    canAutoInstall();         // AppImage/DEB/RPM → true

    // Pick the asset matching installMethod() from `assets`, download it (progress),
    // verify its checksum, then install. Emits finished() exactly once.
    void start(const QString &version, const QVector<ReleaseAsset> &assets);
    void cancel();

signals:
    void progress(int percent, const QString &phase);
    void finished(bool ok, const QString &message);

private:
    void fetch(const QString &url, const QString &savePath,
               const std::function<void()> &onDone, const QString &phase);
    void afterPackageDownloaded();
    void afterSumsDownloaded();
    bool installFile(const QString &file, QString &err);

    QNetworkAccessManager *m_net = nullptr;
    QString      m_version;
    ReleaseAsset m_asset;
    QString      m_sumsUrl;
    QString      m_pkgPath;
    QString      m_sumsPath;
    QNetworkReply *m_reply = nullptr;
    bool m_cancelled = false;
};
