#include "disk_image_tool.h"
#include "../Utils/command_util.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QThread>

namespace {

bool at(const QByteArray &b, int off, const QByteArray &sig)
{
    return b.size() >= off + sig.size() && b.mid(off, sig.size()) == sig;
}

// A synthetic failure for refusals decided here (not by a command).
CommandResult refusal(const QString &why)
{
    CommandResult r;
    r.program = "udisksctl";
    r.started = true;
    r.exitCode = 1;
    r.output = why;
    return r;
}

QString loopName(const QString &dev) { return dev.mid(dev.lastIndexOf('/') + 1); }

bool loopExists(const QString &loopDev)
{
    // A detached loop has no backing file any more.
    return QFile::exists("/sys/block/" + loopName(loopDev) + "/loop/backing_file");
}

// Parse lsblk -J for one loop device into the parts that carry a filesystem.
QVector<DiskImageTool::Part> partsOf(const QString &loopDev)
{
    QVector<DiskImageTool::Part> out;
    const QString js = CommandUtil::execProgramOutput(
        "lsblk", {"-J", "-b", "-o", "PATH,FSTYPE,LABEL,SIZE,MOUNTPOINTS", loopDev}, 8000);
    const QJsonArray devs = QJsonDocument::fromJson(js.toUtf8()).object().value("blockdevices").toArray();
    std::function<void(const QJsonObject &)> walk = [&](const QJsonObject &o) {
        DiskImageTool::Part p;
        p.device = o.value("path").toString();
        p.fsType = o.value("fstype").toString();
        p.label  = o.value("label").toString();
        p.size   = o.value("size").toVariant().toLongLong();
        for (const QJsonValue &m : o.value("mountpoints").toArray())
            if (!m.toString().isEmpty()) { p.mountPoint = m.toString(); break; }
        if (!p.fsType.isEmpty()) out << p;
        for (const QJsonValue &c : o.value("children").toArray()) walk(c.toObject());
    };
    for (const QJsonValue &d : devs) walk(d.toObject());
    return out;
}

// Back-files that are the system's business, not the user's (snap packages,
// live-system images …) — never offered for unmounting here.
bool systemBackFile(const QString &f)
{
    return f.startsWith("/var/lib/snapd/") || f.startsWith("/snap/")
        || f.startsWith("/run/live/") || f.startsWith("/lib/live/")
        || f.startsWith("/var/lib/flatpak/") || f.isEmpty();
}

} // namespace

