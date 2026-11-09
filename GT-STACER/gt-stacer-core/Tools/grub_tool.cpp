#include "grub_tool.h"
#include "../Utils/command_util.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>
#include <algorithm>

namespace {

const char *kDefaultFile = "/etc/default/grub";
const char *kBackupDir   = "/var/backups/gt-stacer/grub";

QByteArray readBytes(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
QString readText(const QString &path) { return QString::fromUtf8(readBytes(path)); }

// ── System files, read where they really are ────────────────────────────
// Inside Flatpak the sandbox has its OWN /etc (and /etc/os-release of the
// runtime): /etc/default/grub would read as empty and a "regenerate" would
// write a near-empty file over the real one (found in the 26.12 review). So
// every system path goes through the host there (cat / test / find).
bool onHost() { return CommandUtil::inFlatpak(); }

QString sysRead(const QString &path)
{
    if (!onHost()) return readText(path);
    int ec = -1;
    const QString out = CommandUtil::execProgramC("cat", {"--", path}, 10000, &ec);
    return ec == 0 ? out : QString();
}

// Exact bytes (the transaction's base must match the file byte-for-byte —
// a decode/re-encode round trip would turn one Latin-1 byte into a mismatch).
QByteArray sysReadBytes(const QString &path)
{
    if (!onHost()) return readBytes(path);
    return sysRead(path).toUtf8();       // best effort through the host
}

bool sysExists(const QString &path)
{
    if (!onHost()) return QFileInfo::exists(path);
    int ec = -1;
    CommandUtil::execProgramC("test", {"-e", path}, 5000, &ec);
    return ec == 0;
}

// Names in `dir` (no recursion), sorted; dirsOnly = sub-directories only.
QStringList sysList(const QString &dir, bool dirsOnly)
{
    QStringList out;
    if (!onHost()) {
        out = QDir(dir).entryList((dirsOnly ? QDir::Dirs : QDir::Files) | QDir::NoDotAndDotDot, QDir::Name);
    } else {
        QStringList args = {dir, "-mindepth", "1", "-maxdepth", "1", "-type", dirsOnly ? "d" : "f", "-printf", "%f\\n"};
        int ec = -1;
        const QString o = CommandUtil::execProgramC("find", args, 10000, &ec);
        if (ec == 0) out = o.split('\n', Qt::SkipEmptyParts);
        out.sort();
    }
    return out;
}

QString firstExisting(const QStringList &cmds)
{
    for (const QString &c : cmds) if (CommandUtil::commandExists(c)) return c;
    return {};
}

// Value part of a shell assignment, unquoted WITHOUT expansion:
// "a b" → a b · 'x' → x · bare → up to whitespace/#.
QString shellValue(const QString &v)
{
    QString out;
    int i = 0;
    while (i < v.size()) {
        const QChar c = v[i];
        if (c == '"') {
            ++i;
            while (i < v.size() && v[i] != '"') {
                if (v[i] == '\\' && i + 1 < v.size()) ++i;
                out += v[i++];
            }
            ++i;
        } else if (c == '\'') {
            ++i;
            while (i < v.size() && v[i] != '\'') out += v[i++];
            ++i;
        } else if (c.isSpace() || c == '#' || c == ';') {
            break;
        } else {
            if (c == '\\' && i + 1 < v.size()) ++i;
            out += v[i++];
        }
    }
    return out;
}

// Physical lines joined where one ends with a backslash (a shell line
// continuation): GRUB_CMDLINE_LINUX_DEFAULT="quiet \\⏎ splash" is ONE assignment.
struct Logical { int first; int last; QString text; };
QVector<Logical> logicalLines(const QStringList &lines)
{
    QVector<Logical> out;
    for (int i = 0; i < lines.size(); ++i) {
        Logical l{i, i, lines[i]};
        while (l.text.endsWith('\\') && !l.text.endsWith("\\\\") && l.last + 1 < lines.size())
            l.text = l.text.chopped(1) + lines[++l.last];
        out << l;
        i = l.last;
    }
    return out;
}

const QRegularExpression &assignRe()
{
    static const QRegularExpression re("^\\s*(?:export\\s+)?([A-Z_][A-Z0-9_]*)=(.*)$");
    return re;
}

void parseAssignments(const QString &text, QMap<QString, QString> *values,
                      QMap<QString, QString> *source, const QString &sourceName)
{
    for (const Logical &l : logicalLines(text.split('\n'))) {
        const auto m = assignRe().match(l.text);
        if (!m.hasMatch()) continue;
        values->insert(m.captured(1), shellValue(m.captured(2)));
        if (source) source->insert(m.captured(1), sourceName);
    }
}

// Safe for /etc/default/grub (a shell script): bare when trivially safe,
// otherwise single quotes — nothing inside single quotes is ever expanded.
QString shellQuote(const QString &v)
{
    static const QRegularExpression plain("^[A-Za-z0-9_./,:=+@%-]+$");
    if (plain.match(v).hasMatch()) return v;
    QString q = v;
    q.replace('\'', "'\\''");
    return "'" + q + "'";
}

CommandResult refusal(const QString &why)
{
    CommandResult r;
    r.program = "grub";
    r.started = true;
    r.exitCode = 1;
    r.output = why;
    return r;
}

QString stagingRoot()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/grub-stage";
    QDir().mkpath(dir);
    QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                   | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    return dir;
}

// The transaction (see the header). Positional parameters only — no value is
// ever pasted into the script text.
const char *kApplyScript = R"SH(
set -u
new=$1; base=$2; bdir=$3; mk=$4; cfg=$5; check=$6; tdir=$7; tname=$8; gdir=$9; mkopt=${10}
umask 022
# Work on a ROOT-OWNED copy: the staged file lives in the user's folder, and
# checking it then installing it by name would let it be swapped in between.
work=$(mktemp) && cp "$new" "$work" \
  || { echo "gt-stacer: could not prepare the new settings — nothing was changed"; exit 16; }
trap 'rm -f "$work"' EXIT
# Never write blind: the file on disk must be exactly what GT-STACER read and
# showed (it may have been edited meanwhile, or not been readable at all).
cmp -s "$base" /etc/default/grub \
  || { echo "gt-stacer: /etc/default/grub is not what GT-STACER read (it was changed meanwhile, or could not be read) — nothing was changed; reload and try again"; exit 15; }
grep -q '^[[:space:]]*GRUB_' "$work" \
  || { echo "gt-stacer: the new settings file has no GRUB settings in it — nothing was changed"; exit 17; }
sh -n "$work" || { echo "gt-stacer: the new settings have a syntax error — nothing was changed"; exit 12; }
mkdir -p "$bdir" && chmod 755 "$bdir" || { echo "gt-stacer: cannot create the backup folder $bdir — nothing was changed"; exit 10; }
stamp=$(date +%Y%m%d-%H%M%S); ts=$stamp; n=1
while [ -e "$bdir/default-grub.$ts" ]; do n=$((n+1)); ts="$stamp-$n"; done   # never overwrite a backup
cp -p /etc/default/grub "$bdir/default-grub.$ts" && chmod 644 "$bdir/default-grub.$ts" \
  || { echo "gt-stacer: could not back up /etc/default/grub — nothing was changed"; exit 11; }
[ -f "$cfg" ] && cp -p "$cfg" "$bdir/grub.cfg.$ts"
d=""
undo_theme() {   # put back a same-name theme that the new one replaced
  [ -n "$d" ] || return 0
  rm -rf "$d"
  [ -e "$d.gtold" ] && mv "$d.gtold" "$d"
  return 0
}
if [ -n "$tdir" ]; then
  d="$gdir/themes/$tname"
  rm -rf "$d.gtnew" "$d.gtold"
  { mkdir -p "$gdir/themes" && cp -R "$tdir" "$d.gtnew" && chown -R root:root "$d.gtnew" \
    && chmod -R u=rwX,go=rX "$d.gtnew"; } \
    || { rm -rf "$d.gtnew"; echo "gt-stacer: the theme could not be installed — nothing was changed"; exit 14; }
  if [ -e "$d" ]; then mv "$d" "$d.gtold" || { rm -rf "$d.gtnew"; echo "gt-stacer: the theme could not be installed — nothing was changed"; exit 14; }; fi
  mv "$d.gtnew" "$d" || { [ -e "$d.gtold" ] && mv "$d.gtold" "$d"; echo "gt-stacer: the theme could not be installed — nothing was changed"; exit 14; }
fi
install -m 644 -o root -g root "$work" /etc/default/grub.gtnew && mv -f /etc/default/grub.gtnew /etc/default/grub \
  || { rm -f /etc/default/grub.gtnew; undo_theme; echo "gt-stacer: could not write /etc/default/grub — nothing was changed"; exit 13; }
tmp="$cfg.gtnew"
set --
[ -n "$mkopt" ] && set -- "$mkopt"
if "$mk" "$@" -o "$tmp"; then
  if [ -z "$check" ] || "$check" "$tmp"; then
    [ -f "$cfg" ] && chmod --reference="$cfg" "$tmp" 2>/dev/null
    if mv -f "$tmp" "$cfg"; then
      [ -n "$d" ] && rm -rf "$d.gtold"
      # Keep the 20 newest restore points — by NAME (the timestamp): cp -p
      # keeps the old file's mtime, so ls -t would misorder them.
      ls -1 "$bdir"/default-grub.* 2>/dev/null | sort -r | tail -n +21 | while read -r f; do
        rm -f "$f" "$bdir/grub.cfg.${f##*.}"; done
      exit 0
    fi
  else
    echo "gt-stacer: the generated boot menu failed its syntax check"
  fi
fi
rm -f "$tmp" "$tmp.new"
undo_theme
if install -m 644 -o root -g root "$bdir/default-grub.$ts" /etc/default/grub.gtold \
   && mv -f /etc/default/grub.gtold /etc/default/grub; then
  echo "gt-stacer: generating the boot menu failed — the previous settings were restored and the menu was not changed"
  exit 20
fi
echo "gt-stacer: generating the boot menu failed AND the previous settings could not be put back — copy $bdir/default-grub.$ts to /etc/default/grub before restarting"
exit 21
)SH";

