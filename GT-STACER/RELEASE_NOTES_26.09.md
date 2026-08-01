# GT-STACER 26.09 stable

**Release date:** 2026-08-01
**Channel:** stable
**License:** GNU GPL v3 or later

GT-STACER 26.09 is the **backup, snapshots & recovery** release — a Qt 6 / C++ 17
Linux system optimiser and monitor by GNUTUX. It adds two new pages (Backup and
Recovery) that turn GT-STACER into a data-safety tool: create system restore
points, mirror your home directory, and carve back files you thought were gone.

## Highlights

- 💾 **Backup (new).** Two independent tools in one page. **System snapshots**
  create restore points through **Timeshift**, **Snapper** or **ZFS** — the ideal
  engine for your root filesystem is detected and pre-selected (Timeshift's rsync
  mode on ext4/xfs, ZFS where a pool exists), and when your filesystem supports
  more than one engine you can pick, with the recommended one tagged and placed
  first. **Home backup** mirrors your home directory to another disk with `rsync`,
  showing live progress and skipping caches and the trash — no root needed, they
  are your own files.
- ♻️ **Recovery (new).** A graphical front-end for **PhotoRec** that carves lost or
  deleted files from a disk, partition or image by signature. Pick a source and a
  destination on a *different* disk (guarded — it refuses to recover onto the disk
  you are rescuing), choose which file types to look for, and have every recovered
  file sorted into its own per-type folder. Reading a raw device needs
  authorization; PhotoRec itself is the upstream `testdisk` binary, wrapped, not
  bundled.
- 🛟 **Restore point before a risky clean.** When System Cleaner is about to run an
  irreversible root-level cleanup (removing old kernels, deleting rotated logs or
  crash dumps), it offers to create a Timeshift restore point first — and if that
  fails, nothing is cleaned.
- 🧭 **Sidebar reorganized by workflow.** Monitor → Maintenance → Backup &
  Recovery → Control → Config → App, so related pages sit together. Distinct
  icons throughout (no more look-alikes).
- 🏷️ **Status pills.** At-a-glance state badges on Firewall (Active / Inactive)
  and Backup (the active snapshot engine), matching System Relief.
- 🪟 **Layout & translation fixes.** Settings now scrolls on short windows instead
  of squeezing its rows, its form fields align correctly in both LTR and RTL, and
  every page re-translates correctly after a language switch while running.

For the full list, see [CHANGELOG.md](CHANGELOG.md).

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.09-x86_64.AppImage` | 52 MB | `fa24fb2b82995a4bd277b487ec0d9c8098054900301f795ccb7b9568c64aaf94` |
| `GT-STACER_26.09_amd64.deb` | 2.1 MB | `ef3e60e9ac2056d46755d6d7d2db75cc4e29dfb31490ef381a1c2c397d0e6000` |
| `gt-stacer-26.09-2.x86_64.rpm` | 2.4 MB | `7bb15478891b819ea852cb3d187e15fd8707a0832e2476865160d6aa13ba7ffb` |
| `GT-STACER-26.09-x86_64.flatpak` | 2.1 MB | `85c0a86b6df0fde685718212b2bfe3c06992b2ce61d178c0c3a97ca36ffc050a` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.09_amd64.deb
sudo apt-get install -f          # fix any missing deps
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE (via alien-built RPM)
```bash
sudo dnf install ./gt-stacer-26.09-2.x86_64.rpm
```

### Anywhere (AppImage — no install needed)
```bash
chmod +x GT-STACER-26.09-x86_64.AppImage
./GT-STACER-26.09-x86_64.AppImage
```

### Flatpak (sandboxed)
```bash
flatpak install --user GT-STACER-26.09-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

The Flatpak builds against `org.kde.Platform//6.9` and routes every privileged
op (cleaner, services, uninstaller, firewall, power, snapshots, recovery) through
`flatpak-spawn --host`, so the polkit prompt is identical to the DEB/RPM case.

## Dependencies

No new hard dependency this release. GT-STACER still does not depend on QtCharts.
The new pages lean on tools detected at runtime and hidden when absent.

| Distro family | Runtime deps |
|---|---|
| Debian / Ubuntu | `libqt6core6t64` (or `libqt6core6`), `libqt6gui6t64`, `libqt6widgets6t64`, `libqt6svg6`, `libqt6dbus6`, `libqt6network6` · recommended: `polkitd`, `flatpak`, `libnotify-bin`, `ufw`, `rsync` |
| Fedora / RHEL | `qt6-qtbase`, `qt6-qtsvg` · recommended: `polkit`, `flatpak`, `libnotify`, `ufw`, `rsync` |
| Arch | `qt6-base`, `qt6-svg` · recommended: `polkit`, `flatpak`, `libnotify`, `ufw`, `rsync` |

Optional integrations, detected at runtime and hidden when absent:
`timeshift` / `snapper` / `zfs` (snapshots), `rsync` (home backup),
`photorec` from the `testdisk` package (recovery), `power-profiles-daemon`,
`ss`/iproute2, `ufw`/`firewalld`.

## Known limitations

- RPM is built via `alien` from the DEB on Debian. A native `rpmbuild` path is in
  the spec file but needs Fedora/openSUSE.
- Snapshot **restore** is wired for Timeshift and ZFS; Snapper restore is not yet
  exposed (create/list/delete are). Timeshift restores reboot the machine.
- Recovery needs the `testdisk` package installed and reads raw devices as root;
  always recover to a disk other than the one being carved.

## Reporting issues

- Bug reports: <https://github.com/SalehGNUTUX/GT-STACER/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md) — please do not open
  public issues.

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