DiskImageTool::Info DiskImageTool::inspect(const QString &path)
{
    Info info;
    QFile f(path);
    info.size = QFileInfo(path).size();
    if (!f.open(QIODevice::ReadOnly)) return info;
    const QByteArray h = f.read(0x10100);            // enough for every signature below
    if (h.isEmpty()) { info.kind = Kind::Empty; return info; }

    // Compressed / container formats first — they can't be mounted as-is.
    struct Sig { int off; QByteArray magic; const char *name; Kind kind; };
    static const QVector<Sig> sigs = {
        {0, QByteArray("\xFD" "7zXZ\x00", 6),        "xz",    Kind::Compressed},
        {0, QByteArray("\x1F\x8B", 2),               "gzip",  Kind::Compressed},
        {0, QByteArray("\x28\xB5\x2F\xFD", 4),       "zstd",  Kind::Compressed},
        {0, QByteArray("BZh"),                       "bzip2", Kind::Compressed},
        {0, QByteArray("PK\x03\x04", 4),             "zip",   Kind::Compressed},
        {0, QByteArray("7z\xBC\xAF\x27\x1C", 6),     "7z",    Kind::Compressed},
        {0, QByteArray("QFI\xFB", 4),                "qcow2", Kind::VirtualDisk},
        {0, QByteArray("KDMV"),                      "VMDK",  Kind::VirtualDisk},
        {0, QByteArray("# Disk DescriptorFile"),     "VMDK",  Kind::VirtualDisk},
        {0, QByteArray("<<< Oracle VM VirtualBox"),  "VDI",   Kind::VirtualDisk},
        {0, QByteArray("vhdxfile"),                  "VHDX",  Kind::VirtualDisk},
        {0, QByteArray("conectix"),                  "VHD",   Kind::VirtualDisk},
    };
    for (const Sig &s : sigs)
        if (at(h, s.off, s.magic)) { info.kind = s.kind; info.detail = s.name; return info; }

    // Optical images. Hybrid ISOs (most Linux installers) also carry an MBR/GPT;
    // the ISO view is the useful one, so it wins.
    if (at(h, 0x8001, "CD001")) { info.kind = Kind::Iso; info.detail = "ISO 9660"; return info; }
    for (int off = 0x8001; off <= 0x9801; off += 0x800)
        if (at(h, off, "NSR02") || at(h, off, "NSR03") || at(h, off, "BEA01")) {
            info.kind = Kind::Udf; info.detail = "UDF"; return info;
        }

    // Bare filesystems (checked before MBR: a FAT/NTFS boot sector also ends in 55AA).
    if (at(h, 0x438, QByteArray("\x53\xEF", 2)))        { info.kind = Kind::Filesystem; info.detail = "ext2/3/4"; return info; }
    if (at(h, 3, "NTFS    "))                            { info.kind = Kind::Filesystem; info.detail = "NTFS"; return info; }
    if (at(h, 3, "EXFAT   "))                            { info.kind = Kind::Filesystem; info.detail = "exFAT"; return info; }
    if (at(h, 0x52, "FAT32") || at(h, 0x36, "FAT1"))     { info.kind = Kind::Filesystem; info.detail = "FAT"; return info; }
    if (at(h, 0, "hsqs"))                                { info.kind = Kind::Filesystem; info.detail = "squashfs"; return info; }
    if (at(h, 0, "XFSB"))                                { info.kind = Kind::Filesystem; info.detail = "XFS"; return info; }
    if (at(h, 0x10040, "_BHRfS_M"))                      { info.kind = Kind::Filesystem; info.detail = "Btrfs"; return info; }
    if (at(h, 0x400, QByteArray("\x10\x20\xF5\xF2", 4))) { info.kind = Kind::Filesystem; info.detail = "F2FS"; return info; }

    // Partition tables.
    if (at(h, 512, "EFI PART") || at(h, 4096, "EFI PART")) { info.kind = Kind::PartitionedDisk; info.detail = "GPT"; return info; }
    if (at(h, 510, QByteArray("\x55\xAA", 2))) {
        bool anyPart = false;
        for (int i = 0; i < 4; ++i) if (quint8(h[446 + i * 16 + 4]) != 0) anyPart = true;
        if (anyPart) { info.kind = Kind::PartitionedDisk; info.detail = "MBR"; return info; }
    }

    bool allZero = true;
    for (int i = 0; i < qMin(h.size(), 65536); ++i) if (h[i]) { allZero = false; break; }
    info.kind = allZero ? Kind::Empty : Kind::Unknown;
    return info;
}

QVector<DiskImageTool::Attached> DiskImageTool::attached()
{
    QVector<Attached> out;
    const QString js = CommandUtil::execProgramOutput("losetup", {"-l", "-J", "-O", "NAME,BACK-FILE,RO"}, 8000);
    const QJsonArray loops = QJsonDocument::fromJson(js.toUtf8()).object().value("loopdevices").toArray();
    for (const QJsonValue &v : loops) {
        const QJsonObject o = v.toObject();
        Attached a;
        a.loop = o.value("name").toString();
        a.file = o.value("back-file").toString();
        a.file.remove(QRegularExpression("\\s*\\(deleted\\)$"));
        const QJsonValue ro = o.value("ro");
        a.readOnly = ro.isBool() ? ro.toBool() : (ro.toVariant().toString() == "1");
        if (systemBackFile(a.file)) continue;
        a.parts = partsOf(a.loop);
        out << a;
    }
    return out;
}