// Restore BY STAMP: root copies the backup itself (byte-exact), even when the
// current file is missing or broken; the current one is backed up first and
// put back if the menu can't be generated from the restored settings.
const char *kRestoreScript = R"SH(
set -u
stamp=$1; bdir=$2; mk=$3; cfg=$4; check=$5
case "$stamp" in *[!0-9-]*|"") echo "gt-stacer: invalid restore point"; exit 40;; esac
src="$bdir/default-grub.$stamp"
[ -f "$src" ] || { echo "gt-stacer: the restore point no longer exists — nothing was changed"; exit 41; }
sh -n "$src" || { echo "gt-stacer: the restore point has a syntax error — nothing was changed"; exit 42; }
umask 022
cur=""
if [ -e /etc/default/grub ]; then
  base=$(date +%Y%m%d-%H%M%S); ts=$base; n=1
  while [ -e "$bdir/default-grub.$ts" ]; do n=$((n+1)); ts="$base-$n"; done
  cp -p /etc/default/grub "$bdir/default-grub.$ts" && chmod 644 "$bdir/default-grub.$ts" \
    || { echo "gt-stacer: could not back up the current /etc/default/grub — nothing was changed"; exit 43; }
  cur="$bdir/default-grub.$ts"
fi
install -m 644 -o root -g root "$src" /etc/default/grub.gtnew && mv -f /etc/default/grub.gtnew /etc/default/grub \
  || { rm -f /etc/default/grub.gtnew; echo "gt-stacer: could not write /etc/default/grub — nothing was changed"; exit 44; }
