# Changelog

All notable changes to GT-STACER are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning follows
the `YY.MM` rolling-release scheme (matching the website roadmap).

---

## [26.12-stable] — 2026-11-09

A large release: the Helpers page gains native Disk images (ISO mount, CD/DVD
copy and burn), USB drives and Boot (GRUB) tabs that replace three separate
tools; System Relief keeps the heaviest app running; Packages gets a reworked
Upgrades tab across every source; one animated waiting card everywhere; and
plain-language explanations for every failed command.

### Added
- **Local packages tab** in the Package & Software Manager: a "+" button (or
  drag & drop) to install a package FILE. It detects the distribution and its
  package manager and which file types this system can install (.deb, .rpm,
  Arch .pkg.tar.*, Alpine .apk, Solus .eopkg, Flatpak bundle/ref, Snap,
  AppImage), shows the package details, and for .deb / .rpm / pacman packages
  checks every dependency: already installed, installable from the
  repositories (version constraint checked against the repo candidate too), or
  not available. Required dependencies are installed automatically; optional
  ones (Recommends / Suggests / optdepends) can be ticked — all, some or none.
- **Plain-language failure explanations everywhere.** A failed command is now
  classified (password prompt cancelled, not authorized, no polkit agent,
  program missing, package manager busy, dpkg interrupted, no network, disk
  full, read-only disk, package not found, broken dependencies, permission
  denied, timeout) and explained with a suggested fix — in the package log
  window, notifications, snapshots, home backup, recovery, System Cleaner,
  Uninstaller, Helpers, Power and APT sources. Replaces guesses such as
  "authorization declined?".
- **Backup & Snapshots shows where Timeshift stores restore points** and
  explains Timeshift settings that can't work (BTRFS mode on a system without
  the "@" subvolume layout, BTRFS mode pointed at a non-BTRFS disk, snapshot
  disk not connected), with the fix (Snapper, or RSYNC mode on the system
  partition).
- **Helpers is organized into short, clear tabs — System · Disk images ·
  USB drives · Boot** — the native, safe replacements for the author's
  separate tools gt-imt, gt-usdr and GT-GMT (reviews of all three in
  `TOOLS-REVIEW/`). Each tab's details are in its tooltip.
- **"Disk images" tab** — the
  native replacement for the GT-IMT tool (see `TOOLS-REVIEW/gt-imt.md`).
  Mount ISO / UDF / IMG images **without a password** through udisks (like a
  file manager mounting a USB stick), browse them, and unmount them; a
  partitioned image (e.g. a Raspberry Pi .img) gets every partition mounted.
  Drag & drop an image onto the list to mount it. The image type is read from
  the file's content, so a compressed image (.img.xz …), a VM disk (qcow2,
  VMDK, VDI, VHD) or an empty/unknown file gets an explanation and the command
  to convert it, instead of a failed mount. "Busy" on unmount names the
  programs still using the image. Extract an image into a new folder named
  after it — ISO with 7z/bsdtar/xorriso, disk images by mounting read-only and
  copying with rsync — with a progress bar and a Stop button (the copy never
  outlives GT-STACER), then the image is unmounted again if GT-STACER mounted
  it. "Replace" overwrites same-name files only, "keep" really skips them; no
  file in the destination is ever deleted. File names are passed as
  arguments, never through a shell.
- **CD / DVD / Blu-ray discs** in the "Disk images" tab: the disc in the drive
  is recognised (type, label, size — through udisks, since an empty drive
  reports a fake 1 GB to the kernel) and re-checked every few seconds while
  the tab is shown. **Save as ISO** copies it into a local `.iso` without a
  password: exactly the ISO 9660 volume size (no unreadable run-out blocks),
  progress bar, Stop, written to `.part` and renamed only when complete — a
  stopped or failed copy never leaves a half image. Free space and FAT32's
  4 GB limit are checked first. Audio CDs, blank discs, missing read
  permission (cdrom group), a scratched disc / copy-protected video DVD, and a
  disc removed mid-copy each get their own explanation. **Open** mounts and
  opens the disc; **Eject** unmounts it first and names the programs still
  using it.
- **Burn ISO** onto a CD / DVD / Blu-ray (xorriso, no password): only on a
  drive that can write (a read-only DVD-ROM explains itself), only ISO/UDF
  images (a USB .img is pointed to the USB tab). The disc is checked first —
  closed or already-written discs are refused with the reason, a written
  rewritable disc is erased only after an explicit "Erase and burn", and an
  image larger than the disc is refused before anything is written. Progress
  bar; Stop asks first (a stopped write-once disc is lost). Afterwards the disc
  is compared byte-for-byte with the image (on by default); a mismatch, a
  removed disc, a busy drive, missing permission and write errors each get
  their own explanation.
- **"USB drives" tab** (replaces gt-usdr, whose GUI listed every disk except
  "sda" as external — an NVMe system disk or zram could be formatted). Lists
  only removable disks (USB, card readers — partitions found from PKNAME, since
  lsblk prints a flat list for some column sets); a removable disk that holds the
  system or active swap is shown locked. Through UDisks2 — no password for
  removable media in a normal session: mount/unmount, **check** (read-only),
  **repair** (unmounts first, remounts after; names the programs keeping it
  busy), **format a partition** or **the whole device** (new MBR table + one
  partition of the right type) with **every filesystem the system can create**
  (asked from UDisks2: FAT32, exFAT, NTFS, ext4, F2FS, Btrfs, XFS, ext3, ext2,
  UDF, NILFS2 — missing ones shown with the package to install). A
  **recommendation for the device** (SD ≤ 32 GB → FAT32/SDHC, SD > 32 GB →
  exFAT/SDXC, small stick → FAT32, large → exFAT) comes first, and a
  compatibility card (Windows / macOS / Linux / TVs-cars-cameras / largest
  file) with warnings for poor choices updates as you pick; filesystems too
  small for the device are refused, labels are checked per filesystem, and
  Linux filesystems are created owned by the user (UDisks2 take-ownership). **Health** reads
  S.M.A.R.T. (with the USB-bridge fallback) and explains plainly that most
  sticks and cards don't have it. **Surface scan** (read-only badblocks) and
  **Back up to image** (an image file owned by the user, free-space check,
  partial image deleted on Stop/failure) run with progress and Stop. Every
  destructive action re-checks right before it runs that the SAME device
  (serial + size) is still connected. Plugging/unplugging updates the list.
