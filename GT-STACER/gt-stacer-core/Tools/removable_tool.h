#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "../Utils/command_failure.h"

// USB sticks & memory cards: list, check, repair, format, health, surface
// scan, back up to an image — the native replacement for the gt-usdr tool
// (26.12). See TOOLS-REVIEW/gt-usdr.md for what the original got wrong (its
// GUI treated every disk except "sda" as external: NVMe system disks and zram
// were offered for formatting).
//
// Safety model:
//  • Only REMOVABLE disks are listed (USB / card reader / RM / hotplug), never
//    zram/loop/optical, and a disk is PROTECTED — listed but locked — when any
//    of its filesystems is mounted outside /media, /run/media, /mnt (the
//    system, /home …) or is active swap.
//  • Destructive actions re-verify the target at the moment they run
//    (verifyTarget): still connected, still removable, not protected, and the
//    SAME device (serial + size) the user confirmed — a different stick
//    plugged into the same /dev name is refused.
//  • Mount, unmount, check, repair and format go through UDisks2 over D-Bus —
//    what the desktop's file manager and GNOME Disks use: no shell, no root
//    for removable media in a normal desktop session (polkit asks only when the
//    system's policy says so). UDisks2 itself refuses to check or format a
//    mounted filesystem.
class RemovableTool {
public:
    struct Part {
        QString device;       // /dev/sdb1
        QString fsType;       // vfat, exfat, ntfs, ext4 … ("" = none)
        QString label;
        QString mountPoint;   // "" = not mounted
        qint64  size = 0;
        qint64  used = -1;    // -1 = unknown (not mounted)
    };

    struct Disk {
        QString device;       // /dev/sdb, /dev/mmcblk0
        QString model;
        QString vendor;
        QString serial;
        QString transport;    // usb, mmc …
        QString tableType;    // dos, gpt, "" (none / whole-device filesystem)
        qint64  size = 0;
        bool    readOnly = false;   // write-protect switch on an SD card, etc.
        QString protectedReason;    // "" = usable; else why it is locked (see Protection)
        QVector<Part> parts;  // partitions; or the disk itself when it has a filesystem directly
        QString title() const;      // "SanDisk Cruzer Blade (15.6 GB)"
    };

    // Reasons a removable disk is locked (protectedReason values).
    static constexpr const char *kProtSystem = "system";   // holds / /home /boot … or is mounted elsewhere
    static constexpr const char *kProtSwap   = "swap";     // active swap

    static QVector<Disk> disks();

    // Re-check right before a destructive action. `device` is a disk or one of
    // its partitions; `serial`/`size` are what the user saw and confirmed.
    // Returns "" when safe, else a reason code: "gone", "changed", "readonly",
    // or a protection reason.
    static QString verifyTarget(const QString &device, const QString &serial, qint64 size);

    // ── UDisks2 operations (blocking — run them off the UI thread) ─────────────
    static CommandResult mount(const QString &device, QString *mountPoint = nullptr);
    static CommandResult unmount(const QString &device);
    // consistent=false → errors were found. Requires an unmounted filesystem.
    static CommandResult check(const QString &device, bool *consistent);
    static CommandResult repair(const QString &device);
    // New filesystem on one partition (or on a whole-device filesystem).
    static CommandResult formatPartition(const QString &device, const QString &fsType, const QString &label);
    // New MBR partition table + ONE partition over the whole disk + filesystem.
    static CommandResult formatDisk(const QString &disk, const QString &fsType, const QString &label);

    static QStringList formatTypes();                // the AVAILABLE ones of formatOptions()

    // Every filesystem worth offering, in display order, and whether this
    // system can create it — asked from UDisks2 itself (Manager.CanFormat),
    // which also names the missing program (→ the package to install).
    struct FsOption {
        QString fs;            // vfat exfat ntfs ext4 f2fs btrfs xfs ext3 ext2 udf nilfs2
        bool    available = false;
        QString missingTool;   // e.g. "mkudffs"
        QString package;       // e.g. "udftools" (Debian/Fedora/Arch names agree for these)
    };
    static QVector<FsOption> formatOptions();

    // The filesystem that suits this device best, and why.
    // why: "sd-small" (SD ≤ 32 GB → FAT32, the SDHC standard), "sd-large"
    // (SD > 32 GB → exFAT, SDXC), "usb-small" (≤ 32 GB → FAT32, works
    // everywhere), "usb-large" (> 32 GB → exFAT), "drive" (big disk → exFAT).
    struct Advice { QString fs; QString why; bool sdCard = false; };
    static Advice advise(const Disk &disk, qint64 targetSize);
    // Smallest size each filesystem can be created on (0 = no practical limit).
    static qint64 minSize(const QString &fsType);
    static QString partitionType(const QString &fsType);   // MBR type for a whole-device format

    static int  maxLabelLength(const QString &fsType);
    // "" if fine, else "too-long" or "bad-chars".
    static QString labelProblem(const QString &fsType, const QString &label);

    // ── Health (S.M.A.R.T.) — needs root (smartctl), one password prompt ──────
    struct Health {
        bool    supported = false;   // most USB sticks / SD cards have no SMART
        bool    passed = true;
        qint64  powerOnHours = -1;
        int     temperature = -1;
        qint64  reallocated = -1;    // ATA attribute 5
        qint64  pending = -1;        // ATA attribute 197
        QString model;
    };
    static Health health(const QString &disk, CommandResult *result);

    // ── Long jobs: argv for a guarded QProcess (run as ROOT) ───────────────────
    // Read-only surface scan; prints "NN.NN% done … (r/w/c errors)".
    static QStringList surfaceScanArgv(const QString &device);
    // Raw image of the device to STDOUT (the caller writes it to a file it owns);
    // progress on stderr ("… bytes … copied").
    static QStringList imageArgv(const QString &device);
};
