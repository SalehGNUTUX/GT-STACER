#pragma once
#include <QString>
#include <QVector>

struct AptSource {
    QString type;      // "deb" or "deb-src"
    QString uri;
    QString suite;
    QString components;
    bool    enabled = true;
    QString filePath;
    QString comment;
};

class AptSourceTool {
public:
    static QVector<AptSource> sources();
    static bool add(const AptSource &source);
    static bool remove(const QString &filePath);
    static bool setEnabled(const QString &filePath, bool enabled);
    static bool update();
};