tmp="$cfg.gtnew"
if "$mk" -o "$tmp" && { [ -z "$check" ] || "$check" "$tmp"; }; then
  [ -f "$cfg" ] && chmod --reference="$cfg" "$tmp" 2>/dev/null
  mv -f "$tmp" "$cfg" && exit 0
fi
rm -f "$tmp" "$tmp.new"
if [ -n "$cur" ]; then
  if install -m 644 -o root -g root "$cur" /etc/default/grub.gtold && mv -f /etc/default/grub.gtold /etc/default/grub; then
    echo "gt-stacer: the boot menu could not be generated from that restore point — the settings in use before were put back and the menu was not changed"
    exit 45
  fi
  echo "gt-stacer: the boot menu could not be generated from that restore point, AND the settings in use before could not be put back — they are saved as $cur"
  exit 46
fi
# There was no settings file before: keep the restored one (better than none).
echo "gt-stacer: the restore point was put back, but the boot menu could not be generated from it — the menu was not changed"
exit 47
)SH";

const char *kReinstallScript = R"SH(
set -u
mk=$1; cfg=$2; check=$3; shift 3
"$@" || { echo "gt-stacer: grub-install failed — GRUB may be PARTLY installed (the boot menu was not changed). Do not restart until it succeeds: fix the reported problem and run Reinstall again"; exit 30; }
tmp="$cfg.gtnew"
if "$mk" -o "$tmp" && { [ -z "$check" ] || "$check" "$tmp"; }; then
  [ -f "$cfg" ] && chmod --reference="$cfg" "$tmp" 2>/dev/null
  mv -f "$tmp" "$cfg"; exit 0
fi
rm -f "$tmp" "$tmp.new"
echo "gt-stacer: GRUB was reinstalled, but generating the boot menu failed — the old menu was kept"
exit 31
)SH";

} // namespace

