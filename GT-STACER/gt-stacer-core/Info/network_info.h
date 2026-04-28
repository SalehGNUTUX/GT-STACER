#pragma once
#include <QString>
#include <QVector>

struct NetworkInterface {
    QString name;
    QString ipv4;
    QString ipv6;
    QString macAddress;
    bool    isUp      = false;
    bool    isWireless = false;
    qint64  rxBytes   = 0;
    qint64  txBytes   = 0;
    qint64  rxSpeed   = 0;
    qint64  txSpeed   = 0;
};

class NetworkInfo {
public:
    static QVector<NetworkInterface> interfaces();
    static NetworkInterface          defaultInterface();
};
