#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "../Utils/command_failure.h"

// Disk images (ISO / IMG): identify, mount, unmount, extract — the native
// replacement for the GT-IMT tool (26.12). See TOOLS-REVIEW/gt-imt.md for the
// bugs in the original that this deliberately avoids.
//
// • Mounting goes through udisks (udisksctl loop-setup + mount), exactly like a
//   file manager mounting a USB stick: NO root, no `sh -c`, paths passed as
//   argv, mount points created by udisks under /media/<user>/<label>.
// • A partitioned disk image (e.g. a Raspberry Pi .img) gets every partition
//   mounted — udisks exposes them as loopNp1, loopNp2 …
// • The kind is read from the file's CONTENT, so a compressed image (.img.xz)
//   or a VM disk (qcow2/VMDK/VDI) gets an explanation instead of a failed mount.
// • Unmount = unmount every partition FIRST, then detach. (udisks accepts a
//   detach while partitions are still mounted and returns success, but the
//   device stays attached and mounted — verified.)
class DiskImageTool {
public:
    enum class Kind {
        Unknown,
        Iso,              // ISO 9660 (CD/DVD image, most Linux installers)
        Udf,              // UDF (DVD/Blu-ray, Windows installers)
        PartitionedDisk,  // MBR/GPT disk image with partitions
        Filesystem,       // a bare filesystem image (ext4, FAT, NTFS, squashfs …)
        Compressed,       // xz / gzip / zstd / bzip2 / zip — must be decompressed first
        VirtualDisk,      // qcow2 / VMDK / VDI / VHD — a VM disk, not a raw image
        Empty             // zero-length / all zeros at the start
    };

    struct Info {
        Kind    kind = Kind::Unknown;
        QString detail;      // e.g. "ext4", "xz", "qcow2", "MBR", "GPT"
        qint64  size = 0;
        bool    mountable() const   { return kind == Kind::Iso || kind == Kind::Udf
                                          || kind == Kind::PartitionedDisk || kind == Kind::Filesystem; }
    };

    struct Part {
        QString device;      // /dev/loop0 or /dev/loop0p1
        QString fsType;      // iso9660, vfat, ext4 … ("" = no filesystem)
        QString label;
        QString mountPoint;  // "" = not mounted
        qint64  size = 0;
    };

    struct Attached {
        QString file;        // backing image file
        QString loop;        // /dev/loopN
        bool    readOnly = true;
        QVector<Part> parts; // the loop itself (no partitions) or each partition
    };

    static Info inspect(const QString &path);

    // Images attached as loop devices that belong to the user's world (backed
    // by a regular file outside system areas — snap packages etc. excluded).
    static QVector<Attached> attached();

    // Attach (read-only by default) and mount every filesystem in it.
    // `mounted` receives the mount points. The result explains a failure.
    static CommandResult mount(const QString &path, bool readOnly, QStringList *mounted);

    // Unmount every partition, then detach the loop device.
    static CommandResult unmount(const Attached &img);

    // Which programs keep a mount point busy: "dolphin (pid 1234)" …
    static QStringList holders(const QString &mountPoint);

    // argv (program first) to extract an ISO/UDF image with 7z (or bsdtar /
    // xorriso if 7z is missing). overwrite=false → keep existing files (real
    // skip, not 7z's -aou rename). Empty if no extractor is installed.
    // `udf` = the image is UDF (else ISO 9660): 7z is told the type, so on a
    // hybrid ISO it never picks the partition table instead of the disc.
    static QStringList isoExtractArgv(const QString &image, const QString &dest, bool overwrite,
                                      bool udf = false);
    // argv to copy a mounted partition's files to dest (rsync, cp fallback).
    static QStringList copyArgv(const QString &mountPoint, const QString &dest, bool overwrite);

    // ── Optical discs (CD / DVD / Blu-ray in a drive) ──────────────────────
    // Detected through udisks (MediaAvailable …), NOT the kernel's size: an
    // empty drive reports a fake 1 GB in /sys/block/srN/size. Reading the disc
    // needs no root — logind gives the seat user an ACL on /dev/srN (and the
    // "cdrom" group can read it) — and goes through the host in Flatpak.
    struct OpticalDrive {
        QString device;            // /dev/sr0
        QString model;             // "PLDS DVD-ROM DS-8DBSH"
        bool    media = false;     // a disc is in the drive
        bool    blank = false;     // an empty writable disc
        int     audioTracks = 0;
        int     dataTracks = 0;
        int     sessions = 0;
        QString mediaType;         // "DVD-R", "CD", "Blu-ray" … ("" = unknown)
        QString fsType;            // iso9660 / udf ("" = none recognised)
        QString label;
        QString mountPoint;        // "" = not mounted
        qint64  size = 0;          // readable capacity of the disc
        bool    readable = false;  // this user may read the device
        bool    canWrite = false;  // the DRIVE can burn (udisks MediaCompatibility has -R/-RW …)
        bool    rewritable = false;// the DISC in it is -RW / -RE / -RAM (can be erased)
        bool    audioOnly() const { return media && audioTracks > 0 && dataTracks == 0; }
    };
    static QVector<OpticalDrive> opticalDrives();

    // Bytes to copy for an exact image of the disc: the ISO 9660 volume size
    // when the disc has one (single session) — reading past it hits the
    // unreadable run-out blocks of many burnt CDs — else the disc capacity.
    static qint64 opticalImageSize(const OpticalDrive &d);

    // argv (program first) to copy `bytes` of the disc into `dest` with dd,
    // printing progress ("… bytes … copied") on stderr.
    static QStringList discCopyArgv(const QString &device, const QString &dest, qint64 bytes);

    // ── Burning an ISO onto a disc (xorriso, the libburn tool) ─────────────
    // What xorriso says about the disc in a writer, before burning.
    struct BurnMedia {
        bool    ok = false;        // xorriso ran and reported the media
        QString media;             // "DVD-R sequential recording", "CD-RW" …
        bool    blank = false;
        bool    appendable = false;
        bool    closed = false;
        qint64  freeBytes = -1;    // -1 = unknown
        QString output;            // xorriso's own words (for explanations)
    };
    static BurnMedia burnMedia(const QString &device);

    // argv to burn `image` onto the disc in `device` (xorriso cdrecord mode,
    // progress "N of M MB written"). blankFirst = erase a written -RW disc.
    static QStringList burnArgv(const QString &device, const QString &image, bool blankFirst);
    // argv to compare the first `bytes` of the disc with the image (cmp:
    // 0 = identical, 1 = different or the disc ended early, 2 = read error).
    static QStringList verifyArgv(const QString &image, const QString &device, qint64 bytes);
};