CommandResult DiskImageTool::mount(const QString &pathIn, bool readOnly, QStringList *mounted)
{
    const QString path = QFileInfo(pathIn).absoluteFilePath();
    const Info info = inspect(path);
    if (!info.mountable())
        return refusal("not a mountable image: " + info.detail);

    // Already attached (e.g. mounted earlier, or partly)? Reuse that loop device.
    QString loop;
    const QString canonical = QFileInfo(path).canonicalFilePath();
    for (const Attached &a : attached())
        if (QFileInfo(a.file).canonicalFilePath() == canonical) { loop = a.loop; break; }

    bool attachedHere = false;
    if (loop.isEmpty()) {
        QStringList args = {"loop-setup"};
        if (readOnly) args << "-r";
        args << "-f" << path;
        // No --no-user-interaction: if policy ever needs a password (another
        // seat, a remote session), the desktop's polkit agent may ask for it.
        const CommandResult r = CommandUtil::runCommand("udisksctl", args, 60000);
        const auto m = QRegularExpression("as (/dev/loop\\d+)").match(r.output);
        if (!r.ok() || !m.hasMatch()) return r.ok() ? refusal(r.output) : r;
        loop = m.captured(1);
        attachedHere = true;
        // Partitions (loopNp1 …) appear asynchronously.
        CommandUtil::execProgram("udevadm", {"settle", "-t", "5"}, 8000);
    }

    // What to mount: the device itself if it carries a filesystem (ISO, bare
    // fs — a hybrid ISO's partitions are then views of the same data), else
    // every partition that has one.
    QVector<Part> parts = partsOf(loop);
    for (int tries = 0; parts.isEmpty() && tries < 10; ++tries) {   // udev may still be probing
        QThread::msleep(300);
        parts = partsOf(loop);
    }
    // Which filesystems to mount:
    //  • no partitions → the device itself (a plain ISO, a filesystem image);
    //  • partitions → UDisks2 exposes NO filesystem on the whole device, even
    //    when it carries one: a HYBRID ISO (most Linux installers — e.g.
    //    CachyOS: loop0 iso9660 + loop0p1 iso9660 + loop0p2 vfat EFI) has to be
    //    mounted through its ISO/UDF partition, which is the whole disc;
    //  • partitions, none of them ISO/UDF → every one (Raspberry Pi images …).
    QVector<Part> targets, partitions;
    for (const Part &p : parts) if (p.device != loop) partitions << p;
    if (partitions.isEmpty()) {
        for (const Part &p : parts) if (p.device == loop) targets << p;
    } else {
        for (const Part &p : partitions) if (p.fsType == "iso9660" || p.fsType == "udf") targets << p;
        if (targets.isEmpty()) targets = partitions;
    }
    if (targets.isEmpty()) {
        if (attachedHere) CommandUtil::execProgram("udisksctl", {"loop-delete", "-b", loop}, 15000);
        return refusal("wrong fs type: no readable filesystem found in the image");
    }

    CommandResult firstFailure;
    bool anyFailed = false;
    for (const Part &p : targets) {
        if (!p.mountPoint.isEmpty()) { if (mounted) *mounted << p.mountPoint; continue; }
        const CommandResult r = CommandUtil::runCommand("udisksctl", {"mount", "-b", p.device}, 60000);
        const auto m = QRegularExpression("Mounted \\S+ at (.+?)\\.?\\s*$", QRegularExpression::MultilineOption)
                           .match(r.output);
        if (r.ok() && m.hasMatch()) { if (mounted) *mounted << m.captured(1).trimmed(); }
        else if (!anyFailed) { anyFailed = true; firstFailure = r.ok() ? refusal(r.output) : r; }
    }
    if (anyFailed && mounted && mounted->isEmpty()) {
        // Nothing usable — don't leave a dangling loop device behind.
        if (attachedHere) CommandUtil::execProgram("udisksctl", {"loop-delete", "-b", loop}, 15000);
        return firstFailure;
    }
    if (anyFailed) return firstFailure;          // partly mounted; caller reports both
    CommandResult ok;
    ok.program = "udisksctl"; ok.started = true; ok.exitCode = 0;
    return ok;
}