GrubTool::Info GrubTool::info()
{
    Info i;
    i.uefi = sysExists("/sys/firmware/efi");
    if (i.uefi) {
        // efivar = 4 attribute bytes + the value byte.
        const QString sbVar = "/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c";
        int ec = -1;
        const QString v = CommandUtil::execProgramC("od", {"-An", "-tu1", "-j4", "-N1", sbVar}, 5000, &ec).trimmed();
        if (ec == 0 && !v.isEmpty()) i.secureBoot = v == "1" ? 1 : 0;
    }
    i.installCmd     = firstExisting({"grub-install", "grub2-install"});
    i.mkconfigCmd    = firstExisting({"grub-mkconfig", "grub2-mkconfig"});
    i.scriptCheckCmd = firstExisting({"grub-script-check", "grub2-script-check"});
    // The directory follows the tools (grub2-* ↔ /boot/grub2), as GT-GMT does —
    // not leftover directories from another installation.
    if (i.mkconfigCmd.startsWith("grub2-")) i.grubDir = "/boot/grub2";
    else if (!i.mkconfigCmd.isEmpty())      i.grubDir = "/boot/grub";
    else i.grubDir = sysExists("/boot/grub2/grub.cfg") ? "/boot/grub2" : "/boot/grub";
    i.grubCfg = i.grubDir + "/grub.cfg";
    if (!i.installCmd.isEmpty()) {
        const QString v = CommandUtil::execProgramC(i.installCmd, {"--version"}, 5000);
        const auto m = QRegularExpression("\\)\\s*(\\S+)").match(v);
        i.grubVersion = m.hasMatch() ? m.captured(1) : v.trimmed();
    }

    // os-release
    QString idLike;
    for (const QString &l : sysRead("/etc/os-release").split('\n')) {
        if (l.startsWith("ID="))      i.distroId = shellValue(l.mid(3));
        if (l.startsWith("ID_LIKE=")) idLike = shellValue(l.mid(8));
    }
    static const QStringList fedora = {"fedora", "rhel", "centos", "rocky", "almalinux", "ol"};
    i.fedoraFamily = fedora.contains(i.distroId);
    const QStringList like = idLike.split(' ', Qt::SkipEmptyParts);
    for (const QString &x : like) if (fedora.contains(x)) i.fedoraFamily = true;
    i.debianFamily = i.distroId == "debian" || i.distroId == "ubuntu"
                  || like.contains("debian") || like.contains("ubuntu");
    i.suseFamily = i.distroId.startsWith("opensuse") || i.distroId == "sles" || i.distroId == "sled"
                || like.contains("suse") || like.contains("opensuse");
    if (!i.mkconfigCmd.isEmpty()
        && CommandUtil::execProgramC(i.mkconfigCmd, {"--help"}, 5000).contains("--update-bls-cmdline"))
        i.blsCmdlineOpt = "--update-bls-cmdline";

    // ESP
    for (const QString &p : {QStringLiteral("/boot/efi"), QStringLiteral("/efi"), QStringLiteral("/boot")}) {
        const QString o = CommandUtil::execProgramC("findmnt", {"-no", "TARGET,FSTYPE", "--mountpoint", p}, 5000).simplified();
        if (o.endsWith(" vfat")) { i.esp = p; break; }
    }

    // Which loader is running: the firmware's BootCurrent entry (UEFI), or
    // GRUB's BIOS core (i386-pc) on the disk.
    if (i.uefi && CommandUtil::commandExists("efibootmgr")) {
        const QString out = CommandUtil::execProgramC("efibootmgr", {"-v"}, 5000);
        const auto cur = QRegularExpression("BootCurrent:\\s*([0-9A-Fa-f]{4})").match(out);
        if (cur.hasMatch()) {
            const auto line = QRegularExpression("Boot" + cur.captured(1) + "\\*?\\s+([^\\n]*)").match(out);
            const QString l = line.captured(1).toLower();
            if (l.contains("systemd")) i.loader = "systemd-boot";
            else if (l.contains("refind")) i.loader = "refind";
            else if (l.contains("grub") || l.contains("shim")) { i.loader = "grub"; i.loaderConfirmed = true; }
            const auto id = QRegularExpression("\\\\efi\\\\([^\\\\]+)\\\\", QRegularExpression::CaseInsensitiveOption).match(line.captured(1));
            if (id.hasMatch()) i.bootCurrentId = id.captured(1);
        }
    }
    // BIOS: a grub.cfg means GRUB. UEFI: only the firmware's BootCurrent entry
    // can confirm it (a leftover grub.cfg next to systemd-boot / a UKI must not
    // make Reinstall put GRUB first in the boot order).
    if (i.loader.isEmpty() && sysExists(i.grubCfg)) { i.loader = "grub"; i.loaderConfirmed = !i.uefi; }
    // --bootloader-id: Debian/Ubuntu's grub-install derives it from
    // GRUB_DISTRIBUTOR, exactly as the package's own upgrades do — pass nothing.
    // Elsewhere reuse the entry the firmware booted, unless that is the
    // fallback path (BOOT) or Windows' (Microsoft) — then leave the default.
    if (!i.debianFamily && !i.bootCurrentId.isEmpty() && i.loaderConfirmed
        && i.bootCurrentId.compare("BOOT", Qt::CaseInsensitive) != 0
        && i.bootCurrentId.compare("Microsoft", Qt::CaseInsensitive) != 0)
        i.efiId = i.bootCurrentId;

    // Can GRUB write grubenv at boot? (00_header: no on btrfs/zfs/…, nor via
    // lvm / diskfilter; LUKS isn't writable to GRUB either.)
    {
        const QString fs = CommandUtil::execProgramC("findmnt", {"-no", "FSTYPE", "--target", i.grubDir}, 5000).trimmed();
        i.bootFs = fs;
        static const QStringList readOnlyFs = {"btrfs", "zfs", "squashfs", "cpiofs", "romfs", "tarfs"};
        bool writable = !readOnlyFs.contains(fs);
        const QString dev = CommandUtil::execProgramC("findmnt", {"-no", "SOURCE", "--target", i.grubDir}, 5000)
                                .trimmed().remove(QRegularExpression("\\[.*\\]$"));
        if (dev.startsWith("/dev/")) {
            const QString types = CommandUtil::execProgramC("lsblk", {"-snlo", "TYPE", dev}, 5000);
            if (types.contains(QRegularExpression("\\b(lvm|raid\\w*|crypt|dm)\\b"))) writable = false;
        }
        i.envWritable = writable;
        i.recordfailForced = i.uefi && !writable && sysRead("/etc/grub.d/00_header").contains("recordfail_broken");
    }

    // The disk that really holds /boot (or /): walk UP through partition,
    // LVM, LUKS, RAID with `lsblk -s` (inverse) and collect every disk.
    QString src = CommandUtil::execProgramC("findmnt", {"-no", "SOURCE", "--mountpoint", "/boot"}, 5000).trimmed();
    if (src.isEmpty()) src = CommandUtil::execProgramC("findmnt", {"-no", "SOURCE", "--mountpoint", "/"}, 5000).trimmed();
    src.remove(QRegularExpression("\\[.*\\]$"));               // btrfs "/dev/sda2[/@]"
    QStringList disks;
    if (src.startsWith("/dev/")) {
        const QString inv = CommandUtil::execProgramC("lsblk", {"-snlo", "PATH,TYPE", src}, 5000);
        for (const QString &l : inv.split('\n', Qt::SkipEmptyParts)) {
            const QStringList f = l.simplified().split(' ');
            if (f.size() == 2 && f[1] == "disk" && !disks.contains(f[0])) disks << f[0];
        }
    }
    if (disks.size() == 1) i.bootDisk = disks.first();
    else i.bootDiskWhy = disks.isEmpty() ? QStringLiteral("unknown") : QStringLiteral("several:") + disks.join(", ");
    return i;
}