- **"Boot" tab** (replaces GT-GMT, whose review found root command injection
  through settings values, grub-install aimed at the wrong disk, and backups
  that failed silently). Shows the firmware (BIOS/UEFI, Secure Boot), the boot
  loader, the boot disk (resolved through partitions/LVM/LUKS) or ESP. Edits
  the GRUB menu: waiting time, menu display, default entry (by name, read from
  the real menu, or "remember the last choice"), other operating systems,
  recovery entries, resolution, kernel parameters, theme (install from a
  folder or archive). Only the edited keys of /etc/default/grub change — it is
  a shell script, so every other line stays byte-for-byte and new values are
  validated and single-quoted. **Every change is one transaction** under one
  password prompt: restore point → new file → `sh -n` → menu generated into a
  temporary file → grub-script-check → replace; any failure puts the previous
  settings back automatically. Restore points can be viewed and restored.
  "Reinstall GRUB" shows the exact commands first and refuses when the target
  is ambiguous (several disks) or unsupported (Fedora + UEFI). systemd-boot
  and rEFInd are shown, not edited.
- **System Relief is smart about what it freezes.** Automatic relief used to
  freeze EVERY candidate, including the job doing the work. It now judges
  activity per app (the whole process tree) and never freezes an app that owns
  the active window (KWin on KDE — Wayland and X11 — or xprop on X11), is
  playing sound, holds a sleep inhibitor, is using the CPU or disk, or was
  active in the last 20 seconds. A frozen app is thawed the moment you switch
  to it. A new "State" column says why each process is (or isn't) in use.
- **Smart auto relief: the heaviest app gets the machine.** Sparing every busy
  app meant that with several busy apps nothing was frozen and the load stayed.
  Auto relief now picks the **main workload** — the app using the most CPU +
  disk, with everything it runs in the background (sticky: a challenger must
  load twice as much to take over, and a short pause doesn't demote it) — and
  escalates in stages: idle apps first; if the load is still high after another
  hold period, the busy background apps too. The active window, apps playing
  sound and apps keeping the system awake are never frozen, and switching to a
  frozen app thaws it at once. Thawing goes back down step by step, and only
  once the load is clearly (10 points) below the threshold, so it doesn't
  oscillate. "Prioritise the heaviest app" (on by default) turns stage 2 off.
  The State column marks the main workload and explains, per app, what auto
  relief would do.
- **Automatic relief is on by default.** With the smarter model above (only
  idle apps first, the main workload, focused, audio-playing and
  sleep-inhibiting apps never paused, thresholds at 90 %), it is safe to run
  for everyone. A choice the user already saved is kept.
- **"i" next to the BFQ disk-scheduler option** explains what BFQ (Budget Fair
  Queueing) is, when it helps (HDD, SD, USB, slow SSD) and when it doesn't.

### Fixed
- **Packages → Upgrades, reworked: every source at once, and a choice of
  sources before anything runs.** "All sources" (or one: APT, Flatpak, Snap …)
  first refreshes the package lists — without a password through PackageKit
  (`pkcon refresh`) or `checkupdates` on Arch — then lists each update with its
  installed and new version and its source. A box per source ("APT (79)",
  "Flatpak (1)") leaves a whole source out; single updates can be unticked too.
  "Upgrade…" shows a summary per source and runs ONE command per source (one
  password per system source, not one per package); a fully ticked source gets
  its proper full upgrade. Fixed on the way:
  - APT ran without a terminal: a "keep your config file?" question could fail
    or hang the upgrade — now non-interactive, keeping the user's files.
  - APT "upgrade all" held back updates that need a new dependency (2 of 79
    here) — now `upgrade --with-new-pkgs` (nothing is removed).
  - Pacman's "upgrade selected" was a partial upgrade, which can break Arch —
    refused with an explanation; only the full -Syu.
  - The lists were not refreshed, so "Everything is up to date" could be days
    old; and managers it can't ask (XBPS, APK) were reported up to date — now
    named as such. Zypper updates are listed.
  - While it searches, a clear animated card sits in the middle of the window
    (pulse rings, a rotating gradient arc, the current step: "Refreshing
    APT's package lists, then checking… (1 of 2)").
- **The loading overlay was redrawn** (every page that uses it: Packages,
  System Cleaner, Backup, Uninstaller): a centred card with pulse rings, a
  rotating gradient arc, a breathing core and bouncing dots, fading in; its
  colours were hard-coded for the dark theme and now follow the theme.
- **One waiting sign everywhere**: the same card, with a title naming the
  service and a line for the step, now covers every wait that has no Stop
  button — Packages (each tab on its own: Installed, Search, Upgrades, Store
  add-ons, AppImages, reading a local package), Uninstaller, System Cleaner
  (scan, clean), Backup (list, create, delete, restore), Boot (read the menu,
  apply, restore, reinstall), Disk images (mount, unmount, open, read / open /
  eject / check a disc), USB drives (mount, check, repair, health, format),
  Firewall, and the App cache / Package cache / Flatpak-Snap dialogs. Jobs
  with progress and Stop (disc copy, burn, extraction, surface scan, PhotoRec,
  the command log) keep their bar. A wait shorter than 0.15 s shows nothing
  (no flash); the card follows its tab or dialog's size by itself.
- *USB "Health" asked for the password twice* on drives behind a USB bridge
  (smartctl first without, then with `-d sat` — two pkexec calls). Both
  attempts now run in ONE pkexec, the disk passed as a positional argument.
- *Firewall froze the whole window* while polkit asked for the password (the
  ufw / firewall-cmd calls ran on the UI thread) — they now run in the
  background behind the card.
- **Boot tab (GRUB): full technical review before first release** (two
  independent passes, every finding checked against GRUB's own scripts and
  GT-GMT; transaction and restore scripts tested on copies in 20+ scenarios).
  Fixed:
  - *Flatpak would have overwritten /etc/default/grub with a near-empty file*:
    the sandbox has its own /etc, so the file read as empty. System files are
    now read through the host, and every write is refused unless the file on
    disk is byte-for-byte what was read (also catches edits made elsewhere).
  - The new settings file is copied by root before it is checked and
    installed (a file in the user's folder could have been swapped in between).
  - Restore points are put back by root, byte-exact, by their stamp — and now
    also work when /etc/default/grub is missing or broken; a failure puts the
    previous file back (or says exactly what happened).
  - Reinstall on UEFI: only when the firmware confirms GRUB started the
    computer; refused with Secure Boot on outside Debian/Ubuntu (an unsigned
    GRUB would not start) and on openSUSE (its own shim tool); Debian/Ubuntu
    get grub-install's own --bootloader-id (as the packages use), never the
    fallback "BOOT" or "Microsoft" entry; the dialog shows which loader starts
    the computer now. A failed grub-install says GRUB may be partly installed.
  - "Remember the last choice" is disabled where GRUB can't write at boot
    (btrfs, zfs, LVM, RAID, LUKS — it failed at every boot); on UEFI there the
    waiting time is also written to GRUB_RECORDFAIL_TIMEOUT, which GRUB uses
    instead; "wait forever" + hidden menu (a black screen) is refused.
  - The default entry is stored by its menu id, so it survives kernel
    updates; titles with apostrophes, line continuations, grub.d files that
    extend a value, and Fedora BLS entries / kernel parameters
    (--update-bls-cmdline) are handled. GRUB_SAVEDEFAULT is no longer added on
    every Apply on Fedora. The GRUB folder follows the tools (grub2-* ↔
    /boot/grub2). Themes: a same-name theme is restored if Apply fails and
    replaced only after asking, /usr/share themes are copied where GRUB can
    read them at boot, "no theme" disables every GRUB_THEME line.
- **USB tab: "Surface scan" crashed the app with no message** (caught with
  gdb: `RemovableTab::surfaceScan` → `Disk::title()` → `std::bad_alloc`). It
  held a reference into the device list, then unmounted and refreshed the list,
  so the reference pointed at freed memory. "Back up to image" had the same
  bug (the list can refresh while its save dialog is open). Both now take a
  copy.
- **USB tab: after "Surface scan" or "Back up to image" the device stayed
  unmounted** (unlike Check / Repair). Whatever the job unmounted is mounted
  again when it ends — finished, failed or stopped; if that fails, the status
  line says so and how to mount it.
- **A crash no longer goes unexplained**: the reason (signal or uncaught
  exception, with its message) and the call stack are written to
  `~/.config/GNUTUX/gt-stacer/crash.log`, and the next start shows a
  notification with the file's path. The app's own functions are named in it
  (linked with `-rdynamic`).
- **Apps frozen by System Relief stayed frozen after GT-STACER was closed
  abruptly** (found in testing: 8 apps, spectacle and KDE Connect among them,
  still stopped hours later). SIGTERM / SIGINT / SIGHUP ended the app without
  any clean-up, and in Qt 6 `quit()` is cancelled by a window that refuses to
  close (minimize-to-tray) — so they now end the event loop with `exit()` and
  every destructor runs (Relief thaws what it froze). For SIGKILL or a crash,
  the frozen apps are kept in a journal (`~/.cache/gt-stacer/`, PID + process
  start time, per instance); the next start thaws what a dead instance left
  frozen — never a recycled PID, never another running instance's — and says
  so in a notification.
- **USB stick health showed "the operation failed"** (e.g. Kingston
  DataTraveler 2.0): smartctl answers a stick without S.M.A.R.T. with exit
  status 2 ("no IDENTIFY DEVICE structure") — a report, not a failure. It now
  says plainly that the device doesn't report its health (normal for USB
  sticks and memory cards) and suggests the surface scan; smartctl's answer is
  in the details.
- **Hybrid ISO images didn't mount** ("is not a mountable filesystem" — most
  Linux installers, e.g. CachyOS): UDisks2 exposes their ISO 9660 filesystem
  only on the ISO partition, not on the whole device. The ISO/UDF partition is
  mounted now (then every partition, for images without one); extraction tells
  7z the image type so a hybrid's partition table is never extracted instead.
- **Backup & Snapshots didn't notice changes made in Timeshift (or Snapper)**
  while GT-STACER was open — the engine, location and warnings stayed stale
  until a restart. The page now watches /etc/timeshift and /etc/snapper
  (surviving Timeshift's save-by-replace), refreshes the status within a
  second, clears a snapshot list that belonged to the old location, and also
  refreshes whenever the page is shown. No password needed.
- **Arabic text mixed with commands, paths or English names was scrambled**
  (parentheses and slashes on the wrong side, words out of order, a paragraph
  starting with an English word laid out left-to-right). Fixed once for the
  whole app: every label, button, group title, message box and tooltip is
  bidi-isolated automatically before it is painted, and the custom-painted
  gauges/charts/cards do the same ("2.79 GB / 7.65 GB", "61.0 °C" on the
  Dashboard were reversed in Arabic).
- **ZFS restore said "restores need a reboot" but rolled back immediately**,
  live. The status now says restores apply immediately, the dialog explains it
  and lists the newer restore points `zfs rollback -r` deletes, requires an
  explicit tick, and offers a restart afterwards.
- **Startup Apps read .desktop files with QSettings**: an Exec with quotes or
  spaces, a Name with a comma or Categories with ";" were mangled, and every
  enable/disable rewrote the file keeping only [Desktop Entry] (deleting
  [Desktop Action …] groups). Now read and written with a real desktop-entry
  parser, atomically; new entries get a safe file name and never overwrite
  another entry with the same name.
- **Pacman packages listed three times** when yay/paru are installed (#1);
  the installed-package filters now list one entry per package database.
- **Recovery: Stop did nothing, and PhotoRec kept running after GT-STACER was
  closed or killed.** PhotoRec runs as root, so our signal was refused. It now
  runs under a root guard tied to a pipe: Stop (no second password) or
  GT-STACER exiting in any way — even SIGKILL — stops it.
- **Home backup kept running after a force-close**, and Stop froze the window.
  Same guard (rsync), instant stop.
- **Home backup into a folder inside home copied the backup into itself** on
  every run (Backups/Backups/…); the destination is now excluded. Choosing home
  itself or a parent of it (e.g. /home) is refused — in mirror mode it would
  have deleted the home folder.
- **Home backup on FAT32 / exFAT / NTFS / SMB** ended in errors and, on FAT,
  re-copied everything each run. rsync options now follow the destination
  filesystem (detected, including ntfs-3g behind "fuseblk"); FAT32 skips files
  ≥ 4 GB. Mirror mode warns clearly before deleting files it didn't create
  (a marker file identifies GT-STACER backups). rsync exit 24 ("files vanished")
  is success; exit 23 lists the files that couldn't be copied and why.
- **Timeshift mode shown wrongly**: GT-STACER read Timeshift's template
  (default.json) instead of its real settings (timeshift.json), so BTRFS-mode
  systems showed "rsync" and an unconfigured Timeshift looked configured.
- **Snapshots reported failure after 5 minutes** while Timeshift carried on;
  snapshot/restore operations are no longer timed out.
- **AppImages**: most apps integrated by other tools were missing from the
  list (a quoted Exec was misread); launchers lost categories, translated
  names, MIME types, StartupWMClass and actions (so wrong menu section, no
  "Open with", generic taskbar icon); the file was renamed (non-Latin names all
  became "_" and overwrote each other); re-integrating a file already in the
  folder DELETED it; a launcher named like a system app hid the system one;
  failures were reported as success. The embedded desktop entry is now carried
  over faithfully and the file keeps its name.
- GT-STACER's own launcher listed two main categories (could appear twice in
  menus).

### Security
- `isSafeIdentifier()` accepted names starting with "-", letting package
  metadata inject options into root package-manager commands. Rejected now.
- Desktop entries are parsed with a real freedesktop parser
  (`Utils/desktop_entry`) instead of QSettings.

## [26.11.3-stable] — 2026-09-17

A UX-focused point release, from a testing pass on the Package Manager and
Recovery pages.

### Added
- **Checkbox selection in tables — no more Ctrl-click.** Every action table (the
  Package Manager's Installed / Search & Install / Upgrades / Store add-ons /
  AppImages tabs, and the Processes list) now has a tick box per row and a
  **Select all** toggle, so you can pick several items for install / upgrade /
  remove / kill without holding a modifier key. A single row selection still works
  as a fallback.
- **A live progress-log window for package operations, with a Stop button.**
  Install / upgrade / uninstall now run in a window that streams the command's
  output line by line, shows a busy indicator, and can be **stopped** — the
  missing cancel control. (A privileged step already running as root may finish in
  the background; the log says so.)
- **Completion notifications for long tasks.** Any operation you start and leave
  running — package install/upgrade/remove, System Cleaner, Home backup, File
  Recovery — now raises a desktop notification when it finishes, so you learn it's
  done even after navigating to another section.

### Changed
- **Recovery: the file-type list fills the available height** instead of a small
  cramped box, and it's dimmed while "Recover all file types" is on (the cue that
  every format is already included) — unticking that re-enables per-type picking.
- Search & Install now gives feedback ("type a package name…") instead of doing
  nothing when the box is empty.
- `PackageTool` gained `installCommand()` / `upgradeCommand()` / `upgradeAllCommand()` /
  `removeCommand()` (the argv the executors already built), so the UI can run a
  package command with live output and cancel — same no-shell, validated-name
  security model.

## [26.11.2-stable] — 2026-09-16

Another small point release, following testing of 26.11.1.

### Fixed
- **The "What's new" dialog still showed the 26.10 highlights.** Its content was
  hard-coded and never refreshed, so after updating to 26.11 / 26.11.1 the dialog
  listed the wrong release's changes (the title was correct). It now lists the
  26.11 highlights plus the 26.11.1 fixes, in English and Arabic.
- **In-app "Download & install" could silently not appear.** The install-method
  detection ran `dpkg -S <path>`, which scans every installed package's file list
  — fast on a warm cache but slow enough on a cold cache or a busy old disk to hit
  the 6 s timeout and be misread as "not a package install", hiding the one-click
  update. Detection now queries by **package name** (`dpkg-query -W` / `rpm -q`,
  a single fast DB lookup) with a path-based fallback and a longer timeout.
- **The update offer is now a clear window.** When you check for updates and a
  newer version exists, GT-STACER shows a dialog with a prominent **Download &
  install** action (for DEB / RPM / AppImage installs) and a **Release page**
  button — not just an easy-to-miss inline link. Translated to Arabic.

### Changed
- **The update check now compares versions across any number of components.**
  `isNewer` walks every dotted component (not just `YY.MM.PATCH`) and the tag
  parser captures them all, so stable updates are recognised whatever the version
  numbering and sequence (26.11 < 26.11.1 < 26.11.2 < 26.12 < 27.01, and deeper).
  `/releases/latest` already returns the newest non-prerelease, and the installer
  picks its asset by type, so any stable release is fetched correctly.

## [26.11.1-stable] — 2026-09-16

A bug-fix point release on top of 26.11.

### Fixed
- **Firewall page reported "no supported firewall" even when ufw/firewalld was
  installed.** A GUI launched from a desktop entry often inherits a `PATH`
  without the sbin directories, so `ufw` (in `/usr/sbin`, `/sbin`) was invisible
  to the executable lookup. GT-STACER now ensures `/usr/local/sbin`, `/usr/sbin`
  and `/sbin` are on `PATH` at startup, so admin tools are detected reliably.
  (The Firewall page shipped in 26.08; this affected every install since.)
- **In-app update check ignored the patch component of a version.** The version
  comparison only looked at `YY.MM`, and the tag parser dropped the patch, so a
  point release such as 26.11.1 was not recognised as newer than 26.11. Versions
  are now compared as `YY.MM.PATCH`, and the tag parser captures the patch.

## [26.11-stable] — 2026-09-16

The "software management" release. The Uninstaller becomes a full Package &
Software Manager; the app can update itself; and a real command-injection hole
was fixed. No new hard dependency.

### Package & Software Manager (evolves the Uninstaller page, same sidebar slot)
- **Installed** — browse and remove software across every detected manager
  (multi-select), as before.
- **Search & Install** — search the system package manager, Flatpak or Snap and
  install packages. New core: `PackageTool::install()` / `upgrade()` /
  `upgradeAll()` / `upgradable()`, mirroring the secure `remove()` model
  (`isSafeIdentifier` + argv + `pkexec`).
- **Upgrades** — list packages with available updates and apply them (selected or
  all). Checking is manual — no automatic package-update polling.
- **Store add-ons** — manage opendesktop.org / KNewStuff content installed under
  `~/.local/share` (Plasma themes, icons, cursors, plasmoids, wallpapers, colour
  schemes…): remove (guarded) and install via `ocs-url://` links, or open the store.
- **AppImages** — integrate/remove AppImages, GearLever-compatible (below).

### AppImage integration (GearLever-compatible)
- Uses the **same managed folder and launcher format** as GearLever
  (`it.mijorus.gearlever`) — reads its configured folder and move-vs-clone
  preference from its keyfile, so apps integrated by either tool appear in both,
  with no duplication. Works standalone when GearLever is absent (defaults to
  `~/AppImages`, clone).
- Integration extracts the icon and desktop entry **without executing the
  AppImage** — it computes the squashfs offset from the ELF header and reads it
  with `unsquashfs`. Writes a valid freedesktop launcher (`desktop-file-validate`
  clean).

### In-app updates
- Checks GitHub for a newer GT-STACER version on startup (**on by default**) and
  can **download → verify SHA-256 → install** the update: replace the running
  AppImage (atomic, no root), or hand the `.deb`/`.rpm` to the package manager via
  `pkexec`; Flatpak/source builds get guidance. Nothing runs without an explicit
  click; the checksum is verified before any install.

### Security
- **Fixed a command-injection vulnerability:** package search concatenated the
  user's query into a `sh -c` string. It is now passed as argv (no shell) and
  validated to a safe character set. All new install/upgrade/remove paths use
  `execProgram` with validated names; store-add-on and AppImage removal are
  path-guarded; AppImage metadata is read without executing the file.

### UI
- **Processes** default to CPU %, busiest first, sorted numerically and live.
- **Tray tooltip** shows the CPU/system temperature next to CPU % / RAM %.
- **Resizable table columns everywhere** — a shared helper keeps the primary
  column filling the width while every column stays freely resizable with a
  visible, grabbable drag handle; clearer header separators.

## [26.10-stable] — 2026-08-19

The "responsiveness & onboarding" release. System Relief learns about disk I/O,
the Power timer reaches the Power page, and a translated onboarding/update/share
layer is added. No new hard dependency.

### System Relief — disk-aware
- Reads **disk pressure** from PSI (`/proc/pressure/io`, "some avg10") — the "frozen
  but CPU/RAM are fine" signal — shown as a live readout and usable as an automatic
  trigger alongside CPU/RAM.
- New **Disk** column showing each candidate's block-layer I/O rate (from
  `/proc/<pid>/io`), so the process thrashing the disk is visible.
- **Ease disk I/O** — lowers the I/O priority of the ticked processes to the idle
  class (`ionice -c3`, no root) so the foreground keeps a responsive disk without
  freezing them.
- **Switch to BFQ** — changes the root disk's I/O scheduler (loads the `bfq`
  module via `pkexec` if needed); the biggest single win for an old HDD under load,
  and it makes the ionice hint effective. Applies until reboot.

