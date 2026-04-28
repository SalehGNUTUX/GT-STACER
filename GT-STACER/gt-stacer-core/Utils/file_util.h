#pragma once
#include <QString>
#include <QStringList>
#include <QFileInfo>

class FileUtil {
public:
    static QString readFile(const QString &path);
    static bool writeFile(const QString &path, const QString &content);
    static qint64 dirSize(const QString &path);
    static QStringList dirFiles(const QString &path, bool recursive = false);
    static bool removeFile(const QString &path);
    static bool removeDir(const QString &path);
    static QString sysPath(const QString &relativePath);
};