QStringList GrubTool::editableKeys()
{
    return {"GRUB_TIMEOUT", "GRUB_TIMEOUT_STYLE", "GRUB_DEFAULT", "GRUB_SAVEDEFAULT",
            "GRUB_DISABLE_OS_PROBER", "GRUB_DISABLE_RECOVERY", "GRUB_GFXMODE",
            "GRUB_CMDLINE_LINUX_DEFAULT", "GRUB_THEME"};
}

GrubTool::Settings GrubTool::settings()
{
    Settings s;
    s.rawBytes = sysReadBytes(kDefaultFile);
    s.raw = QString::fromUtf8(s.rawBytes);
    s.readable = s.raw.contains(QRegularExpression("^\\s*(export\\s+)?GRUB_[A-Z_]*=", QRegularExpression::MultilineOption));
    if (!s.readable) { s.raw.clear(); s.rawBytes.clear(); }
    parseAssignments(s.raw, &s.values, nullptr, QString());
    // grub-mkconfig sources /etc/default/grub.d/*.cfg AFTER the main file:
    // a key set there wins, and editing the main file would do nothing.
    const QString gd = "/etc/default/grub.d";
    for (const QString &f : sysList(gd, false)) {
        if (!f.endsWith(".cfg")) continue;
        const QString path = gd + "/" + f;
        QMap<QString, QString> vals, src;
        parseAssignments(sysRead(path), &vals, &src, path);
        for (auto it = vals.cbegin(); it != vals.cend(); ++it) {
            // "$KEY more" EXTENDS the main value (e.g. a driver adding a kernel
            // parameter): the main file still counts, keep showing its value.
            if (it.value().contains("$" + it.key()) || it.value().contains("${" + it.key() + "}")) {
                s.appendedIn.insert(it.key(), src.value(it.key()));
                continue;
            }
            s.values.insert(it.key(), it.value());
            s.overriddenIn.insert(it.key(), src.value(it.key()));
            s.appendedIn.remove(it.key());
        }
    }
    s.bls = s.values.value("GRUB_ENABLE_BLSCFG") == "true";
    return s;
}

