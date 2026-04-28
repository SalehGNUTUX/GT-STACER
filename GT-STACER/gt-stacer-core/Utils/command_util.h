#pragma once
#include <QString>
#include <QStringList>

class CommandUtil {
public:
    static QString exec(const QString &command);
    static QStringList execLines(const QString &command);
    static int execStatus(const QString &command);
    static bool commandExists(const QString &command);
    static QString execSudo(const QString &command);
};
