# GT-STACER 26.05 beta

**Release date:** 2026-05-14
**Channel:** beta (superseded by [26.06 stable](RELEASE_NOTES_26.06.md))
**License:** GNU GPL v3 or later

GT-STACER 26.05 was the first public beta after the alpha — a Qt 6 / C++ 17
Linux system optimiser and monitor by GNUTUX. This release reworked the
beta's foundations: every privileged code path was hardened, the Qt Charts
dependency was replaced with a hand-rolled widget (~70 % RAM saved), and
the System Cleaner / Startup Apps pages were redesigned around the Stacer
1.x reference look.

> **Upgrade notice.** This is a beta. The current stable is
> [v26.06](https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.06-stable),
> which fixes every regression surfaced during field testing of 26.05 and
> adds Flatpak/Snap drill-down, multi-package uninstall, and configurable
> notifications. New installs should pick 26.06; 26.05 is kept here for
> reproducibility of the audit history.

## Highlights

### 🔒 Security
- **Command-injection fix in `/etc/hosts` editor** — `saveHosts()` previously
  passed user content through `pkexec sh -c 'echo "%1" > /etc/hosts'`,
  allowing arbitrary code execution as root. The replacement writes via a
  temp file + `pkexec install -o root -m 644`.
- All package, service, and APT-source identifiers are validated by
  `CommandUtil::isSafeIdentifier` before reaching `pkexec`.
- `PackageTool::remove`, every `ServiceTool` action, and `AptSourceTool`
  were refactored to use `execProgram(prog, args)` (no shell, no injection).
- Subresource Integrity (SRI) hash added to the Font Awesome CDN link on
  the website.

### ⚡ Performance
- **Background CPU sampler** — `CpuInfo::usage()` no longer blocks the UI
  for ~200 ms per call; a dedicated `gt-stacer-cpu` worker thread samples
  `/proc/stat` once per second under a mutex.
- **Replace QtCharts with custom `LineChart` widget** — a QPainter-based
  chart removes the dependency on `libQt6Charts`, which transitively pulled
  in 8+ QML libraries. RAM use drops by ~70 %; 9 Qt 6 runtime libs gone.
- **`/proc`-based `ProcessInfo`** — no more `ps` shell-out; processes
  enumerate from `/proc/{pid}/{stat,status}` directly in ~30 ms for 277
  running processes.
- **In-place model updates** — `ProcessesPage` and `ServicesPage` reuse
  rows by PID/name instead of `setRowCount(0)`; selection and sort survive
  every refresh tick.
- **Lazy page construction** — pages other than the Dashboard are built on
  first navigation, not at startup. Lower initial RAM and faster cold start.
- `appendPoint` is `O(1)` (ring buffer + batched `replace(QList)`) instead
  of `O(N)` shift.
- Cmdline truncated to 120 chars at parse time — significant savings with
  hundreds of processes.

### 🎨 UX
- **System Cleaner — icon-card redesign**: 7 categories with per-category
  checkboxes, "Select All", and an info badge for cards that support a
  drill-down dialog.
- **App Cache details dialog** (drill-down on the App Cache card): lists
  every subdirectory of `~/.cache` with size + last-modified, with search,
  sort (largest/oldest/newest/A→Z), and per-row selection.
- **Confirmation dialog before any clean** — lists every selected category
  with its size; sensitive categories are tagged `· requires root`.
- **Cleaning fix for `/var/log` / `/var/cache`** — root-owned categories
  now use `pkexec find -delete` (rotated logs only for `/var/log` to avoid
  losing live journals) and `pkexec apt-get clean` for the APT cache.
- **Startup Apps redesign** — icons resolved from each entry's `Icon=`
  field, inline ON/OFF toggle, Remove button per row, "Add…" dialog with a
  system-app browser tab and a manual-entry tab.
- **Quit confirmation dialog** when invoking Quit from the tray, with a
  "Don't ask again" checkbox.
- **About dialog** accessible via F1 or the tray menu.
- **Keyboard shortcuts** — Ctrl+1..9/0 (pages), Ctrl+R (refresh),
  Ctrl+Q (quit), Ctrl+, (settings), F1 (about).