### Power
- The **Power timer** (scheduled shutdown / restart / suspend / hibernate, with a
  live countdown that keeps running while minimized) is now on the **Power page**,
  mirroring the one in Settings.

### Onboarding, updates & sharing
- The **welcome tour** is re-openable from Settings, its slides are now translated
  (they were hard-coded English), and it — like the new **"what's new"** dialog
  shown once after each update — carries an **in-dialog language picker**.
- An **opt-in update check** (Settings) queries GitHub's public Releases API and
  notifies when a newer stable release exists; nothing is downloaded.
- A **Copy share text** button places a description + link + hashtags on the clipboard.
- Language changes now flow through a single hub (`AppManager::changeLanguage`) so
  the Settings combo and both dialogs re-translate the whole UI consistently.

### UI
- **Services** moved directly under **Processes** in the sidebar.
- The Arabic language entry is simplified to "العربية" (flag + name, no region).

## [26.09-stable] — 2026-08-01

The "backup, snapshots & recovery" release. Two new pages — Backup and Recovery —
turn GT-STACER into a data-safety tool. No new hard dependency; the new pages use
tools detected at runtime (`timeshift`/`snapper`/`zfs`, `rsync`, `photorec`).

### Backup (new page)
- **System snapshots** through **Timeshift**, **Snapper** or **ZFS**. The ideal
  engine for the root filesystem is detected (`findmnt`) and pre-selected —
  Timeshift's rsync mode on ext4/xfs, ZFS where a pool exists — and when the
  filesystem supports more than one engine the user can choose, with the
  recommended engine tagged and listed first.
