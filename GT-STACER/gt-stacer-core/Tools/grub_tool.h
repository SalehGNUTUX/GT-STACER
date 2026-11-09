#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QByteArray>
#include "../Utils/command_failure.h"

// Boot loader (GRUB) — the native replacement for the GT-GMT tool (26.12).
// See TOOLS-REVIEW/GT-GMT.md: the original could inject root commands through
// settings values (sed + a file that grub-mkconfig SOURCES as shell), wrote
// GRUB to the wrong disk (lsblk -d never lists partitions), corrupted
// /etc/default/grub by following its own hint, and its backups failed
// silently or could not be restored.
//
// Rules this class follows:
//  • /etc/default/grub is a SHELL script (Debian's GRUB_DISTRIBUTOR even runs
//    $(…)). Only the keys the user edits are touched; every other line stays
//    byte-for-byte. New values are single-quoted, so nothing in them is ever
//    expanded — and they are validated per key before that.
//  • Every change is ONE transaction in ONE pkexec: back up the current file
//    and grub.cfg (timestamped, under /var/backups/gt-stacer/grub), install
//    the new file, `sh -n` it, generate the menu into a TEMPORARY file, check
//    it with grub-script-check, and only then replace grub.cfg. Any failure
//    restores the previous /etc/default/grub and leaves grub.cfg untouched.
//  • Reinstalling GRUB targets the disk that really holds /boot (resolved
//    through partitions, LVM and LUKS), is refused when that is ambiguous,
//    and is shown to the user as the exact commands before it runs.
class GrubTool {
public:
    struct Info {
        bool    uefi = false;
        int     secureBoot = -1;      // 1 on, 0 off, -1 unknown / BIOS
        QString loader;               // "grub", "systemd-boot", "refind", "" (unknown)
        QString grubVersion;          // "2.12-9+deb13u2"
        QString installCmd;           // grub-install / grub2-install
        QString mkconfigCmd;          // grub-mkconfig / grub2-mkconfig
        QString scriptCheckCmd;       // grub-script-check / grub2-script-check ("" = none)
        QString grubDir;              // /boot/grub or /boot/grub2
        QString grubCfg;              // <grubDir>/grub.cfg
        QString esp;                  // EFI system partition mount point ("" = none / BIOS)
        QString bootDisk;             // /dev/sda — disk holding /boot ("" = ambiguous)
        QString bootDiskWhy;          // why bootDisk is empty (several disks, RAID …)
        QString efiId;                // --bootloader-id to pass ("" = let grub-install use the distro default)
        QString bootCurrentId;        // UEFI: \EFI\<id>\ of the loader the firmware started ("" = unknown)
        bool    loaderConfirmed = false; // UEFI: the firmware's BootCurrent entry IS a GRUB/shim one
        QString bootFs;               // filesystem holding grubDir (btrfs, ext4 …)
        // GRUB can write its environment block (grubenv) at boot: NOT on btrfs /
        // zfs, nor through LVM / RAID / LUKS. Without it "remember the last
        // choice" fails at every boot, and Debian/Ubuntu force
        // GRUB_RECORDFAIL_TIMEOUT on UEFI instead of GRUB_TIMEOUT.
        bool    envWritable = true;
        bool    recordfailForced = false; // UEFI + Debian-style 00_header + !envWritable
        QString distroId;             // os-release ID
        bool    fedoraFamily = false; // grub2-install on UEFI is unsupported there
        bool    debianFamily = false; // grub-install keeps the signed shim chain itself
        bool    suseFamily   = false; // UEFI: shim-install / update-bootloader, not grub2-install
        QString blsCmdlineOpt;        // "--update-bls-cmdline" if grub2-mkconfig has it (Fedora BLS)
    };
    static Info info();