- **Wayland taskbar icon** — `setDesktopFileName("gt-stacer")` so the
  compositor resolves the icon from the `.desktop` file instead of falling
  back to a generic Wayland glyph.

### ✨ Features
- **libnotify alerts** for CPU/GPU temperature, memory, disk usage, and
  battery thresholds (all configurable through QSettings; 5-minute per-key
  cooldown so a hot CPU doesn't spam the notification daemon).
- **Per-core CPU bars** on the Resources page (uses `cpu.perCore` data that
  the alpha collected but never displayed).
- **CPU temperature** displayed under the Dashboard CPU gauge.
- **Disk I/O monitor** — read/write speeds from `/proc/diskstats` shown on
  the Dashboard.
- **Auto-follow system theme** option (`Qt 6.5+` `QStyleHints::colorScheme`,
  with fallback to GNOME `gsettings color-scheme` for older Qt).
- **Helpers expansion** — DNS resolver cache flush (auto-detects
  systemd-resolved / nscd / unbound), `vm.swappiness` editor,
  `/proc/cmdline` viewer, locale display.

### 🌍 Translations
- **Arabic complete** — 342/342 strings translated (100 %).
- **English** — identity translation file added so the QM ships.
- **17 other languages** at 18-38 % coverage of the most common UI strings;
  the rest stayed `unfinished` for community completion.

### 📦 Build
- `-Os -ffunction-sections -fdata-sections` + `-Wl,--gc-sections --as-needed`
  for release builds.
- Binary stripped at install time.
- Dropped Qt6Charts dependency from the `find_package` block and packaging
  scripts (DEB / RPM). Saves both build-time deps and runtime install size.

## Downloads (archived)

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.05-x86_64.AppImage` | 53 MB | `4cf44f733d9926461af9797ec9dcdb3e6345737c03e425af29c79669c4240396` |
| `GT-STACER_26.05_amd64.deb` | 1.8 MB | `0b0ea8fa2bc998d8e5573085d2a815f2a9a5633ab4e396510acfa25eaa7bbed9` |
| `gt-stacer-26.05-2.x86_64.rpm` | 8.0 MB | `9c07a9775d98d8edc36ce2efbbc6e3f0718eb91375e2cd08b61452266bba6ba7` |

(Checksums recorded from `release/SHA256SUMS.txt` of the 26.05 beta build.)

## Compatibility

| Distro family | Status |
|---|---|
| Debian 11+, Ubuntu 22.04+, Mint 21+, Pop!_OS, Kali, Trixie | ✅ Tested |
| Fedora 39+, RHEL 9+, AlmaLinux 9, Rocky 9, openSUSE Tumbleweed | ✅ Built via alien |
| Arch / Manjaro / EndeavourOS | ✅ AppImage |
| Wayland (GNOME 45+, KDE Plasma 6) | ✅ AppImage bundles both XCB and Wayland Qt plugins |

## Known issues fixed in 26.06 stable

These were discovered during 26.05 beta field testing and are resolved in
[26.06 stable](RELEASE_NOTES_26.06.md):

- **"Hidden" Stacer process** — the Processes page only searched the
  truncated 15-char `comm` field, so AppImage wrapper processes appeared
  to be missing. 26.06 searches PID + name + cmdline + user.
- **Disk-full notification on `/boot`** — the default 90 % disk threshold
  fired on tiny system partitions. 26.06 skips partitions below 2 GB and
  exposes the threshold in Settings.
- **Single-package uninstall only** — 26.05 limited the Uninstaller to one
  row at a time. 26.06 enables multi-select with sequential progress.
- **APT-only package-cache label** — the cleaner card was labelled
  "APT Cache" everywhere. 26.06 picks the label from the host's package
  manager (`DNF Cache`, `Pacman Cache`, …).
- **No Flatpak/Snap drill-down** — 26.05 cleaned them at the manager level
  only. 26.06 adds dedicated cards with per-app removal dialogs.

## Reporting issues

- Bug reports: <https://github.com/SalehGNUTUX/GT-STACER/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md) — please do not
  open public issues.

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
