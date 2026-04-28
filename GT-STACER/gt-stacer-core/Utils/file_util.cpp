#include "file_util.h"
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>

QString FileUtil::readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QTextStream(&file).readAll().trimmed();
}

bool FileUtil::writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream(&file) << content;
    return true;
}

qint64 FileUtil::dirSize(const QString &path)
{
    qint64 size = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
    }
    return size;
}

QStringList FileUtil::dirFiles(const QString &path, bool recursive)
{
    QStringList files;
    QDirIterator::IteratorFlags flags = recursive
        ? QDirIterator::Subdirectories
        : QDirIterator::NoIteratorFlags;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, flags);
    while (it.hasNext()) {
        files << it.next();
    }
    return files;
}

bool FileUtil::removeFile(const QString &path)
{
    return QFile::remove(path);
}

bool FileUtil::removeDir(const QString &path)
{
    return QDir(path).removeRecursively();
}

QString FileUtil::sysPath(const QString &relativePath)
{
    return QString("/sys/%1").arg(relativePath);
}