- List / create / delete run through `pkexec`; restore is wired for Timeshift
  (reboots to finish) and ZFS. Long operations run off the UI thread behind a
  loading overlay.
- **Home backup** mirrors the home directory to another disk with
  `rsync -a --info=progress2` (optional `--delete` mirror mode), showing a live
  percentage and a cancel button. Caches and the trash are excluded. No root — the
  files are the user's own.

### Recovery (new page)
- A graphical front-end for **PhotoRec** (the `testdisk` suite, wrapped — not
  bundled). Enumerates partitions and whole disks via `lsblk`, marks mounted
  ones, and carves lost/deleted files by signature via `photorec … /cmd … search`
  through `pkexec`.
- **File-type selection** — recover everything, or pick from ~26 common families;
  and an option to **sort every recovered file into a per-type folder**.
- Safety: refuses a destination on the same disk being carved, warns on a mounted
  source, and shows a live recovered-file count with cancel.

### Safer cleaning
- System Cleaner offers to create a **Timeshift restore point** before an
  irreversible root-level cleanup (old kernels, rotated logs, crash dumps); if the
  snapshot fails, nothing is cleaned.

### UI
- **Sidebar reorganized by workflow** (Monitor → Maintenance → Backup & Recovery →
  Control → Config → App); the visual order is decoupled from the fixed page
  indices. Distinct icons throughout (Backup, Power no longer duplicate others).
