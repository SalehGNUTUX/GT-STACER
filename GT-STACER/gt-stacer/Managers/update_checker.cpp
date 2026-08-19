#include "update_checker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this))
{
}

bool UpdateChecker::isNewer(const QString &candidate, const QString &current)
{
    auto ym = [](const QString &v, int &yy, int &mm) {
        const QStringList p = v.split('.');
        yy = p.value(0).toInt();
        mm = p.value(1).toInt();
    };
    int cy = 0, cm = 0, uy = 0, um = 0;
    ym(candidate, cy, cm);
    ym(current, uy, um);
    return cy > uy || (cy == uy && cm > um);
}

void UpdateChecker::checkNow()
{
    QNetworkRequest req(QUrl("https://api.github.com/repos/SalehGNUTUX/GT-STACER/releases/latest"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "GT-STACER");   // GitHub requires a UA
    // /releases/latest already excludes pre-releases, so the newest STABLE wins.
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = obj.value("tag_name").toString();     // e.g. GT-STACER_26.10_STABLE
        QString url = obj.value("html_url").toString();
        if (url.isEmpty()) url = "https://github.com/SalehGNUTUX/GT-STACER/releases";
        static const QRegularExpression re("(\\d{2}\\.\\d{2})");
        const auto m = re.match(tag);
        if (!m.hasMatch()) {
            emit checkFailed(tr("Could not read the latest release version."));
            return;
        }
        const QString latest  = m.captured(1);
        const QString current = QStringLiteral(APP_VERSION);
        if (isNewer(latest, current)) emit updateAvailable(latest, url);
        else                          emit upToDate(current);
    });
}
