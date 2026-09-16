# GT-STACER 26.11.1 stable

**Release date:** 2026-09-16
**Channel:** stable
**License:** GNU GPL v3 or later

A small **bug-fix point release** on top of [26.11](https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.11_STABLE).
No new features — two fixes found during testing.

## Fixes

- 🛡️ **Firewall page no longer reports "no supported firewall" when ufw/firewalld
  is installed.** A GUI launched from a desktop entry often inherits a `PATH`
  without the sbin directories, so `ufw` (which lives in `/usr/sbin` and `/sbin`)
  was invisible to the executable lookup and the page said no firewall was
  present. GT-STACER now ensures `/usr/local/sbin`, `/usr/sbin` and `/sbin` are on
  `PATH` at startup. (The Firewall page shipped in 26.08, so this affected every
  install since.)
- 🔄 **In-app update check now understands patch versions.** The version
  comparison only looked at `YY.MM` and the tag parser dropped the patch, so a
  point release like this one would not have been offered as an update over
  26.11. Versions are now compared as `YY.MM.PATCH`. (This is also why 26.11.1 is
  a good release to try the in-app "Download & install" update from 26.11.)

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.11.1-x86_64.AppImage` | 52 MB | `3af72f9dc92067c4b9e76bb457df536a62ac70484361ce2d9c7a780a4b3aa925` |
| `GT-STACER_26.11.1_amd64.deb` | 2.1 MB | `c359fdd7cd5d7f4e4aa84c51953e5b5d348689b7bdbc13e584b85b94aad4dcc9` |
| `gt-stacer-26.11.1-2.x86_64.rpm` | 2.4 MB | `55395411207581bc2b5c8c043e5443472e84702aeea60488a4cf54ce4b57263a` |
| `GT-STACER-26.11.1-x86_64.flatpak` | 2.2 MB | `77ea0e1f7a487951ba13ef2d0cecd3168fff563df8198aecb10b64b1cda0fe75` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.11.1_amd64.deb
sudo apt-get install -f
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE
```bash
sudo dnf install ./gt-stacer-26.11.1-2.x86_64.rpm
```

### Anywhere (AppImage)
```bash
chmod +x GT-STACER-26.11.1-x86_64.AppImage
./GT-STACER-26.11.1-x86_64.AppImage
```

### Flatpak
```bash
flatpak install --user GT-STACER-26.11.1-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

### In-app (from 26.11)
Settings → check for updates → **Download & install** (AppImage / DEB / RPM
installs auto-update; the checksum is verified before installing).

For the full feature list of this series, see the [26.11 release notes](RELEASE_NOTES_26.11.md)
and [CHANGELOG.md](CHANGELOG.md).

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