CommandResult DiskImageTool::unmount(const Attached &img)
{
    // Partitions first — detaching with mounts still active "succeeds" but
    // leaves everything attached and mounted.
    for (const Part &p : partsOf(img.loop)) {
        if (p.mountPoint.isEmpty()) continue;
        const CommandResult r = CommandUtil::runCommand("udisksctl", {"unmount", "-b", p.device}, 60000);
        if (!r.ok()) return r;
    }
    if (loopExists(img.loop)) {
        const CommandResult r = CommandUtil::runCommand("udisksctl", {"loop-delete", "-b", img.loop}, 30000);
        // Already auto-cleared after the last unmount → not an error.
        if (!r.ok() && loopExists(img.loop)) return r;
    }
    CommandResult ok;
    ok.program = "udisksctl"; ok.started = true; ok.exitCode = 0;
    return ok;
}

QStringList DiskImageTool::holders(const QString &mountPoint)
{
    QStringList out;
    if (mountPoint.isEmpty() || !CommandUtil::commandExists("fuser")) return out;
    // fuser prints the PIDs on stdout (the path and access letters on stderr).
    const QString pids = CommandUtil::execProgramOutput("fuser", {"-m", mountPoint}, 8000);
    const qint64 self = QCoreApplication::applicationPid();
    QSet<QString> seen;
    for (const QString &t : pids.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
        const QString pid = t;
        if (!QRegularExpression("^\\d+$").match(pid).hasMatch() || pid.toLongLong() == self) continue;
        QFile c("/proc/" + pid + "/comm");
        QString name = c.open(QIODevice::ReadOnly) ? QString::fromUtf8(c.readAll()).trimmed() : QString();
        if (name.isEmpty()) continue;                      // already gone
        const QString entry = QString("%1 (pid %2)").arg(name, pid);
        if (!seen.contains(entry)) { seen.insert(entry); out << entry; }
    }
    return out;
}

QStringList DiskImageTool::isoExtractArgv(const QString &image, const QString &dest, bool overwrite, bool udf)
{
    for (const char *z : {"7z", "7zz", "7za"})
        if (CommandUtil::commandExists(z))
            // -aoa = overwrite files with the same name; -aos = keep existing
            // (real skip — GT-IMT used -aou, which RENAMES and duplicates).
            // Nothing else in the folder is ever touched. "--" ends switches.
            return {z, "x", udf ? "-tudf" : "-tiso", "-y", "-bsp1", "-bso0", overwrite ? "-aoa" : "-aos",
                    "-o" + dest, "--", image};
    if (CommandUtil::commandExists("bsdtar")) {
        QStringList a = {"bsdtar", "-x"};
        if (!overwrite) a << "-k";
        return a + QStringList{"-f", image, "-C", dest};
    }
    if (CommandUtil::commandExists("xorriso"))
        return {"xorriso", "-osirrox", "on", "-indev", image, "-extract", "/", dest};
    return {};
}

QStringList DiskImageTool::copyArgv(const QString &mountPoint, const QString &dest, bool overwrite)
{
    if (CommandUtil::commandExists("rsync")) {
        QStringList a = {"rsync", "-rlt", "--info=progress2"};
        if (!overwrite) a << "--ignore-existing";
        return a + QStringList{mountPoint + "/", dest + "/"};
    }
    QStringList a = {"cp", "-R"};
    if (!overwrite) a << "-n";
    return a + QStringList{mountPoint + "/.", dest};
}

// ── Optical discs ───────────────────────────────────────────────────────