    // The editable settings, as GRUB will see them (main file + grub.d overrides).
    struct Settings {
        QMap<QString, QString> values;        // key → unquoted value
        QMap<QString, QString> overriddenIn;  // key → /etc/default/grub.d/x.cfg that sets it LAST
        QMap<QString, QString> appendedIn;    // key → grub.d file that EXTENDS it ("$KEY more")
        QString raw;                          // the main file's text ("" = could not be read)
        QByteArray rawBytes;                  // …its exact bytes (the base the transaction compares)
        bool    readable = false;             // the main file was read (never edit blind)
        bool    bls = false;                  // GRUB_ENABLE_BLSCFG=true (Fedora): entries in /boot/loader/entries
    };
    static QStringList editableKeys();        // GRUB_TIMEOUT, GRUB_DEFAULT, …
    static Settings settings();

    // "" if `value` is acceptable for `key`, else a reason code
    // ("number", "resolution", "params", "theme", "chars").
    static QString validate(const QString &key, const QString &value);

    // New file text: only `changes` keys rewritten (last assignment, or appended).
    static QString applyChanges(const QString &raw, const QMap<QString, QString> &changes);

    // Menu entries from grub.cfg (needs root: grub.cfg is 0600 on Debian) —
    // "Title" or "Submenu>Title", in menu order. One password prompt. On a BLS
    // system (Fedora) grub.cfg only says "blscfg": the titles come from
    // /boot/loader/entries/*.conf instead.
    static QStringList menuEntries(CommandResult *result);
    // A menu entry: what the user reads, and what GRUB_DEFAULT stores. The id
    // ($menuentry_id_option, "submenu-id>entry-id") survives title changes;
    // entries without an id fall back to the title path.
    struct MenuEntry { QString title; QString id; };
    static QVector<MenuEntry> menuEntryList(CommandResult *result);
    static QVector<MenuEntry> parseMenuEntries(const QString &cfgText);
    static QStringList parseMenu(const QString &cfgText);   // titles only (tests)

    // Installed themes (dirs with a theme.txt) → theme.txt path.
    static QMap<QString, QString> themes();
    // Unpack/locate a theme the user picked (a folder, or a .tar.* / .zip
    // archive) into a private staging folder. Returns the folder holding
    // theme.txt, or "" with *error set ("no-theme", "bad-archive", "bad-name").
    static QString stageTheme(const QString &source, QString *themeName, QString *error);

    // The transaction (one pkexec). `newText` = the new /etc/default/grub
    // (pass settings().raw to just regenerate the menu). `baseText` = the file
    // as it was READ (settings().raw): the transaction refuses to write if the
    // file on disk is not exactly that — edited elsewhere meanwhile, or not
    // readable at all (Flatpak without host /etc), so it is never overwritten
    // blind. `themeDir`/`themeName` optionally install a staged theme into
    // <grubDir>/themes/<name> first. `updateBlsCmdline` passes
    // --update-bls-cmdline to grub2-mkconfig (BLS: kernel parameters live in
    // the entries, and changing GRUB_CMDLINE_LINUX_DEFAULT alone does nothing).
    static CommandResult apply(const QString &newText, const QByteArray &baseBytes,
                               const QString &themeDir = QString(), const QString &themeName = QString(),
                               bool updateBlsCmdline = false);

    // Restore points made by apply(): newest first, "YYYYMMDD-HHMMSS".
    static QString backupDir();
    static QStringList restorePoints();
    static QString restorePointText(const QString &stamp);   // its /etc/default/grub
    // Put a restore point back BY STAMP: root copies the backup byte-for-byte
    // (no round trip through the UI), backs up the current file first, then
    // regenerates the menu; a failure puts the current file back. Works even
    // when /etc/default/grub is now missing or broken.
    static CommandResult restore(const QString &stamp);

    // Reinstall the boot loader to the boot disk/ESP, then regenerate the menu.
    // `commands` receives the exact commands, for the confirmation dialog.
    // Returns "" when possible, else a reason code ("not-grub", "no-disk",
    // "not-confirmed" (UEFI: the firmware didn't start GRUB), "fedora-uefi",
    // "suse-uefi", "secure-boot", "no-esp").
    static QString reinstallPlan(const Info &i, QStringList *commands);
    // The exact grub-install argv (program first) — shown AND run, one source.
    static QStringList installArgv(const Info &i);
    static CommandResult reinstall(const Info &i);
};
