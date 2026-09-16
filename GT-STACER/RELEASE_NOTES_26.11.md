# GT-STACER 26.11 stable

**Release date:** 2026-09-16
**Channel:** stable
**License:** GNU GPL v3 or later

GT-STACER 26.11 is the **software-management** release — a Qt 6 / C++ 17 Linux
system optimiser and monitor by GNUTUX. The old Uninstaller grows into a full
**Package & Software Manager** (install, upgrade, remove — system managers,
Flatpak, Snap, opendesktop.org store add-ons and AppImages), the app can now
update itself, and a real command-injection hole was found and fixed.

## Highlights

- 📦 **Package & Software Manager (evolves Uninstaller).** A professional, tabbed
  page: **Installed** (browse & remove across every detected manager),
  **Search & Install** (search your system manager / Flatpak / Snap and install),
  **Upgrades** (list and apply available updates), **Store add-ons** (manage
  opendesktop.org / KNewStuff content — themes, icons, plasmoids… — and install
  via `ocs-url` links), and **AppImages** (below).
- 🧩 **AppImage integration, GearLever-compatible.** Integrate an AppImage into
  your app menu in one click — GT-STACER uses the **same folder and launcher
  format as GearLever** (`it.mijorus.gearlever`) and honours its settings (your
  configured folder, move-vs-clone), so apps integrated by either tool appear in
  both with no duplication. Metadata (icon, name) is extracted **without executing
  the AppImage** (a safe `unsquashfs` read at the squashfs offset).
- 🔄 **In-app updates.** GT-STACER checks for a newer version on startup (on by
  default) and can **download, verify (SHA-256) and install** the update itself —
  replacing the AppImage, or handing the `.deb`/`.rpm` to your package manager via
  `pkexec`. Checking for *package* updates stays manual. Nothing is downloaded or
  installed without your click.
- 🔒 **Security hardening.** Fixed a genuine **command-injection** vulnerability:
  package search passed your query to a shell (`sh -c`), so a crafted term could
  run arbitrary commands. It is now passed as argv (no shell) and validated. Every
  new install/upgrade/remove path goes through `execProgram` with validated names;
  store-add-on and AppImage removal are path-guarded.
- ⚡ **Processes: busiest first.** The Processes list now defaults to **CPU %,
  descending** (and sorts numerically, live) — you see what's using the machine
  the moment you open it, no clicking a header.
- 🌡️ **Tray tooltip temperature.** The tray tooltip now shows the CPU/system
  temperature alongside CPU % and RAM %.
- 🖱️ **Resizable table columns everywhere.** Every table's columns are now freely
  resizable with clearly visible, grabbable drag handles (the primary column still
  fills the width).

For the full list, see [CHANGELOG.md](CHANGELOG.md).

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.11-x86_64.AppImage` | 52 MB | `eb18159ee17bb723ac26bf938eab5658e21351968f868541c935a2f05e85ecb8` |
| `GT-STACER_26.11_amd64.deb` | 2.1 MB | `6352191beabf97ece2f6c77e07b46554baacd27477d22ba9e9e4bd3f14641323` |
| `gt-stacer-26.11-2.x86_64.rpm` | 2.4 MB | `b8636832af70eecc07674a68dc8c4ecf1ed4e9ed8240c2eb3039f7070d9f207d` |
| `GT-STACER-26.11-x86_64.flatpak` | 2.2 MB | `4081e6e096151e1abb545e64c6d5b489c0a89c800c92a66cc53e65f56aaeb6d4` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.11_amd64.deb
sudo apt-get install -f          # fix any missing deps
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE (via alien-built RPM)
```bash
sudo dnf install ./gt-stacer-26.11-2.x86_64.rpm
```

### Anywhere (AppImage — no install needed)
```bash
chmod +x GT-STACER-26.11-x86_64.AppImage
./GT-STACER-26.11-x86_64.AppImage
```

### Flatpak (sandboxed)
```bash
flatpak install --user GT-STACER-26.11-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

## Dependencies

No new hard dependency this release. Optional, detected at runtime and hidden when
absent: the store install helper `ocs-url` (or Plasma Discover) for one-click
opendesktop.org installs, and `squashfs-tools` (`unsquashfs`) for AppImage
integration. GearLever is not required — GT-STACER manages AppImages on its own,
and coexists with GearLever when it is present.

| Distro family | Runtime deps |
|---|---|
| Debian / Ubuntu | `libqt6core6t64` (or `libqt6core6`), `libqt6gui6t64`, `libqt6widgets6t64`, `libqt6svg6`, `libqt6dbus6`, `libqt6network6` · recommended: `polkitd`, `flatpak`, `libnotify-bin`, `squashfs-tools` |
| Fedora / RHEL | `qt6-qtbase`, `qt6-qtsvg` · recommended: `polkit`, `flatpak`, `libnotify`, `squashfs-tools` |
| Arch | `qt6-base`, `qt6-svg` · recommended: `polkit`, `flatpak`, `libnotify`, `squashfs-tools` |

## Known limitations

- RPM is built via `alien` from the DEB on Debian (revision `-2`).
- The install/search covers the system package manager plus Flatpak and Snap;
  language managers (pip / npm / cargo …) are planned for a later release.
- opendesktop.org add-ons: this release manages already-installed content and
  installs via `ocs-url` links; a full in-app store browser is a future feature.
- In-app self-update auto-installs for AppImage / DEB / RPM installs; Flatpak and
  source builds get clear guidance instead.

## Reporting issues

- Bug reports: <https://github.com/SalehGNUTUX/GT-STACER/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md) — please do not open
  public issues.

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