namespace {

// `udisksctl info` → { "MediaAvailable": "true", … } (first line of each key).
QHash<QString, QString> udisksProps(const QStringList &args)
{
    QHash<QString, QString> out;
    const QString text = CommandUtil::execProgramOutput("udisksctl", args, 8000);
    // A list value continues on the next lines, value only:
    //     MediaCompatibility:  optical_cd
    //                          optical_dvd_r       → "optical_cd optical_dvd_r"
    static const QRegularExpression line("^\\s+(\\w+):\\s*(.*)$");
    QString last;
    for (const QString &l : text.split('\n')) {
        const auto m = line.match(l);
        if (m.hasMatch()) {
            last = out.contains(m.captured(1)) ? QString() : m.captured(1);
            if (!last.isEmpty()) out.insert(last, m.captured(2).trimmed());
        } else if (!last.isEmpty() && !l.trimmed().isEmpty() && l.startsWith("    ")) {
            out[last] = (out.value(last) + ' ' + l.trimmed()).trimmed();
        } else {
            last.clear();
        }
    }
    return out;
}

// "optical_dvd_plus_r_dl" → "DVD+R DL", "optical_bd_re" → "BD-RE".
QString prettyMedia(QString m)
{
    if (!m.startsWith("optical_")) return {};
    m = m.mid(8).toUpper();
    m.replace("_PLUS_", "+");
    m.replace("_DL", " DL");
    m.replace('_', '-');
    if (m == "BD") return QStringLiteral("Blu-ray");
    if (m.startsWith("MRW")) return QStringLiteral("CD-MRW");
    return m;
}

} // namespace

QVector<DiskImageTool::OpticalDrive> DiskImageTool::opticalDrives()
{
    QVector<OpticalDrive> out;
    const QString js = CommandUtil::execProgramOutput(
        "lsblk", {"-J", "-b", "-d", "-o", "PATH,TYPE,MODEL,SIZE,FSTYPE,LABEL,MOUNTPOINTS"}, 8000);
    const QJsonArray devs = QJsonDocument::fromJson(js.toUtf8()).object().value("blockdevices").toArray();
    const bool haveUdisks = CommandUtil::commandExists("udisksctl");
    for (const QJsonValue &v : devs) {
        const QJsonObject o = v.toObject();
        if (o.value("type").toString() != "rom") continue;
        OpticalDrive d;
        d.device = o.value("path").toString();
        d.model  = o.value("model").toString().trimmed();
        d.fsType = o.value("fstype").toString();
        d.label  = o.value("label").toString();
        for (const QJsonValue &m : o.value("mountpoints").toArray())
            if (!m.toString().isEmpty()) { d.mountPoint = m.toString(); break; }

        if (haveUdisks) {
            const auto block = udisksProps({"info", "-b", d.device});
            d.size = block.value("Size").toLongLong();
            QString drive = block.value("Drive");
            drive.remove('\'');
            drive.remove("/org/freedesktop/UDisks2/");
            if (drive.startsWith("drives/")) {
                const auto p = udisksProps({"info", "-p", drive});
                d.media       = p.value("MediaAvailable") == "true";
                d.blank       = p.value("OpticalBlank") == "true";
                d.audioTracks = p.value("OpticalNumAudioTracks").toInt();
                d.dataTracks  = p.value("OpticalNumDataTracks").toInt();
                d.sessions    = p.value("OpticalNumSessions").toInt();
                d.mediaType   = prettyMedia(p.value("Media"));
                // A writer lists recordable types: optical_cd_r, optical_dvd_plus_rw …
                static const QRegularExpression writable("^optical_(?:(?:cd|dvd|bd|hddvd)_|mrw)");
                for (const QString &t : p.value("MediaCompatibility").split(' ', Qt::SkipEmptyParts))
                    if (writable.match(t).hasMatch()) { d.canWrite = true; break; }
                static const QRegularExpression rw("(?:_rw|_re|_ram|_rw_dl|mrw.*)$");
                d.rewritable = rw.match(p.value("Media")).hasMatch();
            } else {
                d.media = d.size > 0;
            }
        } else {
            // No udisks: a recognised filesystem is the only reliable sign of a
            // disc (the kernel reports a fake size for an empty drive).
            d.media = !d.fsType.isEmpty();
            if (d.media) d.size = o.value("size").toVariant().toLongLong();
        }
        if (!d.media) { d.size = 0; d.fsType.clear(); d.label.clear(); }
        // Ask the host (Flatpak can't see /dev) whether we may read it.
        d.readable = CommandUtil::execProgram("test", {"-r", d.device}, 5000) == 0;
        out << d;
    }
    return out;
}

