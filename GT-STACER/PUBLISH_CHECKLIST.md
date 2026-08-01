# Pre-publish checklist — GT-STACER 26.09 stable

> **26.09 (2026-08-01)** — released via `scripts/release.sh`
> (tag `GT-STACER_26.09_STABLE`). See **[`PUBLISHING.md`](PUBLISHING.md)** for the
> full procedure and the packaging pitfalls to watch (internal `build_deb`,
> stale `release/`, RPM `-2`, tag scheme).

Before pushing the tag and creating the release, run through this list. Every
item should be a ✅ before clicking publish.

## Code
- [x] `CMakeLists.txt` → `VERSION 26.8.0`, `APP_VERSION="26.09"`,
      `APP_CHANNEL="stable"`
- [x] Packaging scripts (`build-deb.sh`, `build-rpm.sh`, `build-appimage.sh`,
      `build-all.sh`) use `VERSION="26.09"`
- [x] `Qt6::DBus` added to `find_package` + link line
- [x] Strict build (`-Wall -Wextra -Wpedantic`) passes with **0 warnings**
- [ ] `security_test` reports **27/27 PASS**
- [ ] `core_test` reports **21/21 PASS**

## Translations
- [x] All 19 `.qm` files compile cleanly (`578 finished, 0 unfinished`)
- [x] AR + EN at 100 % native (578/578)
- [x] Other 17 languages compiled with English fallback (no `unfinished`)

## Artifacts (in `release/`)
- [x] `GT-STACER-26.09-x86_64.AppImage` — includes Wayland plugin
- [x] `GT-STACER_26.09_amd64.deb` — Depends includes `libqt6dbus6` + `libqt6network6`, no `libqt6charts6`
- [x] `gt-stacer-26.09-2.x86_64.rpm` — built via alien from DEB
- [x] `GT-STACER-26.09-x86_64.flatpak` — `org.gnutux.gt-stacer` on `org.kde.Platform//6.9`
- [x] `SHA256SUMS.txt` — covers all four
- [x] `RELEASE_NOTES_26.09.md` Downloads table filled with real sizes + SHA-256

Verify the checksums one more time:
```bash
cd release/
sha256sum -c SHA256SUMS.txt
```

## Smoke test on a clean install
- [x] DEB installs without `apt-get install -f` fixups on Debian 13 (verified: upgraded 26.07→26.08 cleanly)
- [x] App launches (verified: `/usr/bin/gt-stacer` from the installed DEB runs, clean log)
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
- [ ] `flatpak install --user release/GT-STACER-26.09-x86_64.flatpak` succeeds
- [ ] `flatpak run org.gnutux.gt-stacer` launches with `FLATPAK_ID` set
- [ ] Firewall / power privileged ops reach the host polkit via `flatpak-spawn --host`
- [ ] Keep-awake D-Bus inhibit works from inside the sandbox (session bus is shared)

## Documentation
- [x] `CHANGELOG.md` has a `[26.09-stable]` entry dated 2026-08-01
- [x] `RELEASE_NOTES_26.09.md` exists and matches the changelog
- [x] `README.md` shows the 26.09 badge + "What's new" + new comparison rows (AR + EN)
- [x] `SECURITY.md` lists 26.09 stable as current
- [x] `CONTRIBUTING.md` updated to the 578-string baseline
- [x] `CLAUDE.md` documents the new pages/tools, `Qt6::DBus`, `SleepInhibitor`, and the D-Bus/keep-awake lessons
- [x] `packaging/flatpak/org.gnutux.gt-stacer.metainfo.xml` has a `<release version="26.09">` entry
- [x] Website (`GT-STACER-WEB/`) version refs all show 26.09, with screenshots for the 3 new pages

## Publish — follow [`PUBLISHING.md`](PUBLISHING.md)

There is **no local `.git`** — publish by cloning the repo and rsync-ing the app
into `GT-STACER/` and the website into the root (no `--delete` at root), then:

```bash
gh release create GT-STACER_26.09_STABLE --repo SalehGNUTUX/GT-STACER \
  --title "GT-STACER 26.09 STABLE" --notes-file RELEASE_NOTES_26.09.md \
  --latest --target main \
  release/GT-STACER-26.09-x86_64.AppImage release/GT-STACER_26.09_amd64.deb \
  release/gt-stacer-26.09-2.x86_64.rpm release/GT-STACER-26.09-x86_64.flatpak \
  release/SHA256SUMS.txt
```

> Tag is **`GT-STACER_26.09_STABLE`** (matches the website's download URLs), NOT
> `v26.09-stable`. Do **not** use `scripts/publish-release.sh` — it uses the
> wrong tag scheme and needs a local `.git`.

Release page:
<https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.09_STABLE>

## After publishing
- [x] Push the website (`GT-STACER-WEB/` → repo root on `main`) — done in the same commit.
- [x] Verify the published DEB installs and launches (see [`PUBLISHING.md`](PUBLISHING.md) §6).
- [ ] Announce (r/linux, LWN, Phoronix, Mastodon).
- [ ] Open the next milestone (`v26.09 — backup & snapshots`).