QString GrubTool::validate(const QString &key, const QString &value)
{
    if (value.contains('\n') || value.contains('\r') || value.contains(QChar(0))) return "chars";
    if (key == "GRUB_TIMEOUT" || key == "GRUB_RECORDFAIL_TIMEOUT") {
        bool ok = false; const int n = value.toInt(&ok);
        return ok && n >= -1 && n <= 600 ? QString() : QStringLiteral("number");
    }
    if (key == "GRUB_TIMEOUT_STYLE")
        return QStringList{"menu", "countdown", "hidden"}.contains(value) ? QString() : QStringLiteral("chars");
    if (key == "GRUB_SAVEDEFAULT" || key == "GRUB_DISABLE_OS_PROBER" || key == "GRUB_DISABLE_RECOVERY")
        return value == "true" || value == "false" ? QString() : QStringLiteral("chars");
    if (key == "GRUB_GFXMODE") {
        static const QRegularExpression re("^(auto|\\d{3,5}x\\d{3,5}(x\\d{1,2})?)(,(auto|\\d{3,5}x\\d{3,5}(x\\d{1,2})?))*$");
        return re.match(value).hasMatch() ? QString() : QStringLiteral("resolution");
    }
    if (key == "GRUB_CMDLINE_LINUX_DEFAULT") {
        // Kernel parameters: words of a safe alphabet, single spaces.
        // '!' for e.g. "acpi_osi=! acpi_osi=Linux" (not special in GRUB script).
        static const QRegularExpression re("^([A-Za-z0-9_.,:=/+@%!-]+( [A-Za-z0-9_.,:=/+@%!-]+)*)?$");
        return re.match(value).hasMatch() ? QString() : QStringLiteral("params");
    }
    if (key == "GRUB_THEME") {
        if (value.isEmpty()) return {};
        const QFileInfo fi(value);
        const bool where = value.startsWith("/boot/grub/themes/") || value.startsWith("/boot/grub2/themes/")
                        || value.startsWith("/usr/share/grub/themes/");
        return where && fi.fileName() == "theme.txt" && sysExists(value) && !value.contains("..")
                   ? QString() : QStringLiteral("theme");
    }
    if (key == "GRUB_DEFAULT") return value.isEmpty() ? QStringLiteral("chars") : QString();
    return {};
}

QString GrubTool::applyChanges(const QString &raw, const QMap<QString, QString> &changes)
{
    QStringList lines = raw.split('\n');
    const bool trailingNewline = raw.endsWith('\n');
    if (trailingNewline) lines.removeLast();
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        const QString &key = it.key();
        // Every assignment of the key as a span of physical lines (continuations).
        QVector<Logical> spans;
        for (const Logical &l : logicalLines(lines)) {
            const auto m = assignRe().match(l.text);
            if (m.hasMatch() && m.captured(1) == key) spans << l;
        }
        // An empty theme = no theme: comment out EVERY assignment (an earlier
        // one would otherwise become active) — kept for reference.
        if (key == "GRUB_THEME" && it.value().isEmpty()) {
            for (int k = spans.size() - 1; k >= 0; --k)
                for (int j = spans[k].first; j <= spans[k].last; ++j)
                    lines[j] = "#" + lines[j] + (j == spans[k].first ? "   # disabled by GT-STACER" : "");
            continue;
        }
        const QString line = key + "=" + shellQuote(it.value());
        if (spans.isEmpty()) { lines << line; continue; }
        const Logical last = spans.last();
        lines[last.first] = line;                     // the last assignment wins in sh
        for (int j = last.last; j > last.first; --j) lines.removeAt(j);
    }
    return lines.join('\n') + (trailingNewline ? "\n" : "");
}

QVector<GrubTool::MenuEntry> GrubTool::parseMenuEntries(const QString &cfgText)
{
    QVector<MenuEntry> out;
    struct Open { QString title; QString id; int depth; };
    QVector<Open> subs;
    int depth = 0;
    static const QRegularExpression head("^(menuentry|submenu)\\s+(.*)$");
    static const QRegularExpression idRe("\\$menuentry_id_option\\s+('[^']*'|\"[^\"]*\"|\\S+)");
    for (const QString &raw : cfgText.split('\n')) {
        const QString t = raw.trimmed();
        const auto m = head.match(t);
        if (m.hasMatch()) {
            // The title is one shell-like word: 'It'\''s' → It's, "a b" → a b.
            const QString title = shellValue(m.captured(2));
            const auto idm = idRe.match(m.captured(2));
            const QString id = idm.hasMatch() ? shellValue(idm.captured(1)) : QString();
            if (m.captured(1) == "submenu") {
                subs.append({title, id, depth});
            } else {
                QStringList tp, ip;
                bool allIds = !id.isEmpty();
                for (const Open &o : subs) { tp << o.title; ip << o.id; if (o.id.isEmpty()) allIds = false; }
                tp << title; ip << id;
                // GRUB_DEFAULT by id survives a title change; without ids use titles.
                out.append({tp.join('>'), allIds ? ip.join('>') : tp.join('>')});
            }
        }
        if (t.endsWith('{')) ++depth;
        else if (t == "}") {
            --depth;
            if (!subs.isEmpty() && subs.last().depth == depth) subs.removeLast();
        }
    }
    return out;
}

QStringList GrubTool::parseMenu(const QString &cfgText)
{
    QStringList out;
    for (const MenuEntry &e : parseMenuEntries(cfgText)) out << e.title;
    return out;
}

