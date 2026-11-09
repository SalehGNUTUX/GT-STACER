#include "removable_tool.h"
#include "../Utils/command_util.h"
#include "../Utils/format_util.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <functional>
#include <algorithm>

namespace {

const char *kService = "org.freedesktop.UDisks2";
const char *kBlock   = "org.freedesktop.UDisks2.Block";
const char *kFs      = "org.freedesktop.UDisks2.Filesystem";
const char *kTable   = "org.freedesktop.UDisks2.PartitionTable";

constexpr int kQuickMs = 60 * 1000;          // mount / unmount
constexpr int kLongMs  = 6 * 60 * 60 * 1000; // check / repair / format of a big card

// /dev/sdb1 → /org/freedesktop/UDisks2/block_devices/sdb1 (UDisks escapes every
// byte outside [A-Za-z0-9] as _xx).
QString objectPath(const QString &device)
{
    const QByteArray name = device.mid(device.lastIndexOf('/') + 1).toUtf8();
    QString out = "/org/freedesktop/UDisks2/block_devices/";
    for (char ch : name) {
        const uchar c = uchar(ch);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out += QChar(c);
        else out += QString("_%1").arg(c, 2, 16, QChar('0'));
    }
    return out;
}

CommandResult fromReply(const QDBusMessage &reply)
{
    CommandResult r;
    r.program = "udisks";
    r.started = true;
    if (reply.type() == QDBusMessage::ErrorMessage) {
        r.exitCode = 1;
        r.output = reply.errorName() + ": " + reply.errorMessage();
        if (reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.NoReply")) r.timedOut = true;
        if (reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown"))
            r.output += "\nudisks2: not found";            // → "not installed" explanation
    } else {
        r.exitCode = 0;
    }
    return r;
}

// A UDisks2 method call that may ask for the password through the desktop's
// polkit agent (interactive authorization allowed).
QDBusMessage call(const QString &device, const char *iface, const char *method,
                  const QVariantList &args, int timeoutMs)
{
    QDBusMessage m = QDBusMessage::createMethodCall(kService, objectPath(device), iface, method);
    m.setArguments(args);
    m.setInteractiveAuthorizationAllowed(true);
    return QDBusConnection::systemBus().call(m, QDBus::Block, timeoutMs);
}

// Anything mounted on `device` or below it (its partitions)? UDisks2 2.10
// refuses a check of a mounted filesystem, but a Format of a mounted partition
// made the daemon itself crash and restart (verified on Debian 13, 2.10.1) —
// so the core refuses both before UDisks2 is ever asked.
QStringList mountsUnder(const QString &device)
{
    QStringList out;
    const QString js = CommandUtil::execProgramOutput("lsblk", {"-J", "-o", "PATH,MOUNTPOINTS", device}, 8000);
    std::function<void(const QJsonObject &)> walk = [&](const QJsonObject &o) {
        for (const QJsonValue &m : o.value("mountpoints").toArray())
            if (!m.toString().isEmpty()) out << m.toString();
        for (const QJsonValue &c : o.value("children").toArray()) walk(c.toObject());
    };
    for (const QJsonValue &d : QJsonDocument::fromJson(js.toUtf8()).object().value("blockdevices").toArray())
        walk(d.toObject());
    return out;
}

CommandResult refuseMounted(const QStringList &mounts)
{
    CommandResult r;
    r.program = "udisks";
    r.started = true;
    r.exitCode = 1;
    r.output = "target is busy: still mounted at " + mounts.join(", ");
    return r;
}

bool isRemovableTransport(const QString &tran)
{
    return tran == "usb" || tran == "mmc" || tran == "sdio" || tran == "ieee1394" || tran == "memstick";
}

bool boolField(const QJsonValue &v)
{
    return v.isBool() ? v.toBool() : (v.toVariant().toString() == "1" || v.toVariant().toString() == "true");
}

QString partitionTypeFor(const QString &fs)
{
    if (fs == "vfat")                                    return "0x0c";   // W95 FAT32 (LBA)
    if (fs == "exfat" || fs == "ntfs" || fs == "udf")    return "0x07";   // HPFS/NTFS/exFAT (also used for UDF)
    return "0x83";                                                        // Linux
}

} // namespace

QString RemovableTool::Disk::title() const
{
    QString name = QString(vendor + " " + model).simplified();
    if (name.isEmpty()) name = device.mid(device.lastIndexOf('/') + 1);
    return QString("%1 (%2)").arg(name, FormatUtil::formatBytes(size));
}

QVector<RemovableTool::Disk> RemovableTool::disks()
{
    QVector<Disk> out;
    const QString js = CommandUtil::execProgramOutput("lsblk",
        {"-J", "-b", "-o", "PATH,PKNAME,TYPE,TRAN,RM,HOTPLUG,RO,SIZE,MODEL,VENDOR,SERIAL,FSTYPE,LABEL,"
                           "PTTYPE,MOUNTPOINTS,FSUSED"}, 8000);

    // lsblk prints a TREE for some column sets and a FLAT list for others
    // (with this set it is flat: sdb1 next to sdb, not inside it). Don't rely
    // on either: flatten everything, then rebuild parent → children from
    // PKNAME (falling back to the tree nesting when PKNAME is empty).
    QVector<QJsonObject> nodes;
    QHash<QString, QString> parentOf;                 // path → parent path
    std::function<void(const QJsonObject &, const QString &)> flatten =
        [&](const QJsonObject &o, const QString &treeParent) {
            const QString path = o.value("path").toString();
            nodes << o;
            const QString pk = o.value("pkname").toString();
            if (!pk.isEmpty()) parentOf.insert(path, "/dev/" + pk);
            else if (!treeParent.isEmpty()) parentOf.insert(path, treeParent);
            for (const QJsonValue &c : o.value("children").toArray()) flatten(c.toObject(), path);
        };
    for (const QJsonValue &v : QJsonDocument::fromJson(js.toUtf8()).object().value("blockdevices").toArray())
        flatten(v.toObject(), QString());
    // Each path once (a tree can list a holder, e.g. LVM, under several parents).
    QHash<QString, QJsonObject> byPath;
    for (const QJsonObject &o : nodes) byPath.insert(o.value("path").toString(), o);
    auto descendsFrom = [&](QString path, const QString &disk) {
        for (int guard = 0; guard < 16 && parentOf.contains(path); ++guard) {
            path = parentOf.value(path);
            if (path == disk) return true;
        }
        return false;
    };

    for (const QJsonObject &d : nodes) {
        if (d.value("type").toString() != "disk") continue;          // never loop/rom/zram parts
        const QString path = d.value("path").toString();
        if (path.contains("/zram") || path.contains("/loop") || path.contains("/nbd")) continue;
        if (std::any_of(out.cbegin(), out.cend(), [&](const Disk &x){ return x.device == path; })) continue;
        const QString tran = d.value("tran").toString();
        // Removable = attached over USB / a card reader, or flagged removable /
        // hot-pluggable by the kernel. An internal SATA/NVMe disk is never listed.
        if (!isRemovableTransport(tran) && !boolField(d.value("rm")) && !boolField(d.value("hotplug")))
            continue;
        if (d.value("size").toVariant().toLongLong() <= 0) continue;  // empty card reader slot

        Disk disk;
        disk.device    = path;
        disk.model     = d.value("model").toString().trimmed();
        disk.vendor    = d.value("vendor").toString().trimmed();
        disk.serial    = d.value("serial").toString().trimmed();
        disk.transport = tran;
        disk.tableType = d.value("pttype").toString();
        disk.size      = d.value("size").toVariant().toLongLong();
        disk.readOnly  = boolField(d.value("ro"));

        // The disk itself and everything below it (partitions, LUKS, LVM …):
        // any mount outside the removable-media areas, or swap, locks the disk.
        QVector<QJsonObject> tree{d};
        for (auto it = byPath.cbegin(); it != byPath.cend(); ++it)
            if (descendsFrom(it.key(), path)) tree << it.value();
        for (const QJsonObject &o : tree) {
            for (const QJsonValue &m : o.value("mountpoints").toArray()) {
                const QString mp = m.toString();
                if (mp.isEmpty()) continue;
                if (mp == "[SWAP]") { disk.protectedReason = kProtSwap; continue; }
                const bool mediaArea = mp.startsWith("/media/") || mp.startsWith("/run/media/")
                                    || mp.startsWith("/mnt/") || mp == "/mnt";
                if (!mediaArea && disk.protectedReason.isEmpty()) disk.protectedReason = kProtSystem;
            }
            const QString type = o.value("type").toString();
            const bool self = o.value("path").toString() == path;
            if (type == "part" || (self && !o.value("fstype").toString().isEmpty())) {
                Part p;
                p.device = o.value("path").toString();
                p.fsType = o.value("fstype").toString();
                p.label  = o.value("label").toString();
                p.size   = o.value("size").toVariant().toLongLong();
                for (const QJsonValue &m : o.value("mountpoints").toArray())
                    if (!m.toString().isEmpty()) { p.mountPoint = m.toString(); break; }
                const QJsonValue used = o.value("fsused");
                if (!used.isNull() && !used.toVariant().toString().isEmpty()) p.used = used.toVariant().toLongLong();
                disk.parts << p;
            }
        }
        std::sort(disk.parts.begin(), disk.parts.end(), [](const Part &a, const Part &b) {
            return a.device.size() != b.device.size() ? a.device.size() < b.device.size() : a.device < b.device;
        });
        out << disk;
    }
    return out;
}

QString RemovableTool::verifyTarget(const QString &device, const QString &serial, qint64 size)
{
    for (const Disk &d : disks()) {
        bool owns = d.device == device;
        for (const Part &p : d.parts) if (p.device == device) owns = true;
        if (!owns) continue;
        if (d.serial != serial || d.size != size) return "changed";
        if (!d.protectedReason.isEmpty())         return d.protectedReason;
        if (d.readOnly)                           return "readonly";
        return {};
    }
    return "gone";
}

CommandResult RemovableTool::mount(const QString &device, QString *mountPoint)
{
    const QDBusMessage reply = call(device, kFs, "Mount", {QVariantMap()}, kQuickMs);
    if (mountPoint && reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
        *mountPoint = reply.arguments().first().toString();
    return fromReply(reply);
}

CommandResult RemovableTool::unmount(const QString &device)
{
    return fromReply(call(device, kFs, "Unmount", {QVariantMap()}, kQuickMs));
}

CommandResult RemovableTool::check(const QString &device, bool *consistent)
{
    if (const QStringList m = mountsUnder(device); !m.isEmpty()) return refuseMounted(m);
    const QDBusMessage reply = call(device, kFs, "Check", {QVariantMap()}, kLongMs);
    if (consistent && reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
        *consistent = reply.arguments().first().toBool();
    return fromReply(reply);
}

CommandResult RemovableTool::repair(const QString &device)
{
    if (const QStringList m = mountsUnder(device); !m.isEmpty()) return refuseMounted(m);
    const QDBusMessage reply = call(device, kFs, "Repair", {QVariantMap()}, kLongMs);
    CommandResult r = fromReply(reply);
    // Repair returns false when the tool could not fix everything.
    if (r.ok() && !reply.arguments().isEmpty() && !reply.arguments().first().toBool()) {
        r.exitCode = 1;
        r.output = "repair: the filesystem could not be fully repaired";
    }
    return r;
}

CommandResult RemovableTool::formatPartition(const QString &device, const QString &fsType, const QString &label)
{
    if (const QStringList m = mountsUnder(device); !m.isEmpty()) return refuseMounted(m);
    QVariantMap opts;
    if (!label.isEmpty()) opts.insert("label", label);
    opts.insert("update-partition-type", true);     // MBR type follows the new filesystem
    if (QStringList{"ext2", "ext3", "ext4", "btrfs", "xfs", "f2fs", "nilfs2", "udf"}.contains(fsType))
        opts.insert("take-ownership", true);         // the new root folder belongs to the user
    return fromReply(call(device, kBlock, "Format", {fsType, opts}, kLongMs));
}

CommandResult RemovableTool::formatDisk(const QString &disk, const QString &fsType, const QString &label)
{
    if (const QStringList m = mountsUnder(disk); !m.isEmpty()) return refuseMounted(m);
    // 1) A fresh MBR partition table (wipes the old table and signatures).
    CommandResult r = fromReply(call(disk, kBlock, "Format", {QStringLiteral("dos"), QVariantMap()}, kLongMs));
    if (!r.ok()) return r;
    // 2) One partition over the whole disk, of the right MBR type, formatted.
    QVariantMap fsOpts;
    if (!label.isEmpty()) fsOpts.insert("label", label);
    if (QStringList{"ext2", "ext3", "ext4", "btrfs", "xfs", "f2fs", "nilfs2", "udf"}.contains(fsType))
        fsOpts.insert("take-ownership", true);
    QDBusMessage m = QDBusMessage::createMethodCall(kService, objectPath(disk), kTable, "CreatePartitionAndFormat");
    m.setArguments({QVariant::fromValue<qulonglong>(0), QVariant::fromValue<qulonglong>(0),
                    partitionTypeFor(fsType), QString(), QVariantMap(), fsType, fsOpts});
    m.setInteractiveAuthorizationAllowed(true);
    return fromReply(QDBusConnection::systemBus().call(m, QDBus::Block, kLongMs));
}

QVector<RemovableTool::FsOption> RemovableTool::formatOptions()
{
    static const struct { const char *fs; const char *pkg; } all[] = {
        {"vfat", "dosfstools"}, {"exfat", "exfatprogs"}, {"ntfs", "ntfs-3g"},
        {"ext4", "e2fsprogs"}, {"f2fs", "f2fs-tools"}, {"btrfs", "btrfs-progs"},
        {"xfs", "xfsprogs"}, {"ext3", "e2fsprogs"}, {"ext2", "e2fsprogs"},
        {"udf", "udftools"}, {"nilfs2", "nilfs-tools"}};
    QVector<FsOption> out;
    for (const auto &a : all) {
        FsOption o;
        o.fs = a.fs;
        o.package = a.pkg;
        QDBusMessage m = QDBusMessage::createMethodCall(kService, "/org/freedesktop/UDisks2/Manager",
                                                        "org.freedesktop.UDisks2.Manager", "CanFormat");
        m.setArguments({o.fs});
        const QDBusMessage reply = QDBusConnection::systemBus().call(m, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();
            arg.beginStructure();
            arg >> o.available >> o.missingTool;
            arg.endStructure();
        } else {
            o.missingTool = "udisks2";
        }
        out << o;
    }
    return out;
}

QStringList RemovableTool::formatTypes()
{
    QStringList out;
    for (const FsOption &o : formatOptions()) if (o.available) out << o.fs;
    return out;
}

RemovableTool::Advice RemovableTool::advise(const Disk &disk, qint64 targetSize)
{
    Advice a;
    const QString name = (disk.vendor + " " + disk.model).toLower();
    // A memory card: the kernel's own MMC/SD driver, or a USB card reader
    // (their models read "SD/MMC", "Card Reader", "SD Card" …).
    a.sdCard = disk.transport == "mmc" || disk.transport == "sdio" || disk.device.contains("mmcblk")
            || name.contains("card") || name.contains("sd/mmc") || name.contains(" sd")
            || name.startsWith("sd") || name.contains("mmc") || name.contains("reader");
    const qint64 gb32 = qint64(32) * 1000 * 1000 * 1000 + qint64(512) * 1024 * 1024;   // "32 GB" cards are a bit under 32 GiB
    const qint64 big  = qint64(256) * 1000 * 1000 * 1000;
    if (a.sdCard)                  { a.fs = targetSize <= gb32 ? "vfat" : "exfat"; a.why = targetSize <= gb32 ? "sd-small" : "sd-large"; }
    else if (targetSize <= gb32)   { a.fs = "vfat";  a.why = "usb-small"; }
    else if (targetSize < big)     { a.fs = "exfat"; a.why = "usb-large"; }
    else                           { a.fs = "exfat"; a.why = "drive"; }
    return a;
}

qint64 RemovableTool::minSize(const QString &fs)
{
    const qint64 mib = 1024 * 1024;
    if (fs == "xfs")   return 300 * mib;    // mkfs.xfs refuses smaller filesystems
    if (fs == "btrfs") return 114 * mib;    // mkfs.btrfs minimum (single profile)
    if (fs == "f2fs")  return 64 * mib;
    if (fs == "nilfs2")return 128 * mib;
    if (fs == "ntfs")  return 2 * mib;
    return 0;
}

QString RemovableTool::partitionType(const QString &fs) { return partitionTypeFor(fs); }

int RemovableTool::maxLabelLength(const QString &fsType)
{
    if (fsType == "vfat")   return 11;
    if (fsType == "exfat")  return 15;
    if (fsType == "ntfs")   return 32;
    if (fsType == "xfs")    return 12;
    if (fsType == "btrfs")  return 255;
    if (fsType == "f2fs")   return 512;
    if (fsType == "udf")    return 126;
    if (fsType == "nilfs2") return 80;
    return 16;                                   // ext2/3/4
}

QString RemovableTool::labelProblem(const QString &fsType, const QString &label)
{
    if (label.isEmpty()) return {};
    // ext*, xfs, btrfs, nilfs2 count bytes (UTF-8); the others count characters.
    const bool bytes = QStringList{"ext2", "ext3", "ext4", "xfs", "btrfs", "nilfs2"}.contains(fsType);
    const int len = bytes ? label.toUtf8().size() : label.size();
    if (len > maxLabelLength(fsType)) return "too-long";
    if (fsType == "vfat") {
        static const QRegularExpression bad("[*?.,;:/\\\\|+=<>\\[\\]\"]");
        if (bad.match(label).hasMatch()) return "bad-chars";
    }
    if (label.contains('/') || label.contains(QChar(0))) return "bad-chars";
    return {};
}

RemovableTool::Health RemovableTool::health(const QString &disk, CommandResult *result)
{
    Health h;
    // -j = JSON. USB bridges often need "-d sat" — tried only if the plain call
    // says the device type is unknown. Both attempts run inside ONE pkexec:
    // two pkexec calls meant two password prompts for one click (26.12, seen
    // on a Kingston DataTraveler). The disk is passed as $1, never spliced
    // into the script text.
    static const QString kScript = QStringLiteral(
        "out=$(smartctl -j -H -i -A \"$1\" 2>&1); rc=$?\n"
        "case \"$out\" in\n"
        "  *'Unknown USB bridge'*|*'specify device type'*)\n"
        "    exec smartctl -j -d sat -H -i -A \"$1\" ;;\n"
        "esac\n"
        "printf '%s\\n' \"$out\"\n"
        "exit $rc\n");
    CommandResult r = CommandUtil::runCommand("pkexec", {"/bin/sh", "-c", kScript, "sh", disk}, 120000);
    if (result) *result = r;

    // smartctl's exit status is a bit mask; JSON is printed in every case.
    const int jsonStart = r.output.indexOf('{');
    const QJsonObject o = QJsonDocument::fromJson(r.output.mid(jsonStart < 0 ? 0 : jsonStart).toUtf8()).object();
    if (o.isEmpty()) return h;
    h.model = o.value("model_name").toString();
    // smartctl RAN and answered (its JSON carries a "smartctl" block): that is
    // a report, not a failure — even with exit status 2 ("device did not
    // return an IDENTIFY DEVICE structure"), which is exactly what a USB stick
    // without SMART returns (verified: Kingston DataTraveler 2.0 via -d sat).
    // Only a missing JSON (password dismissed, smartctl not installed …)
    // stays a failure for the caller to explain.
    if (o.value("smartctl").isObject() && result) result->exitCode = 0;
    const QJsonObject status = o.value("smart_status").toObject();
    if (status.isEmpty()) return h;            // no SMART on this device
    h.supported = true;
    h.passed = status.value("passed").toBool(true);
    h.powerOnHours = o.value("power_on_time").toObject().value("hours").toVariant().toLongLong();
    if (!o.value("power_on_time").isObject()) h.powerOnHours = -1;
    if (o.value("temperature").isObject()) h.temperature = o.value("temperature").toObject().value("current").toInt(-1);
    for (const QJsonValue &a : o.value("ata_smart_attributes").toObject().value("table").toArray()) {
        const QJsonObject at = a.toObject();
        const qint64 raw = at.value("raw").toObject().value("value").toVariant().toLongLong();
        if (at.value("id").toInt() == 5)   h.reallocated = raw;
        if (at.value("id").toInt() == 197) h.pending = raw;
    }
    // A health result was read: report success even if smartctl's bit mask is
    // non-zero (bits for "old errors in the log" etc. are not a failure to run).
    if (result) { result->exitCode = 0; }
    return h;
}

QStringList RemovableTool::surfaceScanArgv(const QString &device)
{
    // Default badblocks mode is READ-ONLY. -s progress, -v verbose, 4 KiB blocks.
    return {"badblocks", "-s", "-v", "-b", "4096", device};
}

QStringList RemovableTool::imageArgv(const QString &device)
{
    return {"dd", "if=" + device, "bs=4M", "status=progress", "iflag=fullblock"};
}
