# GT-STACER 26.11.2 stable

**Release date:** 2026-09-16
**Channel:** stable
**License:** GNU GPL v3 or later

A small **point release** following the testing of 26.11.1.

## Fixes & changes

- 📰 **"What's new" dialog now shows the right release.** Its highlights were
  hard-coded and had not been refreshed, so after updating it listed 26.10's
  changes even though the title was correct. It now lists the 26.11 highlights
  plus the 26.11.1 fixes, in English and Arabic.
- ⬇️ **In-app "Download & install" now appears reliably, in a clear window.** The
  install-type detection used `dpkg -S <path>`, which scans every package's file
  list and could exceed its timeout on a cold cache or a busy old disk — then the
  one-click update was hidden and only a release-page link remained. Detection now
  queries by package name (`dpkg-query -W` / `rpm -q`, a fast single-DB lookup),
  and the update offer is a proper dialog with a prominent **Download & install**
  button (DEB / RPM / AppImage) plus a **Release page** button.
- 🔢 **Update check works for any stable version and sequence.** The version
  comparison now walks every dotted component (not just `YY.MM.PATCH`) and the
  tag parser captures them all, so `26.11 < 26.11.1 < 26.11.2 < 26.12 < 27.01`
  (and deeper) are all ordered correctly. `/releases/latest` returns the newest
  non-prerelease and the installer picks its asset by type, so any stable release
  is fetched and installed correctly.

> Because 26.11.1 already had the patch-aware update check, **this is the release
> to try the in-app "Download & install" upgrade** (Settings → check for updates)
> from an installed 26.11.1.

## Downloads

| Asset | Size | SHA-256 |
|---|---|---|
| `GT-STACER-26.11.2-x86_64.AppImage` | 52 MB | `581e19991455e5337bd0c2539d7fce918d388fdd9baac910e0042e8b1a11ee43` |
| `GT-STACER_26.11.2_amd64.deb` | 2.1 MB | `e666c6d4ebc31a35120bff29d76079c4eaebd5449a2c30c0be607dea46b74d15` |
| `gt-stacer-26.11.2-2.x86_64.rpm` | 2.4 MB | `5b913f9e56fcc8b756e99c638fd110c46844b4d3a586c3216dd430688e315f5b` |
| `GT-STACER-26.11.2-x86_64.flatpak` | 2.2 MB | `7daa843450e1cfb5533ea6e56b764917567a39891bf905c237c38aa7320eb440` |
| `SHA256SUMS.txt` | — | (verifies the four above) |

Verify before installing:
```bash
sha256sum -c SHA256SUMS.txt
```

## Install

### Debian / Ubuntu / Mint / Pop!_OS / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.11.2_amd64.deb
sudo apt-get install -f
```

### Fedora / RHEL / AlmaLinux / Rocky / openSUSE
```bash
sudo dnf install ./gt-stacer-26.11.2-2.x86_64.rpm
```

### Anywhere (AppImage)
```bash
chmod +x GT-STACER-26.11.2-x86_64.AppImage
./GT-STACER-26.11.2-x86_64.AppImage
```

### Flatpak
```bash
flatpak install --user GT-STACER-26.11.2-x86_64.flatpak
flatpak run org.gnutux.gt-stacer
```

### In-app (from 26.11.1)
Settings → check for updates → **Download & install**.

For the full feature list of this series, see the [26.11 release notes](RELEASE_NOTES_26.11.md)
and [CHANGELOG.md](CHANGELOG.md).

## Credits

- **Developer:** GNUTUX <gnutux.arabic@gmail.com>
- **Inspired by:** [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN

Released under the GNU GPL v3 or later. No warranty.
