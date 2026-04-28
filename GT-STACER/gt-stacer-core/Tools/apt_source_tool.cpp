#include "apt_source_tool.h"
#include "../Utils/file_util.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>

QVector<AptSource> AptSourceTool::sources()
{
    QVector<AptSource> result;
    QStringList files;

    // Main sources.list
    if (QFile::exists("/etc/apt/sources.list"))
        files << "/etc/apt/sources.list";

    // sources.list.d directory
    QDir dir("/etc/apt/sources.list.d");
    for (const auto &f : dir.entryList({"*.list"}, QDir::Files))
        files << dir.filePath(f);

    for (const auto &filePath : files) {
        auto lines = FileUtil::readFile(filePath).split('\n');
        for (const auto &raw : lines) {
            QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            AptSource s;
            s.filePath = filePath;
            s.enabled  = !line.startsWith('#');

            QString active = s.enabled ? line : line.mid(1).trimmed();
            if (active.isEmpty()) continue;

            auto parts = active.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() < 3) continue;
            if (parts[0] != "deb" && parts[0] != "deb-src") continue;

            s.type       = parts[0];
            s.uri        = parts[1];
            s.suite      = parts[2];
            s.components = parts.mid(3).join(' ');
            result << s;
        }
    }
    return result;
}

bool AptSourceTool::add(const AptSource &source)
{
    QString line = QString("%1 %2 %3 %4\n")
        .arg(source.type).arg(source.uri).arg(source.suite).arg(source.components);
    return CommandUtil::execStatus(
        QString("pkexec sh -c 'echo \"%1\" >> /etc/apt/sources.list.d/gt-stacer-added.list'")
        .arg(line.trimmed())) == 0;
}

bool AptSourceTool::remove(const QString &filePath)
{
    return CommandUtil::execStatus(
        QString("pkexec rm -f %1").arg(filePath)) == 0;
}

bool AptSourceTool::setEnabled(const QString &filePath, bool enabled)
{
    QString action = enabled
        ? QString("pkexec sed -i 's/^#\\s*\\(deb\\)/\\1/' %1").arg(filePath)
        : QString("pkexec sed -i 's/^\\(deb\\)/#\\1/' %1").arg(filePath);
    return CommandUtil::execStatus(action) == 0;
}

bool AptSourceTool::update()
{
    return CommandUtil::execStatus("pkexec apt-get update") == 0;
}
