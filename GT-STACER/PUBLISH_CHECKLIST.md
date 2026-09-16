# Pre-publish checklist — GT-STACER 26.11 stable

> **26.11 (2026-09-16)** — released via `scripts/release.sh`
> (tag `GT-STACER_26.11_STABLE`). See **[`PUBLISHING.md`](PUBLISHING.md)** for the
> full procedure and the packaging pitfalls to watch (internal `build_deb`,
> stale `release/`, RPM `-2`, tag scheme, Flatpak OSTree min-free-space).

Before pushing the tag and creating the release, run through this list. Every
item should be a ✅ before clicking publish.

## Code
- [x] `CMakeLists.txt` → `VERSION 26.11.0`, `APP_VERSION="26.11"`,
      `APP_CHANNEL="stable"`
- [x] Packaging scripts (`build-deb.sh`, `build-rpm.sh`, `build-appimage.sh`,
      `build-all.sh`) use `VERSION="26.11"`
- [x] New sources wired in CMake: `package_manager_page.cpp/.h`,
      `update_checker.cpp`, `self_updater.cpp`, `language_util.cpp` (UI);
      `store_addon_tool.cpp`, `appimage_tool.cpp` (core); old `uninstaller_page`
      removed
- [ ] Strict build (`-Wall -Wextra -Wpedantic`) passes with **0 warnings**
- [ ] `security_test` reports all PASS (incl. the new package-search
      injection guard)
- [ ] `core_test` reports all PASS

## Translations
- [ ] All 19 `.qm` files compile cleanly (no `unfinished`)
- [ ] AR + EN at 100 % native (new Package Manager / self-update strings included)
- [ ] Other 17 languages compiled with English fallback

## Artifacts (in `release/`)
- [ ] `GT-STACER-26.11-x86_64.AppImage` — includes Wayland plugin
- [ ] `GT-STACER_26.11_amd64.deb` — Depends includes `libqt6dbus6` + `libqt6network6`, no `libqt6charts6`
- [ ] `gt-stacer-26.11-2.x86_64.rpm` — built via alien from DEB
- [ ] `GT-STACER-26.11-x86_64.flatpak` — `org.gnutux.gt-stacer` on `org.kde.Platform//6.9`
- [ ] `SHA256SUMS.txt` — covers all four
- [ ] `RELEASE_NOTES_26.11.md` Downloads table filled with real sizes + SHA-256

Verify the checksums one more time:
```bash
cd release/
sha256sum -c SHA256SUMS.txt
```

## Smoke test on a clean install
- [ ] DEB installs without `apt-get install -f` fixups on Debian 13
- [ ] App launches (`/usr/bin/gt-stacer` from the installed DEB, clean log)
- [ ] All **16** pages open without crashing (Packages replaces the old
      Uninstaller at index 6)
- [ ] **Processes** — opens already sorted by CPU % descending, no header click
- [ ] **Packages → Installed** — lists installed packages; remove prompts once
- [ ] **Packages → Search & Install** — a search term with shell metacharacters
      (`; touch /tmp/x`) does **not** run a shell (no file created); install works
- [ ] **Packages → Upgrades** — lists upgradable packages; apply works
- [ ] **Packages → Store add-ons** — lists installed opendesktop content;
      remove is path-guarded; `ocs-url` install offered when a handler exists
- [ ] **Packages → AppImages** — lists GearLever-integrated apps (custom folder
      honoured); integrate an AppImage → valid launcher, clone vs move per setting
- [ ] **Tray tooltip** shows CPU % / RAM % / temperature
- [ ] Table columns resize freely (visible drag handles) on every page
- [ ] **Settings → update check** — offers "Download & install" when the install
      method supports it; package-update auto-check stays **off** by default,
      app-version check stays **on**
- [ ] Navigating between pages leaves only the current page polling (CPU ≈ idle)

## Flatpak sandbox smoke test
- [ ] `flatpak install --user release/GT-STACER-26.11-x86_64.flatpak` succeeds
- [ ] `flatpak run org.gnutux.gt-stacer` launches with `FLATPAK_ID` set
- [ ] Package-manager privileged ops reach the host polkit via `flatpak-spawn --host`

## Documentation
- [x] `CHANGELOG.md` has a `[26.11-stable]` entry dated 2026-09-16
- [x] `RELEASE_NOTES_26.11.md` exists and matches the changelog
- [x] `README.md` shows the 26.11 badge + "What's new" + new comparison rows (AR + EN)
- [ ] `SECURITY.md` lists 26.11 stable as current + notes the fixed injection
- [ ] `CLAUDE.md` documents Packages page, self-update, `StoreAddonTool`,
      `AppImageTool`, and the injection lesson
- [x] `packaging/flatpak/org.gnutux.gt-stacer.metainfo.xml` has a `<release version="26.11">` entry
- [ ] Website (`GT-STACER-WEB/`) version refs all show 26.11, with the renamed
      Packages screenshots (was Uninstaller)

## Publish — follow [`PUBLISHING.md`](PUBLISHING.md)

There is **no local `.git`** — publish by cloning the repo and rsync-ing the app
into `GT-STACER/` and the website into the root (no `--delete` at root), then:

```bash
gh release create GT-STACER_26.11_STABLE --repo SalehGNUTUX/GT-STACER \
  --title "GT-STACER 26.11 STABLE" --notes-file RELEASE_NOTES_26.11.md \
  --latest --target main \
  release/GT-STACER-26.11-x86_64.AppImage release/GT-STACER_26.11_amd64.deb \
  release/gt-stacer-26.11-2.x86_64.rpm release/GT-STACER-26.11-x86_64.flatpak \
  release/SHA256SUMS.txt
```

> Tag is **`GT-STACER_26.11_STABLE`** (matches the website's download URLs), NOT
> `v26.11-stable`. Do **not** use `scripts/publish-release.sh` — it uses the
> wrong tag scheme and needs a local `.git`. `scripts/release.sh` automates the
> build + SHA-fill + push + GitHub release.

Release page:
<https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.11_STABLE>

## After publishing
- [ ] Push the website (`GT-STACER-WEB/` → repo root on `main`) — same run.
- [ ] Sync repo-root `README.md` + `CHANGELOG.md` to 26.11 (release.sh doesn't
      touch them — recurring gotcha).
- [ ] Verify the published DEB installs and launches (see [`PUBLISHING.md`](PUBLISHING.md) §6).
- [ ] Announce (r/linux, LWN, Phoronix, Mastodon).
