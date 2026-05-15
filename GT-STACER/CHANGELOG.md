# Changelog

All notable changes to GT-STACER are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning follows
the `YY.MM` rolling-release scheme (matching the website roadmap).

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

[26.05-beta]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.05-beta
[26.04-alpha]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.04-alpha