- **Status pills** on Firewall (Active / Inactive) and Backup (active engine),
  sharing one helper with System Relief.
- **`EmptyState`** placeholder when PhotoRec is not installed.
- **Settings fixes:** the page now scrolls on short windows instead of squeezing
  its rows; its `QFormLayout` groups align correctly (label/field) in LTR and RTL;
  the "Apply" button is pinned at the bottom.
- **Runtime language switch** now re-translates the code-built pages (Relief,
  Connections, Power, Firewall, Backup, Recovery), not only the `.ui` pages.

## [26.08-stable] — 2026-07-31

The "network & power tools" release. Three new pages — Connections, Power and
Firewall — plus a cross-desktop power-integration layer and a program-wide
timer-efficiency pass. New runtime dependency: `Qt6::DBus` (ships inside
`qt6-base`, so no new distro package).

### Connections (new page)
- Lists every active TCP/UDP socket by parsing `ss -tunaHp`, with the owning
  process/PID, a live text filter (address / port / process / state), sortable
  columns, and an optional auto-refresh that only runs while the page is shown.
- A "show processes of all users" toggle runs `ss` through `pkexec` to reveal
  sockets owned by other users.

### Power (new page)
- **Power profile** switching via `power-profiles-daemon` (Power Saver /
  Balanced / Performance), falling back to raw **cpufreq governors** (written to
  every CPU via `pkexec`) where the daemon is absent. Works on desktops too — it
  drives the CPU governor, not the battery.