qint64 DiskImageTool::opticalImageSize(const OpticalDrive &d)
{
    if (d.size <= 0) return 0;
    if (d.fsType != "iso9660" || d.sessions > 1) return d.size;   // UDF / multisession: whole disc
    // Primary Volume Descriptor = sector 16: "CD001" at +1, volume space size
    // (LE 32-bit) at +80, logical block size (LE 16-bit) at +128.
    const QString hex = CommandUtil::execProgramOutput(
        "od", {"-A", "n", "-v", "-t", "x1", "-j", "32768", "-N", "136", d.device}, 15000);
    QByteArray pvd;
    for (const QString &b : hex.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts))
        pvd.append(char(b.toUInt(nullptr, 16)));
    if (pvd.size() < 136 || pvd.mid(1, 5) != "CD001" || quint8(pvd[0]) != 1) return d.size;
    auto le = [&](int off, int n) { quint64 v = 0; for (int i = n - 1; i >= 0; --i) v = (v << 8) | quint8(pvd[off + i]); return v; };
    const qint64 bytes = qint64(le(80, 4)) * qint64(le(128, 2));
    // Trust it only if it is sane: inside the disc and a whole number of sectors.
    if (bytes <= 0 || bytes > d.size || bytes % 2048) return d.size;
    return bytes;
}

QStringList DiskImageTool::discCopyArgv(const QString &device, const QString &dest, qint64 bytes)
{
    // 2048 = a CD/DVD sector; 32 sectors per read keeps the drive streaming.
    // count_bytes: copy exactly `bytes`, never into the run-out area.
    return {"dd", "if=" + device, "of=" + dest, "bs=64K", "iflag=count_bytes",
            "count=" + QString::number(bytes), "status=progress"};
}

DiskImageTool::BurnMedia DiskImageTool::burnMedia(const QString &device)
{
    BurnMedia m;
    // -tell_media_space prints the drive/media report and the free blocks.
    const CommandResult r = CommandUtil::runCommand("xorriso", {"-outdev", device, "-tell_media_space"}, 90000);
    m.output = r.output;
    static const QRegularExpression cur("^Media current:\\s*(.+)$", QRegularExpression::MultilineOption);
    static const QRegularExpression st("^Media status :\\s*(.+)$", QRegularExpression::MultilineOption);
    static const QRegularExpression sp("^Media space\\s*:\\s*(\\d+)s", QRegularExpression::MultilineOption);
    const auto c = cur.match(r.output), s = st.match(r.output), f = sp.match(r.output);
    if (!c.hasMatch() || !s.hasMatch()) return m;          // no drive / no disc / xorriso failed
    m.ok = true;
    m.media = c.captured(1).trimmed();
    const QString status = s.captured(1);
    m.blank      = status.contains("is blank");
    m.appendable = status.contains("is appendable");
    m.closed     = status.contains("is closed");
    if (f.hasMatch()) m.freeBytes = f.captured(1).toLongLong() * 2048;
    return m;
}

QStringList DiskImageTool::burnArgv(const QString &device, const QString &image, bool blankFirst)
{
    // -dao: one closed track of exactly the image (no run-out blocks on CD);
    // xorriso picks the right write type for DVD/BD itself.
    QStringList a = {"xorriso", "-as", "cdrecord", "-v", "dev=" + device, "-dao"};
    if (blankFirst) a << "blank=as_needed";
    // NOT "--": in xorriso it ends the cdrecord emulation. An absolute path
    // starts with '/', so it can never be taken for an option.
    a << QFileInfo(image).absoluteFilePath();
    return a;
}

QStringList DiskImageTool::verifyArgv(const QString &image, const QString &device, qint64 bytes)
{
    return {"cmp", "-n", QString::number(bytes), "--", image, device};
}
