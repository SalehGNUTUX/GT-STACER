# GT-STACER 26.08 stable

**Release date:** 2026-07-31
**Channel:** stable
**License:** GNU GPL v3 or later

GT-STACER 26.08 is the **network & power tools** release — a Qt 6 / C++ 17 Linux
system optimiser and monitor by GNUTUX. It adds three new pages (Connections,
Power and Firewall), a cross-desktop power-integration layer, and a
program-wide timer-efficiency pass.

## Highlights

- 🔌 **Connections (new).** Every active TCP/UDP socket and the process that owns
  it, parsed from `ss -tunaHp`. Live text filter (address / port / process /
  state), sortable columns, an optional auto-refresh that only runs while the
  page is shown, and a "show processes of all users" toggle (via `pkexec`).
- ⏻ **Power (new).** Switch the system **power profile** — `power-profiles-daemon`
  (Power Saver / Balanced / Performance), or raw **cpufreq governors** where the
  daemon is absent (works on desktops too — it drives the CPU governor). On
  laptops, a **battery charge limit** (`charge_control_*_threshold`) caps
  charging to extend battery lifespan; the section hides on desktops.
- 🌙 **Keep awake — the cross-desktop way.** Block automatic sleep and screen
  locking using the freedesktop D-Bus interfaces every major desktop implements
  (`org.freedesktop.PowerManagement.Inhibit`, falling back to `ScreenSaver`, then
  `systemd-inhibit`). Our block appears in the desktop's own power applet, and
  `HasInhibit()` lets us **detect a block set from elsewhere** (e.g. KDE's own
  "Manually block") — which `systemd-inhibit` never sees. A tray badge and a
  desktop notification signal the state even when the window is closed.
- 🛡️ **Firewall (new).** Enable/disable **ufw** or **firewalld**, list rules, and
  add/remove port rules (port + tcp/udp + allow/deny). The enabled state is read
  without root; each change mutates **and** re-lists in one `pkexec`, so you
  authorize **once** per action — not twice.
- ⚡ **Quieter in the background.** Dashboard, Resources, Processes (and the new
  pages) pause their refresh timers when the page isn't shown; the Settings power
  timer keeps counting down when minimised so a scheduled shutdown still fires.
- 🌍 **Auto language + KDE flags.** A default "Auto (system language)" that
  follows the system locale (English when unsupported) and is remembered once
  changed; flag emoji render as icons so they show on KDE, not only GNOME.

For the full list, see [CHANGELOG.md](CHANGELOG.md).

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.08-x86_64.AppImage` | 52 MB | `d0ca2311dbfaf86fe5748b607f53cb94d01c9c621039c7df4e062d289a75f87b` |
| `GT-STACER_26.08_amd64.deb` | 2.0 MB | `c0f014e744bccacbd7281c27b50aa1514b39a6dbd384cd49e685a07ef8a74de7` |
| `gt-stacer-26.08-2.x86_64.rpm` | 2.3 MB | `1839bba453b2b8efeb478d08b28a81f1413603f4e55c17498f76b5068c9871b7` |
| `GT-STACER-26.08-x86_64.flatpak` | 2.1 MB | `de31c9f9989c76ef62c8e9c181c81d30425180cc853ac1342a66f2a69d6e2446` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.08_amd64.deb
sudo apt-get install -f          # fix any missing deps
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE (via alien-built RPM)
```bash
sudo dnf install ./gt-stacer-26.08-2.x86_64.rpm
```

### Anywhere (AppImage — no install needed)
```bash
chmod +x GT-STACER-26.08-x86_64.AppImage
./GT-STACER-26.08-x86_64.AppImage
```

### Flatpak (sandboxed)
```bash
flatpak install --user GT-STACER-26.08-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

The Flatpak builds against `org.kde.Platform//6.9` and routes every privileged
op (cleaner, services, uninstaller, firewall, power) through
`flatpak-spawn --host`, so the polkit prompt is identical to the DEB/RPM case.

## Dependencies

New this release: **`Qt6::DBus`** — it ships inside `qt6-base`, so there is no
new distro package to install. GT-STACER still does not depend on QtCharts.

| Distro family | Runtime deps |
|---|---|
| Debian / Ubuntu | `libqt6core6t64` (or `libqt6core6`), `libqt6gui6t64`, `libqt6widgets6t64`, `libqt6svg6`, `libqt6dbus6` · recommended: `polkitd`, `flatpak`, `libnotify-bin`, `ufw` |
| Fedora / RHEL | `qt6-qtbase`, `qt6-qtsvg` · recommended: `polkit`, `flatpak`, `libnotify`, `ufw` |
| Arch | `qt6-base`, `qt6-svg` · recommended: `polkit`, `flatpak`, `libnotify`, `ufw` |

Optional integrations, detected at runtime and hidden when absent:
`power-profiles-daemon` (power profiles), `ss`/iproute2 (connections),
`ufw`/`firewalld` (firewall), `systemd-inhibit` and the desktop's D-Bus power
service (keep awake).

## Known limitations

- RPM is built via `alien` from the DEB on Debian. A native `rpmbuild` path is in
  the spec file but needs Fedora/openSUSE.
- The battery charge-limit only appears on laptops whose kernel exposes
  `charge_control_end_threshold`.
- "Keep awake" blocks sleep and screen locking, but cannot release a block that
  another app or the desktop set (it detects and reports it instead).
- Firewall listing and changes require authorization each time (they need root);
  the enabled state is shown without a prompt.

## Reporting issues

- Bug reports: <https://github.com/SalehGNUTUX/GT-STACER/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md) — please do not open
  public issues.

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