- **Battery charge limit** (laptops): reads/writes
  `charge_control_{start,end}_threshold` to cap charging (e.g. 80 %) and extend
  battery lifespan. The whole battery section hides on machines without one.
- **Keep awake** — block automatic sleep and screen locking, the cross-desktop
  way. Uses the freedesktop D-Bus interfaces every major desktop implements
  (`org.freedesktop.PowerManagement.Inhibit` and, as a fallback, `ScreenSaver`),
  with `systemd-inhibit` (logind) as a last resort. A **single** inhibitor is
  held to avoid duplicate entries in the desktop's power UI.
- **Two-way desktop integration.** Our block appears in the desktop's own power
  applet, and `PowerManagement.Inhibit.HasInhibit()` lets us detect a block set
  from elsewhere (e.g. KDE's own "Manually block") — which never shows up in
  `systemd-inhibit`. A tray-icon badge and a desktop notification signal the
  keep-awake state even when the window is closed.

### Firewall (new page)
- Enable/disable **ufw** or **firewalld**, list rules, and add or remove port
  rules (port + tcp/udp + allow/deny). The enabled state is read without root
  from `ufw.conf`; each rule change runs its mutation **and** re-lists in one
  `pkexec` call, so the user authorizes **once** per action instead of twice.

### Performance
- Dashboard, Resources, Processes (and the new Connections / Power pages) now
  pause their refresh timers when the page isn't the current view — previously a
  visited page kept polling `/proc` in the background. The Settings **power
  timer** is `keepAlive` so a scheduled shutdown still fires while minimised.

### Earlier in the cycle (folded into 26.08)
- **System Relief**: settings persisted; automatic mode can start with the app
  (materialised at launch); an in-place, flicker-free candidate table with a
  memory bar, a colored status badge, and a select-all button.
- **Language**: an "Auto (system language)" default that follows the system
  locale (English when unsupported) and is remembered once changed; flag emoji
  are rendered as icons so they show correctly on KDE, not just GNOME.
- **Appearance/UI**: GPU memory label corrected to "ذاكرة البطاقة"; the About
  version line is generated from `APP_VERSION`.

### Build / packaging
- Version bumped to **26.08** (CMake 26.8.0, `APP_VERSION="26.08"`).
- `Qt6::DBus` added to `find_package` and the link line.

---

## [26.07-stable] — 2026-07-11

Field-test follow-up to 26.06. Focus: making long operations visibly
in-progress, fixing autostart, two new headline tools (System Relief and a
Power timer), and finishing the Arabic UI to 100 %.

### Startup Applications
- **Autostart entries now actually launch.** Entries were written through
  `QSettings`, which mangles a `.desktop` file (value quoting / unicode
  escaping) so desktops silently ignored them. `StartupTool` now writes
  freedesktop-compliant `.desktop` files as plain UTF-8 text; `enable`/
  `disable` edit them in place the same way.
- **Flatpak & Snap apps** are now listed in the "Add…" browser (Snap desktop
  files under `/var/lib/snapd/desktop/applications`, alongside the existing
  Flatpak exports).
- **Start delay** — each autostart entry can wait N seconds after login before
  launching (wrapped as `sh -c "sleep N && exec …"`, field codes stripped).
  The configured delay is shown on the row.
- **Fix — the list now refreshes when you open the page.** An entry added
  elsewhere (e.g. the Settings "start on login" checkbox) used to be invisible
  here until a manual Refresh, which looked like "add didn't work". The page now
  re-scans `~/.config/autostart` on every `showEvent`.

### Settings
- **Start GT-STACER on system login** — a new checkbox that reflects the real
  state of the managed `~/.config/autostart` entry and writes/removes it on
  apply (AppImage uses `$APPIMAGE`, otherwise the installed binary path). The
  checkbox re-syncs with the on-disk entry each time the page is shown, so it
  never displays a stale state.
- **Power timer** — schedule Shut down / Restart / Suspend (to RAM) /
  Hibernate (to disk) after a set number of minutes, with a live countdown and
  a cancel button. Runs through `systemctl` (logind/polkit), so it works under
  Flatpak too. A confirmation guards against an accidental schedule. No root
  password is requested because logind grants an active local session these
  actions — the same reason the desktop's own shutdown button doesn't ask.
- **Fix — hibernate availability now asks logind.** `isAvailable()` consulted
  only `/sys/power/state`, which can advertise `disk` even when no usable
  swap/resume exists; the timer would then schedule a hibernate that silently
  failed. It now queries logind's `CanSuspend`/`CanHibernate` (falling back to
  the kernel state only if logind is unreachable), so unsupported modes are
  correctly greyed out.

### System Relief (new page)
- Temporarily **freezes idle, non-critical, user-owned processes** (SIGSTOP) to
  relieve RAM/CPU pressure, then thaws them (SIGCONT). Freezing is fully
  reversible — no process state is lost.
- Safety layers: only the current user's own processes are eligible;
  `ProcessInfo::isCriticalProcess` plus a desktop-shell denylist (compositors,
  panels, portals, input methods) are excluded; GT-STACER never freezes itself;
  everything frozen is resumed on exit.
- **Fix — never freeze the launching session.** Relief could suspend the very
  terminal, shell, or `node`/agent process that launched GT-STACER (its comm may
  be a version string, not `node`), freezing the user's session. `candidates()`
  now excludes (1) GT-STACER's full ancestor chain walked via `/proc/<pid>/stat`,
  (2) every process in its controlling-terminal session (`getsid`), and (3) a
  name denylist of terminals, shells and multiplexers. Both the manual and
  automatic paths are covered.
- **Manual**: pick from a memory-sorted candidate list and "Relieve now",
  optionally dropping clean file caches (`vm.drop_caches=3` via pkexec).
- **Automatic mode** (off by default): watches CPU/RAM and freezes idle apps
  after sustained pressure, thawing them once it clears. Its watchdog keeps
  running in the tray.

### Uninstaller
- **Visible progress** — a full-page spinner overlay with a live "Removing X
  (i of n)…" message now dims the page and blocks interaction during removal,
  so a slow uninstall no longer looks like a hang.
- **Externally / manually installed apps are now detected** (`External`). Apps
  dropped in by install scripts (e.g. Megacubo via `wget | bash`), tarballs
  under `/opt`, or AppImages are found by scanning `.desktop` launchers whose
  `Exec` lives under `/opt`, `/usr/local`, `~`, or ends in `.AppImage` — none of
  which any package manager tracked, so they were previously impossible to
  remove from here.