QVector<GrubTool::MenuEntry> GrubTool::menuEntryList(CommandResult *result)
{
    const Info i = info();
    const CommandResult r = CommandUtil::runCommand("pkexec", {"cat", i.grubCfg}, 120000);
    if (result) *result = r;
    if (!r.ok()) return {};
    QVector<MenuEntry> out = parseMenuEntries(r.output);
    // Fedora (BLS): the kernels are not menuentry lines but "blscfg", which
    // makes one entry per /boot/loader/entries/*.conf (newest version first);
    // its id is the file name without .conf.
    if (r.output.contains(QRegularExpression("^\\s*blscfg\\b", QRegularExpression::MultilineOption))) {
        QVector<MenuEntry> bls;
        QStringList files = sysList("/boot/loader/entries", false);
        std::sort(files.begin(), files.end(), std::greater<QString>());
        for (const QString &f : files) {
            if (!f.endsWith(".conf")) continue;
            for (const QString &l : sysRead("/boot/loader/entries/" + f).split('\n'))
                if (l.startsWith("title ")) { bls.append({l.mid(6).trimmed(), f.chopped(5)}); break; }
        }
        out = bls + out;
    }
    return out;
}

QStringList GrubTool::menuEntries(CommandResult *result)
{
    QStringList out;
    for (const MenuEntry &e : menuEntryList(result)) out << e.title;
    return out;
}

QMap<QString, QString> GrubTool::themes()
{
    QMap<QString, QString> out;
    for (const QString &base : {QStringLiteral("/boot/grub/themes"), QStringLiteral("/boot/grub2/themes"),
                                QStringLiteral("/usr/share/grub/themes")}) {
        for (const QString &name : sysList(base, true)) {
            const QString t = base + "/" + name + "/theme.txt";
            if (!out.contains(name) && sysExists(t)) out.insert(name, t);
        }
    }
    return out;
}

QString GrubTool::stageTheme(const QString &source, QString *themeName, QString *error)
{
    const QFileInfo fi(source);
    QString root;
    if (fi.isDir()) {
        root = fi.absoluteFilePath();
    } else {
        root = stagingRoot() + "/" + QUuid::createUuid().toString(QUuid::Id128);
        QDir().mkpath(root);
        const QString low = source.toLower();
        CommandResult r;
        if (low.endsWith(".zip"))
            r = CommandUtil::commandExists("unzip")
                ? CommandUtil::runCommand("unzip", {"-q", "-o", source, "-d", root}, 120000)
                : CommandUtil::runCommand("bsdtar", {"-x", "-f", source, "-C", root}, 120000);
        else   // tar refuses absolute and ../ member names by default
            r = CommandUtil::runCommand("tar", {"-x", "--no-same-owner", "-f", source, "-C", root}, 120000);
        if (!r.ok()) { if (error) *error = "bad-archive"; return {}; }
    }
    // Shallowest theme.txt; symlinks are not followed (and are removed from
    // a staged copy so nothing outside it ends up under /boot).
    QString best;
    int bestDepth = 99;
    QDirIterator it(root, {"theme.txt"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        const int depth = f.mid(root.size()).count('/');
        if (depth < bestDepth) { bestDepth = depth; best = QFileInfo(f).absolutePath(); }
    }
    if (best.isEmpty() || bestDepth > 4) { if (error) *error = "no-theme"; return {}; }
    if (!fi.isDir()) {
        QDirIterator links(best, QDir::AllEntries | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot,
                           QDirIterator::Subdirectories);
        while (links.hasNext()) { const QString f = links.next(); if (QFileInfo(f).isSymLink()) QFile::remove(f); }
    }
    QString name = QFileInfo(best).fileName();
    if (best == root || name.isEmpty()) name = fi.completeBaseName();
    name.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "-");
    name.remove(QRegularExpression("^[-.]+"));
    if (name.isEmpty()) { if (error) *error = "bad-name"; return {}; }
    if (themeName) *themeName = name;
    return best;
}

