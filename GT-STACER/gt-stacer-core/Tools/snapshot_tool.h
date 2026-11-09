#pragma once
#include <QString>
#include <QVector>

// System-restore snapshots. The primary backend is **Timeshift** (linuxmint/
// timeshift) — it works on any filesystem in rsync mode and on Btrfs in btrfs
// mode, so it covers the vast majority of desktops. **ZFS** is supported where a
// pool exists. Everything is detected at runtime; the UI hides the section when
// no backend is present. Listing and every change run through pkexec (root).
//
// Restore is destructive and, for Timeshift, reboots the machine — callers must
// confirm emphatically.
struct Snapshot {
    QString id;           // Timeshift: "YYYY-MM-DD_HH-MM-SS"; ZFS: "pool/ds@name"
    QString date;         // display date/time
    QString tags;         // Timeshift tags (O/B/H/D/W/M) — "" for ZFS
    QString description;  // user comment
    QString size;         // "" when unknown
};

class SnapshotTool {
public:
    enum Backend { None, Timeshift, Snapper, Zfs };

    // Every engine that is installed AND usable on this system (the filesystem
    // may support more than one — e.g. Btrfs can offer Timeshift + Snapper). The
    // UI offers a choice when this returns >1.
    static QVector<Backend> availableBackends();
    static Backend idealBackend();         // best engine for the detected filesystem
    static void    setBackend(Backend b);  // choose the active engine
    static Backend backend();              // the active engine (first available by default)
    static QString nameOf(Backend b);      // "Timeshift" | "Snapper" | "ZFS"
    static QString backendName();          // nameOf(backend()) — "" if None
    static bool    available();            // backend() != None
    static QString mode();                 // Timeshift: "rsync"|"btrfs"; else ""
    static bool    canRestore();           // Snapper restore is not wired yet

    // Facts for suggesting the right engine when nothing is active yet. The UI
    // builds the (translated) advice from these; the core stays text-free.
    static QString rootFsType();           // "ext4" | "btrfs" | "zfs" | "" …
    static bool    hasTimeshift();         // the tool is installed (may be unconfigured)
    static bool    hasSnapper();
    static bool    hasZfs();               // zfs tools + a pool

    // ── Where & how Timeshift really works here (26.12) ──
    // Timeshift's btrfs mode needs the Ubuntu-style layout: the root filesystem
    // mounted from a subvolume named "@" (and /home from "@home"). A system
    // installed at the btrfs top level (subvol=/, subvolid=5) can only use
    // rsync mode — and rsync snapshots go to whatever device is selected.
    static QString rootSubvolume();        // "/@", "/" … ("" if not btrfs)
    static bool    timeshiftBtrfsLayoutOk(); // root on btrfs subvolume "@"
    struct Location {
        QString device;    // e.g. "/dev/sda2"
        QString fsType;    // e.g. "ext4"
        QString label;     // e.g. "DATA"
        QString mount;     // where it is mounted, if it is
        bool    sameAsRoot = false;   // the system partition itself
    };
    static Location timeshiftLocation();   // the device Timeshift writes to
    // Problems in Timeshift's own settings that make btrfs mode impossible.
    enum class TsIssue { None, BtrfsModeWithoutLayout, BtrfsModeOnNonBtrfsDevice, NoDevice };
    static TsIssue timeshiftIssue();

    static QVector<Snapshot> list();       // pkexec (prompts)
    static bool create(const QString &comment);
    static bool remove(const Snapshot &s);
    static bool restore(const Snapshot &s);   // DESTRUCTIVE (Timeshift reboots)
};
