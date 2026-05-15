#include "apt_source_tool.h"
#include "../Utils/file_util.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QDateTime>

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

// Validate fields so a malformed AptSource can't produce a corrupt sources.list
// or smuggle in shell metacharacters via legacy callers.
static bool isValidSource(const AptSource &s)
{
    if (s.type != "deb" && s.type != "deb-src") return false;
    static const QRegularExpression uriRx(R"(^[A-Za-z][A-Za-z0-9+.\-]*://[^\s\n#]+$)");
    static const QRegularExpression suiteRx(R"(^[A-Za-z0-9._\-/]+$)");
    static const QRegularExpression compsRx(R"(^[A-Za-z0-9._\-\s]+$)");
    if (!uriRx.match(s.uri).hasMatch()) return false;
    if (!suiteRx.match(s.suite).hasMatch()) return false;
    if (!s.components.isEmpty() && !compsRx.match(s.components).hasMatch()) return false;
    return true;
}

// Allow only files inside /etc/apt/sources.list.d/ or /etc/apt/sources.list.
static bool isManagedSourcePath(const QString &path)
{
    QString clean = QDir::cleanPath(path);
    if (clean == "/etc/apt/sources.list") return true;
    if (!clean.startsWith("/etc/apt/sources.list.d/")) return false;
    if (clean.contains("/..")) return false;
    return clean.endsWith(".list");
}

bool AptSourceTool::add(const AptSource &source)
{
    if (!isValidSource(source)) return false;

    // Each addition lands in its own file inside sources.list.d so we never
    // need to read-modify-write a shared file.
    QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    QString fileName = QString("/etc/apt/sources.list.d/gt-stacer-%1.list").arg(stamp);

    QString line = QString("%1 %2 %3 %4\n")
        .arg(source.type, source.uri, source.suite, source.components).trimmed() + "\n";

    return CommandUtil::pkexecWriteFile(fileName, line.toUtf8(),
                                         "root", "root", "0644");
}

bool AptSourceTool::remove(const QString &filePath)
{
    if (!isManagedSourcePath(filePath)) return false;
    // /etc/apt/sources.list itself shouldn't be deleted — only files in .list.d.
    if (filePath == "/etc/apt/sources.list") return false;
    return CommandUtil::execProgram("pkexec", {"rm", "-f", filePath}, 30000) == 0;
}

bool AptSourceTool::setEnabled(const QString &filePath, bool enabled)
{
    if (!isManagedSourcePath(filePath)) return false;
    // sed pattern is constant (no user data interpolated). Pass filePath as a
    // separate argv element so it cannot be interpreted as part of the script.
    QString pattern = enabled
        ? QStringLiteral(R"(s/^#\s*\(deb\)/\1/)")
        : QStringLiteral(R"(s/^\(deb\)/#\1/)");
    return CommandUtil::execProgram("pkexec",
        {"sed", "-i", pattern, filePath}, 30000) == 0;
}

bool AptSourceTool::update()
{
    return CommandUtil::execProgram("pkexec", {"apt-get", "update"}, 300000) == 0;
}
