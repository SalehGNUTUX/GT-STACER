#include "recovery_tool.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>

QString RecoverySource::label() const
{
    QString s = path + "  ·  " + size;
    if (isDisk && !model.isEmpty()) s += "  ·  " + model;
    else if (!fstype.isEmpty())     s += "  ·  " + fstype;
    if (mounted())                  s += "  ·  " + mountpoint;
    return s;
}

bool RecoveryTool::available()
{
    return CommandUtil::commandExists("photorec");
}

QVector<RecoverySource> RecoveryTool::sources()
{
    // -P prints unambiguous KEY="value" pairs (values may contain spaces).
    const QString out = CommandUtil::execProgramOutput(
        "lsblk", {"-Pno", "PATH,SIZE,TYPE,FSTYPE,MOUNTPOINT,MODEL"}, 6000);
    static const QRegularExpression kv(QStringLiteral("(\\w+)=\"([^\"]*)\""));

    QVector<RecoverySource> disks, parts;
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        QHash<QString, QString> f;
        auto it = kv.globalMatch(line);
        while (it.hasNext()) { auto m = it.next(); f.insert(m.captured(1), m.captured(2)); }
        const QString type = f.value("TYPE");
        if (type != "part" && type != "disk") continue;       // skip rom/loop/lvm…
        if (f.value("FSTYPE") == "swap") continue;
        RecoverySource s;
        s.path       = f.value("PATH");
        s.size       = f.value("SIZE");
        s.fstype     = f.value("FSTYPE");
        s.mountpoint = f.value("MOUNTPOINT");
        s.model      = f.value("MODEL").trimmed();
        s.isDisk     = (type == "disk");
        (s.isDisk ? disks : parts) << s;
    }
    // Partitions first (the usual recovery target), then whole disks.
    return parts + disks;
}

QVector<FileFamily> RecoveryTool::fileFamilies()
{
    // A curated set of common PhotoRec families (id = fileopt token). Not the
    // full ~480 formats — the picker stays usable; "recover all" covers the rest.
    return {
        {"jpg","JPEG image","Images"}, {"png","PNG image","Images"},
        {"gif","GIF image","Images"},  {"bmp","BMP image","Images"},
        {"tif","TIFF image","Images"}, {"webp","WebP image","Images"},
        {"pdf","PDF document","Documents"}, {"doc","MS Office (legacy)","Documents"},
        {"rtf","Rich text","Documents"},    {"txt","Text/HTML","Documents"},
        {"odt","OpenDocument","Documents"},
        {"zip","ZIP (incl. docx/xlsx)","Archives"}, {"rar","RAR archive","Archives"},
        {"gz","gzip","Archives"}, {"7z","7-Zip","Archives"}, {"tar","tar","Archives"},
        {"mp3","MP3 audio","Audio"}, {"ogg","Ogg audio","Audio"},
        {"flac","FLAC audio","Audio"}, {"wav","WAV audio","Audio"},
        {"mp4","MP4 video","Video"}, {"mov","QuickTime","Video"},
        {"avi","AVI video","Video"}, {"mkv","Matroska","Video"},
        {"sqlite","SQLite database","Other"}, {"iso","ISO image","Other"},
    };
}

QString RecoveryTool::recupBase(const QString &destDir)
{
    return destDir + "/gt-stacer-recovery";   // PhotoRec appends .1, .2, …
}

QStringList RecoveryTool::photorecArgs(const QString &source, const QString &destDir,
                                       const QStringList &enableIds)
{
    // /log writes photorec.log; /d sets the recovery dir; /cmd runs
    // non-interactively. partition_none treats the given device as one region.
    QString cmd = "partition_none";
    if (!enableIds.isEmpty()) {
        // Turn everything off, then enable only the chosen families.
        cmd += ",fileopt,everything,disable";
        for (const QString &id : enableIds)
            if (QRegularExpression("^[a-z0-9]+$").match(id).hasMatch())
                cmd += "," + id + ",enable";
    }
    cmd += ",search";
    return {"/log", "/d", recupBase(destDir), "/cmd", source, cmd};
}

int RecoveryTool::organizeByType(const QString &destDir)
{
    int moved = 0;
    const QDir base(destDir);
    const QString byType = base.filePath("by-type");
    for (const QString &d : base.entryList({"gt-stacer-recovery.*"},
                                           QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDirIterator it(base.filePath(d), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo fi(path);
            QString ext = fi.suffix().toLower();
            if (ext.isEmpty()) ext = "no-extension";
            const QString outDir = byType + '/' + ext;
            QDir().mkpath(outDir);
            QString target = outDir + '/' + fi.fileName();
            for (int n = 1; QFile::exists(target); ++n)   // avoid clobbering
                target = QString("%1/%2_%3.%4").arg(outDir, fi.completeBaseName())
                             .arg(n).arg(fi.suffix());
            if (QFile::rename(path, target)) ++moved;
        }
    }
    return moved;
}
