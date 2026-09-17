# GT-STACER 26.11.3 stable

**Release date:** 2026-09-17
**Channel:** stable
**License:** GNU GPL v3 or later

A **UX-focused point release** from a testing pass on the Package Manager and
Recovery pages.

## What's new

- ☑️ **Checkbox selection in tables — no more Ctrl-click.** Every action table
  (the Package Manager's Installed, Search & Install, Upgrades, Store add-ons and
  AppImages tabs, plus the Processes list) now has a tick box per row and a
  **Select all** toggle. Pick several items for install / upgrade / remove / kill
  without holding a modifier key; a single row selection still works too.
- 📜 **A live progress-log window, with a Stop button.** Package install / upgrade
  / uninstall now run in a window that streams the command output line by line,
  shows a busy indicator, and can be **stopped** — the cancel control that was
  missing.
- 🔔 **Completion notifications for long tasks.** Any operation you start and leave
  running — package install/upgrade/remove, System Cleaner, Home backup, File
  Recovery — raises a desktop notification when it finishes, so you learn it's done
  even after moving to another section.
- 🗂️ **Recovery: the file-type list fills the window** instead of a cramped box,
  and dims while "Recover all file types" is on (the cue that every format is
  already included).
- 🔎 **Search & Install** gives feedback when the box is empty instead of doing
  nothing.

No behavioural change to the security model — package commands still run as
validated argv (no shell) through `pkexec` when root is needed.

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.11.3-x86_64.AppImage` | 52 MB | `f261e5be8041186bc37cd7834cfc80c078efe552d68cd380d0cfe74d79e5eddc` |
| `GT-STACER_26.11.3_amd64.deb` | 2.1 MB | `e23c3e61e245baa1b953ac4e7a23015b6c7395b4393273b6f24376fec87522f0` |
| `gt-stacer-26.11.3-2.x86_64.rpm` | 2.5 MB | `9aeba8dcb95c67a375310c876e808fd1e595d1886e5d9845523fb5cb2ce32fd7` |
| `GT-STACER-26.11.3-x86_64.flatpak` | 2.2 MB | `cae91ac04d80fce7b66514ec8164464f0c496c67aa0b32a7ecd3da13c0ba0860` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.11.3_amd64.deb
sudo apt-get install -f
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE
```bash
sudo dnf install ./gt-stacer-26.11.3-2.x86_64.rpm
```

### Anywhere (AppImage)
```bash
chmod +x GT-STACER-26.11.3-x86_64.AppImage
./GT-STACER-26.11.3-x86_64.AppImage
```

### Flatpak
```bash
flatpak install --user GT-STACER-26.11.3-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

### In-app (from 26.11.1 or later)
Settings → check for updates → **Download & install**.

For the full feature list of this series, see the [26.11 release notes](RELEASE_NOTES_26.11.md)
and [CHANGELOG.md](CHANGELOG.md).

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