- **Respects shared installs.** A single batched ownership query
  (`dpkg -S` / `rpm -qf` / `pacman -Qo`) drops anything a package manager owns,
  so e.g. a Brave PWA launcher pointing at `/opt/brave.com` never offers to
  delete the whole browser.
- Removal is end-to-end: the `.desktop` launcher, the `/opt` payload (or the
  AppImage/binary), and per-user leftovers in `~/.config`, `~/.cache`,
  `~/.local/{share,state}` — guarded by a path allowlist that can never delete a
  shared system root.

### Appearance
- **Light theme rebuilt.** The light (Catppuccin Latte) theme was badly broken —
  a dark sidebar, near-white titles that vanished on the light background, dark
  cards and table rows — because many colors were hard-coded for dark mode in
  C++ and the light QSS was missing ~150 lines of rules. A central `Theme`
  palette (`Managers/theme.*`) now resolves every painted widget's colors from
  the active theme (gauges, sidebar, cleaner cards), the remaining inline styles
  moved to theme-aware `objectName` + QSS, and the light stylesheet was brought
  to full parity with a modern, comfortable Latte look. The dark theme is
  unchanged.

### Translations
- **Arabic UI at 100 %** again (486/486), covering every new string.
- GPU is now "بطاقة الرسوميات" everywhere (was the transliterated "كرت").

### Build / packaging
- Version bumped to **26.07** (CMake 26.7.0, `APP_VERSION="26.07"`) across the
  DEB / RPM / AppImage / Flatpak scripts.

---

## [26.06-stable] — 2026-05-15

First **stable** release after the 26.05 beta. Every regression surfaced
during field testing is fixed here, plus three large additions: Flatpak/Snap
drill-down in the cleaner, multi-package uninstall, and a configurable
notifications panel. No architectural changes — only polish, breadth, and
safety.

### Processes
- **Multi-field search** (PID + name + cmdline + user). The 26.05 search hit
  only the truncated 15-char kernel `comm`, so AppImage and wrapper processes
  appeared "hidden" — a Stacer AppImage with `comm=AppRun` would not match
  `stacer`. The new proxy searches across all four fields simultaneously.
- **Smarter display name** — prefer the cmdline basename over `comm` when it
  carries more characters. AppImage entries now show as
  `Stacer-1.1.0-x64.AppImage` instead of `Stacer-1.1.0-x6`.
- **Five new actions** beyond plain Terminate: Force kill (SIGKILL),
  Suspend (SIGSTOP), Resume (SIGCONT), Lower priority, Raise priority — all
  exposed in a **right-click context menu**.
- **Critical-process guard**: kernel threads, PID 1, `systemd*`, `dbus`,
  `Xorg`/`Xwayland`, `gnome-shell`/`plasmashell`, `pipewire*`, `NetworkManager`,
  etc. trigger a red, escalated confirmation dialog before signalling.

### System Cleaner
- **Flatpak Apps** + **Snap Apps** cards (custom SVG icons) sit alongside
  Trash / App Cache / Package Cache. Double-click opens a per-app dialog
  with version, size, ID, search, sort, Select-All, and Remove-Selected —
  the same UX as App Cache and Package Cache.
- `PackageTool::flatpakApps()` parses `flatpak list --columns=name,application,version,size`;
  `PackageTool::snapApps()` parses `snap list` and reads the on-disk
  `.snap` blob size from `/var/lib/snapd/snaps` when present.
- **Dynamic package-cache card** — labeled `APT Cache` on Debian/Ubuntu,
  `DNF Cache` on Fedora, `Pacman Cache` on Arch, `Zypper Cache` on openSUSE,
  and so on. Description and target path follow the detected manager.
- **`Package Cache` drill-down** — list every `.deb` / `.rpm` / `.pkg.tar.zst`
  with size + last-modified, search, sort, per-file removal through a single
  `pkexec` prompt.
- **Cleanup support for 13+ managers** — `cleanCache()` now covers APT, DNF,
  DNF5, YUM, TDNF, Pacman/Yay/Paru, Zypper, XBPS, APK, Portage, Eopkg, Equo,
  Swupd, Nix, Flatpak, Snap and Homebrew. Every branch uses `execProgram`
  (no shell).

### Uninstaller — multi-select
- `ExtendedSelection` enabled; **Ctrl/Shift-click + Ctrl+A** queue up several
  rows at once. The Uninstall button re-labels itself ("Uninstall 5 packages")
  and the confirmation dialog lists every queued name.
- Removals run sequentially on a `QtConcurrent::run` worker thread; the
  status label updates "Removing X (3 of 5)…" between packages.
- Final summary reports successes vs. failures with the failing names so the
  user knows what to retry manually.
- The Uninstall button stays disabled until a row is selected — the 26.05
  beta silently no-op'd when users clicked the table without realising.
- **First-visit auto-load** — `Load Packages` was renamed to **Reload**; the
  scan starts automatically when the page first opens.

### Notifications & alerts
- The default disk-full notification was firing on tiny system partitions
  (`/boot`, `/efi`) — those are now skipped, along with `/snap`, `/run`, and
  any partition under 2 GB. Disk-threshold default raised to 95 %.
- New **Settings → Notifications & alerts** group with checkbox + 4 spin
  boxes (CPU/GPU °C, RAM %, Disk %, Battery %). `0` disables an individual
  threshold without disabling the whole alert system. Live-applied through
  `AlertManager::setEnabled()`.

### Translations
- **19 languages compiled at 100 %.** Every entry is `finished` from Qt's
  point of view, so `lrelease` produces a usable QM for each language.
- Arabic + English at full native coverage. French at **83 % native**
  (346 / 417); German / Italian / Portuguese / Russian / Turkish at 33–43 %.
- Per-language fallback policy: when a dictionary doesn't cover a string,
  the English source is used as the visible text rather than an empty
  `unfinished` entry. Better than blank, and contributors can refine each
  string over time. The QM ships clean regardless.

### Performance / power
- **Hide-event timer pause**: when the window is minimised to the tray,
  every page's `QTimer` is stopped (Dashboard refresh, Resources charts,
  Processes refresh, Services refresh). `showEvent()` restarts only those
  that were previously running, tagged via a dynamic `wasActive` property.
- CpuSampler keeps running for the tray tooltip but is already on its own
  thread and already throttled to 1 Hz.

### Build / packaging
- Version bumped to **26.06** (CMake 26.6.0, `APP_VERSION="26.06"`,
  `APP_CHANNEL="stable"`).
