# GT-STACER 26.06 stable

**Release date:** 2026-05-15
**Channel:** stable
**License:** GNU GPL v3 or later

GT-STACER 26.06 is the first **stable** release of the modernised Stacer fork
— a Qt 6 / C++ 17 Linux system optimiser and monitor by GNUTUX. Every issue
surfaced during the 26.05 beta field-test is fixed here, plus three large
additions: Flatpak/Snap drill-down in the cleaner, multi-package uninstall,
and a configurable notifications panel.

## Highlights

- 🔍 **Multi-field process search** — name + PID + cmdline + user. AppImage
  and wrapper processes that the 26.05 beta hid (because the kernel `comm`
  is truncated to 15 chars) now show up correctly.
- 🛑 **Six process actions** (Terminate · Force kill · Suspend · Resume ·
  Lower priority · Raise priority) via a right-click context menu, with a
  red escalated-confirmation dialog for critical system processes
  (`systemd*`, `dbus*`, `Xorg`/`Xwayland`, `gnome-shell`/`plasmashell`,
  `pipewire*`, `NetworkManager`, kernel threads, PID 1).
- 🧹 **Flatpak / Snap drill-down** in System Cleaner — pick exactly which
  applications you want to remove, with size + version + ID + search/sort.
- 📦 **Dynamic package-cache card** — labelled "APT Cache" on Debian,
  "DNF Cache" on Fedora, "Pacman Cache" on Arch, "Zypper Cache" on openSUSE…
  follows the host's actual package manager.
- 🗑️ **Multi-package uninstall** — Ctrl/Shift-click selects several rows;
  the button re-labels itself and the worker thread reports progress
  ("Removing X (3 of 5)…") between packages.
- 🔔 **Configurable notifications** — full Settings UI for the alert
  thresholds (CPU/GPU °C, RAM %, Disk %, Battery %); any individual
  threshold can be `0` to disable.
- ⚡ **Timer pause on hide** — every page's `QTimer` stops when the window
  is minimised to the tray, restored on `showEvent()`. CPU usage drops to
  near zero while the tray icon stays live.
- 🌍 **19 languages compiled at 100 %** — every entry is `finished`; the
  per-language native-translation rate ranges from 11 % (Kannada, Malayalam)
  to 100 % (Arabic, English), with English fallback used for the rest so
  the UI never shows a blank string.

For the full list, see [CHANGELOG.md](CHANGELOG.md).

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.06-x86_64.AppImage` | 54 MB | `7a47118a3319208ed0600d053bc63056f215e6e463f5e0bb0bfbfc9a07e30ac4` |
| `GT-STACER_26.06_amd64.deb` | 1.8 MB | `89e885d9bfbf1ff7d05856b6a39fb02c34e22c0e36b7693b768d1219be75553b` |
| `gt-stacer-26.06-2.x86_64.rpm` | 8.2 MB | `5304e80737c8f9e2587adfab67a5e691779d6b2708fd6088d44192cb21d880b2` |
| `GT-STACER-26.06-x86_64.flatpak` | 2.0 MB | `7322326e4bd3f1022e6c5a8ab99be991880b95f6c7bfdb5a60210a00650341bc` |
| `SHA256SUMS.txt` | 380 B | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.06_amd64.deb
sudo apt-get install -f          # fix any missing deps
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE (via alien-built RPM)
```bash
sudo dnf install ./gt-stacer-26.06-2.x86_64.rpm
# or
sudo rpm -i ./gt-stacer-26.06-2.x86_64.rpm
```

### Anywhere (AppImage — no install needed)
```bash
chmod +x GT-STACER-26.06-x86_64.AppImage
./GT-STACER-26.06-x86_64.AppImage
```

The AppImage bundles both XCB and Wayland Qt platform plugins, so it works
on every modern desktop environment (GNOME, KDE Plasma, XFCE, Cinnamon,
MATE, LXQt, Hyprland, Sway, …).

### Flatpak (sandboxed)
```bash
flatpak install --user GT-STACER-26.06-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

The Flatpak builds against `org.kde.Platform//6.9` and uses
`flatpak-spawn --host` for every privileged operation (cleaner, service
manager, uninstaller, `/etc/hosts` editor, sysctl). `pkexec` runs on the
host, not inside the sandbox, so the polkit prompt is unchanged. The
sandbox is detected at runtime via `$FLATPAK_ID` and `/.flatpak-info`,
and `~/.cache/gt-stacer-tmp/` is used in place of `/tmp` for the
write-via-temp-file path so the host can see the staged content.

## Dependencies

GT-STACER no longer depends on QtCharts (replaced with a custom widget),
which cuts the install size and unblocks distros that don't ship qt6-charts.

| Distro family | Runtime deps |
|---|---|
| Debian / Ubuntu | `libqt6core6t64` (or `libqt6core6`), `libqt6gui6t64`, `libqt6widgets6t64`, `libqt6svg6` · recommended: `polkitd`, `flatpak` |
| Fedora / RHEL | `qt6-qtbase`, `qt6-qtsvg` · recommended: `polkit`, `flatpak` |
| Arch | `qt6-base`, `qt6-svg` · recommended: `polkit`, `flatpak` |

## Known limitations

- RPM is currently built via `alien` from the DEB on Debian. A native
  `rpmbuild` path is in the spec file but requires Fedora/openSUSE deps;
  community pull-requests welcome.
- Battery monitoring shows `0` on systems without `/sys/class/power_supply`.
- Snap cache cleaning requires `snapd` to be installed (no-op otherwise).
- Translations other than Arabic / English / French are partial — the UI
  falls back to English for missing strings rather than showing blanks.

## Reporting issues

- Bug reports: <https://github.com/SalehGNUTUX/GT-STACER/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md) — please do not
  open public issues.

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
