#include "self_updater.h"
#include "../../gt-stacer-core/Utils/command_util.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <functional>

SelfUpdater::SelfUpdater(QObject *parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this))
{
    // GitHub download URLs redirect to the objects CDN — follow them.
    m_net->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

SelfUpdater::Method SelfUpdater::installMethod()
{
    if (qEnvironmentVariableIsSet("APPIMAGE")) return Method::AppImage;
    if (qEnvironmentVariableIsSet("FLATPAK_ID") || QFileInfo::exists("/.flatpak-info"))
        return Method::Flatpak;
    const QString bin = QCoreApplication::applicationFilePath();
    if (CommandUtil::commandExists("dpkg")
        && CommandUtil::execProgram("dpkg", {"-S", bin}, 6000) == 0)
        return Method::Deb;
    if (CommandUtil::commandExists("rpm")
        && CommandUtil::execProgram("rpm", {"-qf", bin}, 6000) == 0)
        return Method::Rpm;
    return Method::Unsupported;
}

QString SelfUpdater::methodLabel(Method m)
{
    switch (m) {
    case Method::AppImage: return "AppImage";
    case Method::Deb:      return "DEB";
    case Method::Rpm:      return "RPM";
    case Method::Flatpak:  return "Flatpak";
    default:               return "source";
    }
}

bool SelfUpdater::canAutoInstall()
{
    const Method m = installMethod();
    return m == Method::AppImage || m == Method::Deb || m == Method::Rpm;
}

void SelfUpdater::cancel()
{
    m_cancelled = true;
    if (m_reply) m_reply->abort();
}

void SelfUpdater::start(const QString &version, const QVector<ReleaseAsset> &assets)
{
    m_version = version;
    const Method method = installMethod();
    if (!canAutoInstall()) {
        emit finished(false, method == Method::Flatpak
            ? tr("This is a Flatpak install — update it with your software centre or "
                 "\"flatpak update org.gnutux.gt-stacer\".")
            : tr("This build was not installed from a package (running from source?). "
                 "Download the new version from the release page."));
        return;
    }

    // Choose the asset that matches how we were installed, and locate SHA256SUMS.
    QString wantExt;
    switch (method) {
    case Method::AppImage: wantExt = ".appimage"; break;
    case Method::Deb:      wantExt = ".deb";      break;
    case Method::Rpm:      wantExt = ".rpm";      break;
    default: break;
    }
    m_asset = {};
    m_sumsUrl.clear();
    for (const ReleaseAsset &a : assets) {
        if (a.name.endsWith(wantExt, Qt::CaseInsensitive) && m_asset.url.isEmpty())
            m_asset = a;
        if (a.name.compare("SHA256SUMS.txt", Qt::CaseInsensitive) == 0)
            m_sumsUrl = a.url;
    }
    if (m_asset.url.isEmpty()) {
        emit finished(false, tr("The new release has no %1 package to install.").arg(wantExt));
        return;
    }

    // For AppImage we download beside the current file so the final replace is an
    // atomic same-directory rename; otherwise use the cache directory.
    QString dir;
    if (method == Method::AppImage)
        dir = QFileInfo(qEnvironmentVariable("APPIMAGE")).absolutePath();
    if (dir.isEmpty() || !QFileInfo(dir).isWritable())
        dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    m_pkgPath  = dir + "/." + m_asset.name + ".download";
    m_sumsPath = dir + "/.gt-stacer-SHA256SUMS.txt";

    fetch(m_asset.url, m_pkgPath, [this]{ afterPackageDownloaded(); }, tr("Downloading"));
}

void SelfUpdater::fetch(const QString &url, const QString &savePath,
                        const std::function<void()> &onDone, const QString &phase)
{
    auto *file = new QFile(savePath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete file;
        emit finished(false, tr("Could not write to %1.").arg(savePath));
        return;
    }
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "GT-STACER");
    m_reply = m_net->get(req);

    connect(m_reply, &QNetworkReply::readyRead, this, [this, file]{
        file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this, phase](qint64 rec, qint64 total){
        emit progress(total > 0 ? int(rec * 100 / total) : 0, phase);
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, file, onDone]{
        file->write(m_reply->readAll());
        file->close();
        delete file;
        auto *r = m_reply; m_reply = nullptr;
        const bool err = r->error() != QNetworkReply::NoError;
        r->deleteLater();
        if (m_cancelled) { emit finished(false, tr("Update cancelled.")); return; }
        if (err) { emit finished(false, tr("Download failed: %1").arg(r->errorString())); return; }
        onDone();
    });
}