- **Flatpak packaging added** — `org.gnutux.gt-stacer` against
  `org.kde.Platform//6.9`. Manifest at
  `packaging/flatpak/org.gnutux.gt-stacer.yaml`, AppStream metainfo at
  `packaging/flatpak/org.gnutux.gt-stacer.metainfo.xml`, build script
  `packaging/build-flatpak.sh`. Sandbox-aware command layer in
  `CommandUtil`: when `FLATPAK_ID` is set (or `/.flatpak-info` exists),
  every `execProgram`, `execProgramOutput`, `commandExists` and
  `pkexecWriteFile` call is rewritten to go through `flatpak-spawn --host`,
  with the write-via-temp-file path relocated from `/tmp` to
  `~/.cache/gt-stacer-tmp/` so the host can see the staged content. Tray
  works without `--own-name` (newer Flatpak rejects the glob form), the
  `StatusNotifierWatcher` registers items on demand.
- Resized hicolor icons to their proper sizes (16/32/48/64/128/256) —
  required by Flatpak's strict icon validator.
- `scripts/build-all.sh` learns a new `flatpak` target and lists
  `flatpak-builder` in `install-deps` for every supported distro.
- DEB / RPM / AppImage / Flatpak rebuilt and verified; SHA-256 sums in
  `release/SHA256SUMS.txt`.

---

## [26.05-beta] — 2026-05-14

### Security
- **Command-injection fix in `/etc/hosts` editor** — `saveHosts()` previously
  passed user content through `pkexec sh -c 'echo "%1" > /etc/hosts'`, allowing
  arbitrary code execution as root. Now writes via a temp file +
  `pkexec install -o root -m 644`.
- Sanitize all package and service names through `CommandUtil::isSafeIdentifier`
  before passing to `pkexec`.
- Refactor `PackageTool::remove`, all `ServiceTool` actions, and
  `AptSourceTool` to use `execProgram(prog, args)` — no shell, no injection.
- Add Subresource Integrity (SRI) hash to the Font Awesome CDN link in the
  website.

### Performance
- **Background CPU sampler** — `CpuInfo::usage()` no longer blocks the UI for
  ~200 ms per call; a dedicated `gt-stacer-cpu` worker thread samples
  `/proc/stat` once per second under a mutex.
- **Replace QtCharts with `LineChart` widget** — a custom QPainter-based
  chart removes the dependency on `libQt6Charts` (which pulled in 8+ QML
  libraries), cutting RAM use by ~70 % and `Qt6` runtime libs by 9.
- **`/proc`-based `ProcessInfo`** — no more `ps` shell-out; processes
  enumerate from `/proc/{pid}/{stat,status}` directly in ~30 ms for 277
  processes.
- **In-place model updates** — `ProcessesPage` and `ServicesPage` now reuse
  rows by PID/name instead of `setRowCount(0)`; selection and sort survive.
- **Lazy page construction** — pages other than the Dashboard are built on
  first navigation, not at startup. Lower initial RAM and faster cold start.
- `appendPoint` is `O(1)` (ring buffer + batch `replace(QList)`) instead of
  O(N) shift.
- Cmdline truncated to 120 chars at parse time — significant savings with
  hundreds of processes.

### UX
- **System Cleaner — icon-card redesign**: 7 categories with per-category
  checkboxes, "Select All", info badge for drill-down support.
- **App Cache details dialog** (drill-down on the App Cache card): lists
  every subdirectory of `~/.cache` with size + last-modified, with search,
  sort (largest/oldest/newest/A→Z), and per-row selection.
- **Confirmation dialog before any clean** — lists every selected category
  with its size; sensitive categories are tagged `· requires root`.
- **Cleaning fix for `/var/log` / `/var/cache`** — now uses `pkexec find -delete`
  (rotated logs only for `/var/log`) and `pkexec apt-get clean`.
- **Startup Apps redesign** — icons resolved from `Icon=`, inline ON/OFF
  toggle, Remove button per row, "Add…" dialog with system-app browser +
  manual entry.
- **Quit confirmation dialog** when invoking Quit from the tray, with a
  "Don't ask again" checkbox.
- **About dialog** (F1 / tray menu).
- **Keyboard shortcuts** — Ctrl+1..9/0 (pages), Ctrl+R (refresh), Ctrl+Q
  (quit), Ctrl+, (settings), F1 (about).
- **Wayland taskbar icon** — `setDesktopFileName("gt-stacer")` so the
  compositor resolves the icon from the `.desktop` file instead of falling
  back to a generic glyph.

### Features
- **libnotify alerts** for CPU/GPU temperature, memory, disk usage, and
  battery thresholds (all configurable; 5-minute per-key cooldown).
- **Per-core CPU bars** on the Resources page (uses `cpu.perCore` data that
  was previously collected but unused).
- **CPU temperature** under the Dashboard CPU gauge.
- **Disk I/O monitor** — read/write speeds from `/proc/diskstats` shown on
  the Dashboard.
- **Auto-follow system theme** option (`Qt 6.5+` `QStyleHints::colorScheme`).
- **Helpers expansion** — DNS resolver cache flush (systemd-resolved /
  nscd / unbound), `vm.swappiness` editor, `/proc/cmdline` viewer, locale
  display.

### Translations
- **Arabic complete** — 342/342 strings translated (100 %).
- **English** — identity translation file added.
- **17 other languages** at 18-38 % coverage (most common UI strings).

### Build
- `-Os -ffunction-sections -fdata-sections` + `-Wl,--gc-sections --as-needed`
  for release builds.
- Binary stripped at install time.
- Dropped Qt6Charts dependency from the `find_package` block and packaging
  scripts (DEB / RPM).

---

## [26.04-alpha] — 2026-04-27

### Added
- Initial Qt6 / C++17 port from Stacer 1.x.
- 28+ package manager detection (APT, DNF, Pacman, Zypper, XBPS, APK,
  Portage, Nix, Flatpak, Snap, …).
- GPU monitoring (Intel / AMD / NVIDIA).
- Temperature sensors via `hwmon` + thermal zones.
- Battery info.
- AppImage / DEB / RPM packaging scripts.
- Dark + light QSS themes.

---

[26.11.3-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.11.3_STABLE
[26.11.2-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.11.2_STABLE
[26.11.1-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.11.1_STABLE
[26.11-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.11_STABLE
[26.10-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.10_STABLE
[26.09-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.09_STABLE
[26.08-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.08_STABLE
[26.07-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.07_STABLE
[26.06-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.06_STABLE
[26.05-beta]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.05_BETA
[26.04-alpha]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER-26.04-x86_64-ALPHA
