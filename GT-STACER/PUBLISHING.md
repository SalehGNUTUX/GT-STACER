# Packaging & publishing GT-STACER

The exact procedure for cutting a release, plus the pitfalls that have bitten us
(each one shipped a broken release until caught). Run through
[`PUBLISH_CHECKLIST.md`](PUBLISH_CHECKLIST.md) alongside this.

> **Repo layout reminder.** One GitHub repo, `SalehGNUTUX/GT-STACER`, two mount
> points: the **app** lives in the repo's **`GT-STACER/` subfolder**, the
> **website** at the **repo root**, and CI under **`.github/`**. Neither the app
> nor the web working copy has a local `.git` — publishing is clone + rsync.

## 1. Bump the version (never blanket-`sed`)

Older CHANGELOG / metainfo / roadmap entries are history — keep them verbatim.
Touch, for `YY.MM`:

- `CMakeLists.txt` — `VERSION YY.M.0`, `APP_VERSION="YY.MM"`.
- **All four** packaging scripts — `packaging/build-{deb,rpm,appimage}.sh` and
  `scripts/build-all.sh` — `VERSION="YY.MM"`.
- `CHANGELOG.md` (new top entry), `packaging/flatpak/*.metainfo.xml`
  (new `<release>`), a new `RELEASE_NOTES_YY.MM.md`.
- `README.md` (badge, "What's new" AR+EN, comparison table, install commands,
  deps), `SECURITY.md`, `CONTRIBUTING.md` (string count),
  `gt-stacer/Pages/Settings/settings_page.ui` (versionLabel text).
- Website: `sw.js` `CACHE_VERSION`, the download cards, the schema.org block,
  `table_header_gtstacer`, a `changelogData`/`roadmapData` entry, screenshots.

## 2. Build — the pitfalls

```bash
# sudo hangs in a non-interactive shell; front a no-op shim (deps are installed)
mkdir -p /tmp/shim && printf '#!/bin/sh\nexit 0\n' > /tmp/shim/sudo && chmod +x /tmp/shim/sudo
rm -rf build            # optional: clean configure for a release
# move OLD-version packages out of release/ (see pitfall #2)
PATH="/tmp/shim:$PATH" ./scripts/build-all.sh all
```

1. **`scripts/build-all.sh` has its OWN internal `build_deb()`** with a `qt_deps`
   variable (~line 463). It is *independent* of `packaging/build-deb.sh`, and
   `build-all.sh all` uses the internal one. **A new Qt runtime dependency must be
   added to BOTH.** 26.08 linked `Qt6::DBus` for the first time; the DEB shipped
   without `libqt6dbus6` and would not launch on a clean install until we added
   `libqt6dbus6` + `libqt6network6` to both `qt_deps` and `packaging/build-deb.sh`.
2. **Clean old versions out of `release/` before building.** `alien` globs
   `release/*.deb` and builds the RPM from the first match — it once built the RPM
   from a stale **26.06** DEB. Move older packages to `release-archive/`.
3. **The RPM is `-2`, not `-1`.** `alien` bumps the deb revision by one, so the
   file is `gt-stacer-YY.MM-2.x86_64.rpm`. (Native `rpmbuild` usually fails on
   missing BuildRequires and falls back to alien — expected.)
4. **Debian 13 (t64):** `libqt6core6t64`/`gui`/`widgets` carry the `t64` suffix
   (use `X t64 | X`), but `libqt6dbus6`/`libqt6network6`/`libqt6svg6` do **not**.
   Confirm a lib's package with `dpkg -S libQt6Xxx.so.6`.
5. `build-all.sh` has no `set -e`, so the `sudo` shim skipping `apt-get` is safe.
   AppImage/Flatpak bundle their own libs, so only DEB/RPM carry the dep metadata
   (the RPM's `Requires` is auto-detected from the ELF — DBus/Network appear
   there automatically once the binary links them).

## 3. Checksums + fill the docs

```bash
cd release
sha256sum GT-STACER-YY.MM-x86_64.AppImage GT-STACER_YY.MM_amd64.deb \
          gt-stacer-YY.MM-2.x86_64.rpm GT-STACER-YY.MM-x86_64.flatpak > SHA256SUMS.txt
```

Fill `RELEASE_NOTES_YY.MM.md`'s download table and the website's four download
cards with the real `size` + `sha256`. `node --check assets/app.js` after editing.

## 4. Push (clone + rsync — no local `.git`)

```bash
git clone --depth 1 https://github.com/SalehGNUTUX/GT-STACER.git repo
```

- **App → `repo/GT-STACER/`**, `rsync -a --delete` excluding
  `build*/ release/ release-archive/ .build-tools/ .flatpak-builder/ .git/ .claude/ CLAUDE.md 'claude*.txt' '*.log' 'Text File.txt'`.
  **Dry-run with `-ain` first** — verify no cache leaks and no unintended deletes.
- **Website → repo root**: **never `--delete` at the root** (it would wipe
  `GT-STACER/` and `.github/`). Copy the top-level files (`index.html`, `sw.js`,
  `manifest.webmanifest`) without delete, then `rsync -a --delete` each of
  `assets/`, `fonts/`, `images/` separately.
- Verify: `git status` shows only `GT-STACER/` + root web files, and
  `git status --short | grep -c .github` is **0**.
- Commit: **Arabic message**, author **SalehGNUTUX only**, **no `Co-Authored-By`**.
  `git push origin HEAD:main`.

## 5. GitHub release

Use the tag **`GT-STACER_YY.MM_STABLE`** (not `vYY.MM-stable`) so it matches the
website's download-card URLs. `scripts/publish-release.sh` uses a different tag
scheme and needs a local `.git`, so **don't use it** — call `gh` directly:

```bash
gh release create GT-STACER_YY.MM_STABLE --repo SalehGNUTUX/GT-STACER \
  --title "GT-STACER YY.MM STABLE" --notes-file RELEASE_NOTES_YY.MM.md \
  --latest --target main \
  release/GT-STACER-YY.MM-x86_64.AppImage release/GT-STACER_YY.MM_amd64.deb \
  release/gt-stacer-YY.MM-2.x86_64.rpm release/GT-STACER-YY.MM-x86_64.flatpak \
  release/SHA256SUMS.txt
```

`--target main` creates the tag on the remote HEAD (no local `.git` needed).

## 6. Verify the published DEB (the test-before-push rule)

```bash
gh release download GT-STACER_YY.MM_STABLE --repo SalehGNUTUX/GT-STACER --pattern '*.deb'
sha256sum *.deb                       # matches SHA256SUMS.txt
dpkg-deb -f *.deb Depends             # includes libqt6dbus6, libqt6network6
pkexec dpkg -i *.deb                  # approve the polkit dialog
gt-stacer                             # launches and stays running, clean log
```

The maintainer normally tests the DEB on a real install **before** publishing.
When explicitly told to publish directly, do it — but state clearly that the DEB
was not install-tested, and substitute the automated checks above.