CommandResult GrubTool::apply(const QString &newText, const QByteArray &baseBytes,
                              const QString &themeDir, const QString &themeName, bool updateBlsCmdline)
{
    const Info i = info();
    if (i.mkconfigCmd.isEmpty()) return refusal("grub-mkconfig: command not found");
    if (baseBytes.isEmpty())
        return refusal("gt-stacer: /etc/default/grub could not be read — nothing was changed");
    // "." / ".." would make "$gdir/themes/$tname" the grub dir itself.
    if (!themeName.isEmpty() && (!QRegularExpression("^[A-Za-z0-9._-]+$").match(themeName).hasMatch()
                                 || themeName.startsWith('.')))
        return refusal("gt-stacer: invalid theme name");
    // Stage the new file where root (also on the host, under Flatpak) can read it.
    // Unique names: two GT-STACER windows must not overwrite each other's files.
    const QString tag = QUuid::createUuid().toString(QUuid::Id128);
    const QString staged = stagingRoot() + "/default-grub." + tag + ".new";
    QSaveFile f(staged);
    if (!f.open(QIODevice::WriteOnly) || f.write(newText.toUtf8()) < 0 || !f.commit())
        return refusal("gt-stacer: could not stage the new settings file");
    QFile::setPermissions(staged, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    const QString base = stagingRoot() + "/default-grub." + tag + ".base";
    QSaveFile b(base);
    if (!b.open(QIODevice::WriteOnly) || b.write(baseBytes) < 0 || !b.commit()) {
        QFile::remove(staged);
        return refusal("gt-stacer: could not stage the new settings file");
    }
    QFile::setPermissions(base, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

    const CommandResult r = CommandUtil::runCommand("pkexec",
        {"/bin/sh", "-c", QString::fromLatin1(kApplyScript), "gt-stacer-grub",
         staged, base, kBackupDir, i.mkconfigCmd, i.grubCfg, i.scriptCheckCmd,
         themeDir, themeName, i.grubDir, updateBlsCmdline ? i.blsCmdlineOpt : QString()}, -1);
    QFile::remove(staged);
    QFile::remove(base);
    return r;
}

QString GrubTool::backupDir() { return kBackupDir; }

CommandResult GrubTool::restore(const QString &stamp)
{
    if (!QRegularExpression("^\\d{8}-\\d{6}(-\\d+)?$").match(stamp).hasMatch())
        return refusal("gt-stacer: invalid restore point");
    const Info i = info();
    if (i.mkconfigCmd.isEmpty()) return refusal("grub-mkconfig: command not found");
    return CommandUtil::runCommand("pkexec",
        {"/bin/sh", "-c", QString::fromLatin1(kRestoreScript), "gt-stacer-grub",
         stamp, kBackupDir, i.mkconfigCmd, i.grubCfg, i.scriptCheckCmd}, -1);
}

QStringList GrubTool::restorePoints()
{
    QStringList out;
    QStringList files = sysList(kBackupDir, false);
    std::sort(files.begin(), files.end(), std::greater<QString>());
    for (const QString &f : files)
        if (f.startsWith("default-grub.")) out << f.mid(QStringLiteral("default-grub.").size());
    return out;
}

QString GrubTool::restorePointText(const QString &stamp)
{
    if (!QRegularExpression("^\\d{8}-\\d{6}(-\\d+)?$").match(stamp).hasMatch()) return {};
    return sysRead(QString(kBackupDir) + "/default-grub." + stamp);
}

QStringList GrubTool::installArgv(const Info &i)
{
    if (i.uefi) {
        QString target = "x86_64-efi";
        if (QSysInfo::currentCpuArchitecture().startsWith("arm64")) target = "arm64-efi";
        else if (sysRead("/sys/firmware/efi/fw_platform_size").trimmed() == "32") target = "i386-efi";
        QStringList a = {i.installCmd, "--target=" + target, "--efi-directory=" + i.esp};
        if (!i.efiId.isEmpty()) a << "--bootloader-id=" + i.efiId;
        return a;
    }
    return {i.installCmd, "--target=i386-pc", i.bootDisk};
}

QString GrubTool::reinstallPlan(const Info &i, QStringList *commands)
{
    if (i.loader != "grub" || i.installCmd.isEmpty() || i.mkconfigCmd.isEmpty()) return "not-grub";
    if (i.uefi) {
        if (!i.loaderConfirmed) return "not-confirmed";  // the firmware didn't say GRUB started us
        if (i.fedoraFamily) return "fedora-uefi";       // grub2-install is unsupported on UEFI there
        if (i.suseFamily)   return "suse-uefi";         // shim-install / update-bootloader do it there
        // With Secure Boot on, a plain grub-install writes an UNSIGNED GRUB the
        // firmware refuses → no boot. Debian/Ubuntu's grub-install installs the
        // signed shim chain itself; elsewhere the signed packages must do it.
        if (i.secureBoot == 1 && !i.debianFamily) return "secure-boot";
        if (i.esp.isEmpty()) return "no-esp";
    } else if (i.bootDisk.isEmpty()) {
        return "no-disk";
    }
    if (commands)
        *commands = QStringList{installArgv(i).join(' '), i.mkconfigCmd + " -o " + i.grubCfg};
    return {};
}

CommandResult GrubTool::reinstall(const Info &i)
{
    const QString why = reinstallPlan(i, nullptr);
    if (!why.isEmpty()) return refusal("gt-stacer: reinstall not possible: " + why);
    return CommandUtil::runCommand("pkexec",
        QStringList{"/bin/sh", "-c", QString::fromLatin1(kReinstallScript), "gt-stacer-grub",
                    i.mkconfigCmd, i.grubCfg, i.scriptCheckCmd} + installArgv(i), -1);
}
