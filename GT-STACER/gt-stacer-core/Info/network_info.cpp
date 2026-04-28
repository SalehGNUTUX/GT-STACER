#include "network_info.h"
#include "../Utils/file_util.h"
#include <QNetworkInterface>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>

// ─── حساب السرعة بتتبع التغيير بين استدعاءين ──────────────────────────
namespace {
    struct IfaceStats { qint64 rx = 0; qint64 tx = 0; };
    QHash<QString, IfaceStats> s_prev;
    QElapsedTimer               s_timer;
    QMutex                      s_mutex;
}

QVector<NetworkInterface> NetworkInfo::interfaces()
{
    QMutexLocker lock(&s_mutex);

    // وقت منذ آخر استدعاء (بالثواني، حدّ أدنى 0.1)
    double elapsed = 1.0;
    if (s_timer.isValid() && s_timer.elapsed() > 50) {
        elapsed = s_timer.elapsed() / 1000.0;
    }
    s_timer.restart();

    QVector<NetworkInterface> result;

    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (iface.name() == "lo") continue;

        NetworkInterface ni;
        ni.name       = iface.name();
        ni.macAddress = iface.hardwareAddress();
        ni.isUp       = iface.flags().testFlag(QNetworkInterface::IsUp);
        ni.isWireless = QDir(QString("/sys/class/net/%1/wireless").arg(iface.name())).exists();

        for (const auto &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                ni.ipv4 = entry.ip().toString();
            else if (entry.ip().protocol() == QAbstractSocket::IPv6Protocol && ni.ipv6.isEmpty())
                ni.ipv6 = entry.ip().toString();
        }

        // قراءة البايتات التراكمية
        QString base = QString("/sys/class/net/%1/statistics/").arg(iface.name());
        ni.rxBytes = FileUtil::readFile(base + "rx_bytes").toLongLong();
        ni.txBytes = FileUtil::readFile(base + "tx_bytes").toLongLong();

        // حساب السرعة اللحظية
        if (s_prev.contains(ni.name)) {
            auto &prev = s_prev[ni.name];
            qint64 dRx = ni.rxBytes - prev.rx;
            qint64 dTx = ni.txBytes - prev.tx;
            ni.rxSpeed = (dRx > 0) ? static_cast<qint64>(dRx / elapsed) : 0;
            ni.txSpeed = (dTx > 0) ? static_cast<qint64>(dTx / elapsed) : 0;
        }
        s_prev[ni.name] = {ni.rxBytes, ni.txBytes};

        result << ni;
    }
    return result;
}

NetworkInterface NetworkInfo::defaultInterface()
{
    auto ifaces = interfaces();
    for (const auto &iface : ifaces) {
        if (iface.isUp && !iface.ipv4.isEmpty())
            return iface;
    }
    return ifaces.isEmpty() ? NetworkInterface{} : ifaces.first();
}
