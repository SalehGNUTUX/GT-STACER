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
| `GT-STACER-26.11.2-x86_64.AppImage` | 52 MB | `bb85d72985a673146603c6bbfd21ca9b7b4a744aa2a8ee040ed27c0dfa7be973` |
| `GT-STACER_26.11.2_amd64.deb` | 2.1 MB | `2f2794dc156a625623476fa39851757ad732e00ae1b8bb515385d1b9aa40d44e` |
| `gt-stacer-26.11.2-2.x86_64.rpm` | 2.4 MB | `5b8c00add74a56fda87b63a9ac6d654c7b0aff6c91ff39ce392d2aff48085737` |
| `GT-STACER-26.11.2-x86_64.flatpak` | 2.2 MB | `02455579e3aa7b3d2cf5b76a59c49d765fd5deb10985661ee8299b430ec05132` |
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
