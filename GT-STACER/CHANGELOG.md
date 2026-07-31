# Changelog

All notable changes to GT-STACER are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning follows
the `YY.MM` rolling-release scheme (matching the website roadmap).

---

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

[26.08-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.08-stable
[26.07-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.07-stable
[26.06-stable]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.06-stable
[26.05-beta]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.05-beta
[26.04-alpha]: https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.04-alpha
