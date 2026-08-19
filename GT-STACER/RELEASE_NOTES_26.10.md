# GT-STACER 26.10 stable

**Release date:** 2026-08-19
**Channel:** stable
**License:** GNU GPL v3 or later

GT-STACER 26.10 is the **responsiveness & onboarding** release — a Qt 6 / C++ 17
Linux system optimiser and monitor by GNUTUX. It teaches System Relief about disk
I/O (the real cause of many "frozen but CPU is idle" stalls on old drives), brings
the Power timer to the Power page, and adds a translated welcome tour, an
after-update "what's new" dialog, an optional update check, and app sharing.

## Highlights

- 💽 **Disk-aware System Relief.** The page now reads **disk pressure (PSI,
  `/proc/pressure/io`)** and each candidate's **block-layer I/O rate**, so you can
  see *which* process is thrashing the disk. Two new levers beyond freezing:
  **Ease disk I/O** lowers a process's I/O priority (`ionice` idle) so the
  foreground gets a responsive disk without stopping it, and a one-click **Switch
  to BFQ** changes the disk scheduler — the single biggest win for an old HDD
  under load (it also makes the ionice hint actually bite). Automatic mode can now
  trigger on disk pressure, not only CPU/RAM.
- ⏻ **Power timer on the Power page.** The scheduled shutdown / restart / suspend /
  hibernate timer (with a live countdown that survives minimize-to-tray) is now
  available on the Power page too, mirroring the one in Settings.
- 🎬 **Welcome tour & "what's new".** The onboarding tour is re-openable from
  Settings, and a concise **"what's new"** dialog appears once after each update.
  Both are fully translated and carry an **in-dialog language picker** — the
  welcome tour is the natural place to choose your language on first run.
- 🔔 **Update check & sharing.** An **opt-in update check** (Settings → *Check for
  updates on startup*, or *Check now*) asks GitHub's public API whether a newer
  stable release exists and notifies you — nothing is downloaded. A **Copy share
  text** button puts a ready-to-post description + link + hashtags on the clipboard.
- 🧭 **Sidebar & polish.** Services now sits directly under Processes; the Arabic
  language entry is simplified to "العربية"; assorted fixes.

For the full list, see [CHANGELOG.md](CHANGELOG.md).

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.10-x86_64.AppImage` | 52 MB | `f5a9bf09be8268fcdd9cea8953fa16f7059af154a6688e234a0fcf7f2e125592` |
| `GT-STACER_26.10_amd64.deb` | 2.1 MB | `5c0e003fbd8c9178737d9826ac7b8451de3fcb6f44b617d57e46d4706f983348` |
| `gt-stacer-26.10-2.x86_64.rpm` | 2.4 MB | `9df4265ea5429abefe70d78da717693968ac06ddc119ffbf0db588b3f3ce6e4b` |
| `GT-STACER-26.10-x86_64.flatpak` | 2.1 MB | `a36633cce2dfe411d3b68d9f89937563d0fb7f74cc025a4ad0557e5f61eb80b1` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.10_amd64.deb
sudo apt-get install -f          # fix any missing deps
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE (via alien-built RPM)
```bash
sudo dnf install ./gt-stacer-26.10-2.x86_64.rpm
```

### Anywhere (AppImage — no install needed)
```bash
chmod +x GT-STACER-26.10-x86_64.AppImage
./GT-STACER-26.10-x86_64.AppImage
```

### Flatpak (sandboxed)
```bash
flatpak install --user GT-STACER-26.10-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

The Flatpak builds against `org.kde.Platform//6.9` and routes every privileged
op through `flatpak-spawn --host`, so the polkit prompt is identical to the
DEB/RPM case.

## Dependencies

No new hard dependency this release. GT-STACER still does not depend on QtCharts.
The disk-relief features use the kernel's PSI (`/proc/pressure/io`), `ionice`
(util-linux, already present) and, for the scheduler switch, `pkexec`.

| Distro family | Runtime deps |
|---|---|
| Debian / Ubuntu | `libqt6core6t64` (or `libqt6core6`), `libqt6gui6t64`, `libqt6widgets6t64`, `libqt6svg6`, `libqt6dbus6`, `libqt6network6` · recommended: `polkitd`, `flatpak`, `libnotify-bin`, `ufw`, `rsync` |
| Fedora / RHEL | `qt6-qtbase`, `qt6-qtsvg` · recommended: `polkit`, `flatpak`, `libnotify`, `ufw`, `rsync` |
| Arch | `qt6-base`, `qt6-svg` · recommended: `polkit`, `flatpak`, `libnotify`, `ufw`, `rsync` |

Optional integrations, detected at runtime and hidden when absent:
`timeshift` / `snapper` / `zfs`, `rsync`, `photorec` (testdisk),
`power-profiles-daemon`, `ss`/iproute2, `ufw`/`firewalld`, and the BFQ scheduler
module for disk relief.

## Known limitations

- RPM is built via `alien` from the DEB on Debian (revision `-2`).
- `ionice` idle-class only has strong effect under the **BFQ** (or CFQ) scheduler;
  on `mq-deadline`/`none` its effect is weak — switch to BFQ first. The scheduler
  change applies until reboot.
- Disk pressure (PSI) needs a kernel ≥ 4.20 with PSI enabled (default on most
  distros); the readout and disk-pressure trigger are hidden without it.
- The update check contacts GitHub only when enabled; it never downloads or
  installs anything.

## Reporting issues

- Bug reports: <https://github.com/SalehGNUTUX/GT-STACER/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md) — please do not open
  public issues.

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