void SelfUpdater::afterPackageDownloaded()
{
    if (m_sumsUrl.isEmpty()) { afterSumsDownloaded(); return; }   // no sums → skip (verified below fails safe)
    fetch(m_sumsUrl, m_sumsPath, [this]{ afterSumsDownloaded(); }, tr("Verifying"));
}

void SelfUpdater::afterSumsDownloaded()
{
    emit progress(100, tr("Verifying"));

    // Expected checksum for our asset from SHA256SUMS.txt ("<sha>  <name>").
    QString expected;
    QFile sf(m_sumsPath);
    if (sf.open(QIODevice::ReadOnly)) {
        for (const QByteArray &line : sf.readAll().split('\n')) {
            const QString l = QString::fromUtf8(line).trimmed();
            if (l.endsWith(m_asset.name)) { expected = l.section(' ', 0, 0); break; }
        }
    }
    // Compute the SHA-256 of the downloaded package (chunked for large files).
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QFile pf(m_pkgPath);
    if (!pf.open(QIODevice::ReadOnly)) { emit finished(false, tr("Downloaded file is unreadable.")); return; }
    if (!hash.addData(&pf)) { emit finished(false, tr("Could not read the downloaded file.")); return; }
    const QString got = QString::fromLatin1(hash.result().toHex());

    if (expected.isEmpty()) {
        QFile::remove(m_pkgPath);
        emit finished(false, tr("Could not verify the download (no checksum published). Aborted for safety."));
        return;
    }
    if (got.compare(expected, Qt::CaseInsensitive) != 0) {
        QFile::remove(m_pkgPath);
        emit finished(false, tr("Checksum mismatch — the download may be corrupt or tampered with. Aborted."));
        return;
    }

    emit progress(100, tr("Installing"));
    QString err;
    if (installFile(m_pkgPath, err))
        emit finished(true, tr("Updated to %1. Restart GT-STACER to run the new version.").arg(m_version));
    else
        emit finished(false, err.isEmpty() ? tr("Installation failed.") : err);
    if (installMethod() != Method::AppImage) QFile::remove(m_pkgPath);
}

bool SelfUpdater::installFile(const QString &file, QString &err)
{
    switch (installMethod()) {
    case Method::AppImage: {
        const QString target = qEnvironmentVariable("APPIMAGE");
        if (target.isEmpty()) { err = tr("Cannot locate the current AppImage."); return false; }
        QFile::setPermissions(file, QFileInfo(file).permissions()
            | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
        // Move the old one aside, then rename the new one into place (atomic on the
        // same filesystem — that's why we downloaded next to it).
        const QString backup = target + ".old";
        QFile::remove(backup);
        if (QFileInfo::exists(target) && !QFile::rename(target, backup)) {
            err = tr("Cannot replace %1 (permission denied?).").arg(target); return false;
        }
        if (!QFile::rename(file, target)) {
            QFile::rename(backup, target);   // restore
            err = tr("Could not put the new AppImage in place."); return false;
        }
        QFile::remove(backup);
        return true;
    }
    case Method::Deb:
        return CommandUtil::execProgram("pkexec", {"dpkg", "-i", file}, 600000) == 0;
    case Method::Rpm:
        return CommandUtil::execProgram("pkexec", {"rpm", "-U", "--replacepkgs", file}, 600000) == 0;
    default:
        err = tr("Unsupported install type.");
        return false;
    }
}
