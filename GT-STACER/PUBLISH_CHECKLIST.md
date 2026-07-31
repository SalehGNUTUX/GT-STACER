# Pre-publish checklist — GT-STACER 26.08 stable

Before pushing the v26.08-stable tag and creating the release, run through
this list. Every item should be a ✅ before clicking publish.

## Code
- [x] `CMakeLists.txt` → `VERSION 26.8.0`, `APP_VERSION="26.08"`,
      `APP_CHANNEL="stable"`
- [x] Packaging scripts (`build-deb.sh`, `build-rpm.sh`, `build-appimage.sh`,
      `build-all.sh`) use `VERSION="26.08"`
- [x] `Qt6::DBus` added to `find_package` + link line
- [ ] Strict build (`-Wall -Wextra -Wpedantic`) passes with **0 warnings**
- [ ] `security_test` reports **27/27 PASS**
- [ ] `core_test` reports **21/21 PASS**

## Translations
- [x] All 19 `.qm` files compile cleanly (`578 finished, 0 unfinished`)
- [x] AR + EN at 100 % native (578/578)
- [x] Other 17 languages compiled with English fallback (no `unfinished`)

## Artifacts (in `release/`)
- [ ] `GT-STACER-26.08-x86_64.AppImage` — includes Wayland plugin
- [ ] `GT-STACER_26.08_amd64.deb` — Depends includes `libqt6dbus6`, no `libqt6charts6`
- [ ] `gt-stacer-26.08-2.x86_64.rpm` — built via alien from DEB
- [ ] `GT-STACER-26.08-x86_64.flatpak` — `org.gnutux.gt-stacer` on `org.kde.Platform//6.9`
- [ ] `SHA256SUMS.txt` — covers all four
- [ ] `RELEASE_NOTES_26.08.md` Downloads table filled with real sizes + SHA-256

Verify the checksums one more time:
```bash
cd release/
sha256sum -c SHA256SUMS.txt
```

## Smoke test on a clean install
- [ ] DEB installs without `apt-get install -f` fixups on Debian 13
- [ ] App launches from the application menu (icon resolves correctly)
- [ ] All **14** pages open without crashing (… System Relief, **Connections,
      Power, Firewall**)
- [ ] **Connections** — sockets list with owning process; filter + auto-refresh
      work; "all users" toggle prompts once and reveals more
- [ ] **Power** — the three profile buttons switch the active profile (KDE/GNOME
      applet reflects it); battery section hidden on desktop
- [ ] **Power → keep awake** — "Manually block" holds; shows as **one** entry in
      the desktop's power UI; tray badge + desktop notification appear; the page
      also reflects a block set from the desktop itself (HasInhibit)
- [ ] **Firewall** — enable/disable works; add rule (8080/tcp) prompts **once**;
      delete prompts once; state shown without a prompt
- [ ] Navigating between pages leaves only the current page polling (CPU ≈ idle)
- [ ] Quitting from tray triggers the confirmation dialog

## Flatpak sandbox smoke test
- [ ] `flatpak install --user release/GT-STACER-26.08-x86_64.flatpak` succeeds
- [ ] `flatpak run org.gnutux.gt-stacer` launches with `FLATPAK_ID` set
- [ ] Firewall / power privileged ops reach the host polkit via `flatpak-spawn --host`
- [ ] Keep-awake D-Bus inhibit works from inside the sandbox (session bus is shared)

## Documentation
- [x] `CHANGELOG.md` has a `[26.08-stable]` entry dated 2026-07-31
- [x] `RELEASE_NOTES_26.08.md` exists and matches the changelog
- [x] `README.md` shows the 26.08 badge + "What's new" + new comparison rows (AR + EN)
- [x] `SECURITY.md` lists 26.08 stable as current
- [x] `CONTRIBUTING.md` updated to the 578-string baseline
- [x] `CLAUDE.md` documents the new pages/tools, `Qt6::DBus`, `SleepInhibitor`, and the D-Bus/keep-awake lessons
- [x] `packaging/flatpak/org.gnutux.gt-stacer.metainfo.xml` has a `<release version="26.08">` entry
- [ ] Website (`GT-STACER-WEB/`) version refs all show 26.08, with screenshots for the 3 new pages

## Publish

When everything above is ✅:

```bash
gh auth login                       # one-off
./scripts/publish-release.sh --dry-run
./scripts/publish-release.sh
```

The script creates the tag `v26.08-stable`, pushes it, and creates the GitHub
release with `RELEASE_NOTES_26.08.md` as the body and every file in `release/`
attached. Release page:
<https://github.com/SalehGNUTUX/GT-STACER/releases/tag/v26.08-stable>

## After publishing
- [ ] Push the website (`GT-STACER-WEB/` → repo root on `main`).
- [ ] Announce (r/linux, LWN, Phoronix, Mastodon).
- [ ] Open the next milestone (`v26.09 — backup & snapshots`).
