# GT-STACER 26.07 stable

**Release date:** 2026-07-11
**Channel:** stable
**License:** GNU GPL v3 or later

GT-STACER 26.07 is a field-test follow-up to the 26.06 stable release — a
Qt 6 / C++ 17 Linux system optimiser and monitor by GNUTUX. It adds two
headline tools (System Relief and a Power timer), makes autostart actually
work, detects externally-installed apps in the Uninstaller, rebuilds the
light theme, and brings the Arabic UI back to 100 %.

## Highlights

- 🌬️ **System Relief (new page).** Temporarily freezes idle, non-critical,
  user-owned processes (SIGSTOP) to relieve RAM/CPU pressure, then thaws them
  (SIGCONT). Freezing is fully reversible — no process state is lost. Manual
  ("Relieve now" from a memory-sorted candidate list, optionally dropping clean
  file caches) or automatic (off by default; a tray watchdog freezes idle apps
  under sustained pressure and thaws them once it clears). It **never** freezes
  GT-STACER's own launching session — the full ancestor chain (`/proc/<pid>/stat`),
  every process in the controlling-terminal session (`getsid`), and a denylist
  of terminals/shells/multiplexers are all excluded.
- ⏻ **Power timer.** Schedule Shut down / Restart / Suspend (to RAM) /
  Hibernate (to disk) after a set number of minutes, with a live countdown and
  a cancel button. Runs through `systemctl` (logind/polkit), so it works under
  Flatpak too. No root password is requested — logind grants an active local
  session these actions, the same reason the desktop's own shutdown button
  doesn't ask. Hibernate availability now asks logind (`CanHibernate`) instead
  of trusting `/sys/power/state`, so unsupported modes are correctly greyed out.
- 🚀 **Autostart fixed.** "Start on login" entries were written through
  `QSettings`, which mangles a `.desktop` file so desktops silently ignored
  them. `StartupTool` now writes freedesktop-compliant `.desktop` text. Flatpak
  and Snap apps are listed in the "Add…" browser, and each entry can wait N
  seconds after login before launching. The Startup and Settings pages re-scan
  `~/.config/autostart` on every `showEvent`, so the list is never stale.
- 🗑️ **External / manually-installed apps detected in the Uninstaller.** Apps
  dropped in by install scripts (e.g. Megacubo via `wget | bash`), `/opt`
  tarballs, or AppImages are found by scanning `.desktop` launchers whose `Exec`
  lives under `/opt`, `/usr/local`, `~`, or ends in `.AppImage`. A single batched
  ownership query (`dpkg -S` / `rpm -qf` / `pacman -Qo`) drops anything a package
  manager owns, so e.g. a Brave PWA launcher never offers to delete the whole
  browser. Removal covers the launcher, the `/opt` payload, and per-user
  leftovers, guarded by a path allowlist that can never delete a shared root.
  A full-page spinner overlay shows "Removing X (i of n)…" during removal.
- 🌗 **Light theme rebuilt.** The light (Catppuccin Latte) theme was badly broken
  — a dark sidebar, near-white titles that vanished, dark cards. A central
  `Theme` palette (`Managers/theme.*`) now resolves every painted widget's colors
  from the active theme, and the light stylesheet was brought to full parity with
  a modern, comfortable Latte look. The dark theme is unchanged.
- 🌍 **Arabic UI at 100 %** again (486/486), covering every new string. GPU is
  now "بطاقة الرسوميات" everywhere (was the transliterated "كرت").

For the full list, see [CHANGELOG.md](CHANGELOG.md).

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.07-x86_64.AppImage` | 52 MB | `4384b0d3172246838bc0afbe04312c13d4380c606ebbba96e2ce4e62e4beb788` |
| `GT-STACER_26.07_amd64.deb` | 1.9 MB | `f9c3d99b4df745a619e36ddf43e6046acdfcb711ef8a044d9c77f399860f0b8d` |
| `gt-stacer-26.07-2.x86_64.rpm` | 2.2 MB | `10ee27588fb51f6b505aaea8fbbcbf2b9d56792a3325b0e5c726f9a7cb3f73d2` |
| `GT-STACER-26.07-x86_64.flatpak` | 2.0 MB | `608970ecff0dcc3713df195560998e86d13e032b6bedd2427467e5fd665df1e4` |
| `SHA256SUMS.txt` | 382 B | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.07_amd64.deb
sudo apt-get install -f          # fix any missing deps
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE (via alien-built RPM)
```bash
sudo dnf install ./gt-stacer-26.07-2.x86_64.rpm
# or
sudo rpm -i ./gt-stacer-26.07-2.x86_64.rpm
```

### Anywhere (AppImage — no install needed)
```bash
chmod +x GT-STACER-26.07-x86_64.AppImage
./GT-STACER-26.07-x86_64.AppImage
```

The AppImage bundles both XCB and Wayland Qt platform plugins, so it works
on every modern desktop environment (GNOME, KDE Plasma, XFCE, Cinnamon,
MATE, LXQt, Hyprland, Sway, …).

### Flatpak (sandboxed)
```bash
flatpak install --user GT-STACER-26.07-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

The Flatpak builds against `org.kde.Platform//6.9` and uses
`flatpak-spawn --host` for every privileged operation (cleaner, service
manager, uninstaller, `/etc/hosts` editor, sysctl, and the Power timer's
`systemctl` calls). `pkexec` runs on the host, not inside the sandbox, so the
polkit prompt is unchanged. The sandbox is detected at runtime via
`$FLATPAK_ID` and `/.flatpak-info`, and `~/.cache/gt-stacer-tmp/` is used in
place of `/tmp` for the write-via-temp-file path so the host can see the
staged content.

## Dependencies

GT-STACER does not depend on QtCharts (replaced with a custom `LineChart`
widget), which cuts the install size and unblocks distros that don't ship
qt6-charts.

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
- System Relief automatic mode is off by default; enable it in the page only
  if you understand that frozen apps stop responding until thawed.
- Translations other than Arabic / English / French are partial — the UI
  falls back to English for missing strings rather than showing blanks. The
  69 strings added in 26.07 are native in Arabic/English only for now.

## Reporting issues

- Bug reports: <https://github.com/SalehGNUTUX/GT-STACER/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md) — please do not
  open public issues.

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
