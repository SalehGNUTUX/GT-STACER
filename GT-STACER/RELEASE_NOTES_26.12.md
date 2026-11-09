# GT-STACER 26.12 stable

**Release date:** 2026-11-09
**Channel:** stable
**License:** GNU GPL v3 or later

A **large feature release**. The Helpers page gains three native tools that
replace the author's separate utilities (gt-imt, gt-usdr, GT-GMT) after a bug
review of each; System Relief learns which app you are working with; the
Package Manager's Upgrades tab covers every source at once; and every failed
command now explains itself.

## What's new

- 💿 **Disk images** — mount ISO / IMG / disk images (hybrid distro ISOs too)
  by drag & drop, read-only or writable, through UDisks2 (no password), and
  extract them with a Stop button.
  - **CD / DVD / Blu-ray**: the disc in the drive is detected; **Save as ISO**
    copies it into an identical image (written to `.part`, renamed only when
    complete), **Open** and **Eject**.
  - **Burn ISO** onto a disc with xorriso (no password), blanking a rewritable
    disc when needed, then **verifies** the disc against the image.
- 🔌 **USB drives** — USB sticks and memory cards only (internal disks never
  appear): mount / unmount, check and repair the filesystem, **format** (FAT32,
  exFAT, NTFS, ext4 …, a partition or the whole device), **SMART health**,
  a read-only **surface scan** for bad blocks, and **back up to an image**. The
  device's identity (serial + size) is re-checked right before anything
  destructive, and partitions are mounted again after a scan or backup.
- 🥾 **Boot (GRUB)** — timeout, default entry (stored by its stable id),
  resolution, kernel parameters, OS detection and themes. Every change is ONE
  transaction: backup → `sh -n` → the menu generated into a temporary file →
  `grub-script-check` → replace; any failure puts everything back. It refuses to
  write if the file changed outside GT-STACER, keeps named **restore points**,
  and reinstalls GRUB on the disk that really holds `/boot` (with UEFI and
  Secure Boot guards).
- ⚡ **Smarter System Relief** — the heaviest app you are working with (CPU +
  disk, its background processes included) keeps the machine; others are paused
  in stages (idle first, then busy) and resumed with a safety margin. The
  active window, audio and sleep-inhibiting apps are never paused. **Automatic
  relief is now on by default** (a choice you already saved is kept). Apps are no
  longer left frozen if GT-STACER is closed or killed — they are resumed at the
  next start.
- 📦 **Packages**
  - **Upgrades from every source** — APT, DNF, Pacman, Zypper, Flatpak, Snap in
    one list with installed / new versions and a Source column; untick whole
    sources or single packages, then upgrade with one command per source. Lists
    are refreshed first without a password where the system allows it. APT runs
    non-interactively and keeps your config files; a partial Arch upgrade is
    refused with an explanation.
  - **Local packages** — install a downloaded `.deb`, `.rpm`, Arch package,
    Flatpak bundle, Snap or AppImage, with every dependency checked first.
- 🩺 **Plain-language failures** — a failed command says why (password
  dismissed, not authorized, tool missing, package-manager lock …) with the
  details one click away. A crash is recorded and reported at the next start.
- ✨ **One waiting sign** — the same animated card, naming the task and its
  step, everywhere a task runs without a Stop button. The Firewall no longer
  freezes the window while the password is asked.
- 🔤 **Arabic text mixed with commands, paths or English names** is no longer
  scrambled, app-wide.
- 💾 **Backup**: Timeshift's real settings, where restore points are stored,
  changes made in Timeshift noticed live, rsync to FAT32 / exFAT / NTFS / SMB,
  and Stop that really stops rsync and PhotoRec — even after a force-close.

The full list is in [CHANGELOG.md](CHANGELOG.md).

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.12-x86_64.AppImage` | 53 MB | `91a3fa26be572bad71b8bb26a4a5060935400f778398021eb1d86d91fc341972` |
| `GT-STACER_26.12_amd64.deb` | 2.4 MB | `712c71b9f5c446a60ba213c9fead4b76b520af4268b7993d88335ebd5853138b` |
| `gt-stacer-26.12-1.x86_64.rpm` | 2.8 MB | `c6ab4700241bf067c94590a9777bcaabbd43ebc9d5d3eaf2e01b3f2a2501cf74` |
| `GT-STACER-26.12-x86_64.flatpak` | 2.5 MB | `c203b3755f11fee809108160c1f9103cf5d8405a069560753d29782c5a9e44c5` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.12_amd64.deb
sudo apt-get install -f
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE
```bash
sudo dnf install ./gt-stacer-26.12-1.x86_64.rpm
```

### Any distribution — AppImage
```bash
chmod +x GT-STACER-26.12-x86_64.AppImage
./GT-STACER-26.12-x86_64.AppImage
```

### Flatpak
```bash
flatpak install --user GT-STACER-26.12-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

Optional tools used by the new tabs (each is explained in the app if missing):
`xorriso` (burning), `smartmontools` (health), `e2fsprogs` / `dosfstools` /
`exfatprogs` / `ntfs-3g` (check, repair, format), `p7zip` or `bsdtar`
(extracting ISO images).
