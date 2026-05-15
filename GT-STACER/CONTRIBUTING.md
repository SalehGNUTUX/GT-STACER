# Contributing to GT-STACER

Thanks for your interest in improving GT-STACER. This guide collects the
practical bits — how to build, test, translate, and submit changes — so you
can get something useful merged with minimum friction.

If you only want to translate, jump straight to
[Translations](#translations).

---

## Quick start

```bash
git clone https://github.com/SalehGNUTUX/GT-STACER.git
cd GT-STACER

# One-shot build that auto-installs deps for Debian / Fedora / Arch
./scripts/build-all.sh build

# Or, if you have Qt6 already:
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build
./build/gt-stacer/gt-stacer
```

Minimum: **CMake 3.24 · C++17 · Qt 6.2** · `lrelease` (Qt linguist tools).

---

## Project layout

```
gt-stacer-core/      Pure C++ — system interaction, no UI.
  Info/              CPU, memory, disk, network, GPU, temperature, battery, processes.
  Tools/             Service / package / startup / APT-source / notification.
  Utils/             Command, file, format helpers.

gt-stacer/           Qt6 Widgets application.
  Managers/          AppManager, SettingManager, InfoManager, AlertManager, ToolManager.
  Widgets/           CircularGauge, LineChart, PerCoreBars, Sidebar, CleanerCard, …
  Dialogs/           Welcome, About, Quit-confirm, App-cache details, Startup-add, …
  Pages/             Dashboard, Resources, Processes, Services, StartupApps,
                     SystemCleaner, Uninstaller, AptSourceManager, Settings, Helpers.
  static/themes/     QSS for dark + light themes.

translations/        Qt linguist .ts files (one per language).
packaging/           build-deb.sh · build-rpm.sh · build-appimage.sh
scripts/build-all.sh Auto-detect distro, install deps, build any/all packages.
tests/               Stand-alone test programs (security_test, core_test).
```

---

## How to build

### Debug (best for development — symbols, faster iteration)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build
./build/gt-stacer/gt-stacer
```

### Release (what we ship)

```bash
cmake -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-rel
strip --strip-unneeded build-rel/gt-stacer/gt-stacer
```

Release flags: `-Os -ffunction-sections -fdata-sections` plus
`-Wl,--gc-sections --as-needed` for tight binaries. IPO is enabled by CMake.

### Packaging

```bash
./packaging/build-deb.sh        # .deb for Debian/Ubuntu
./packaging/build-rpm.sh        # .rpm for Fedora/RHEL/Suse
./packaging/build-appimage.sh   # universal AppImage (needs linuxdeploy)
```

---

## Tests

Two small stand-alone test programs live in `tests/`. They link against the
core static library and exercise the public surface — no Qt Test framework
dependency, no fixtures.

```bash
# Build once
cmake -B build -G Ninja && ninja -C build

# security_test — input sanitisation, isSafeIdentifier, pkexec wrapper
g++ -std=c++17 -fPIC \
    -I/usr/include/x86_64-linux-gnu/qt6 \
    -I/usr/include/x86_64-linux-gnu/qt6/QtCore \
    tests/security_test.cpp -o /tmp/security_test \
    -L build/gt-stacer-core -l gt-stacer-core \
    -lQt6Core -lQt6Network -lQt6Concurrent -lpthread
/tmp/security_test    # expects 27/27 PASS

# core_test — CpuSampler, ProcessInfo, MemoryInfo, …
g++ -std=c++17 -fPIC … tests/core_test.cpp -o /tmp/core_test …
/tmp/core_test        # expects 21/21 PASS
```

Add a test for any non-trivial bug fix. Tests don't need to cover every
branch — they need to **make the bug observable** if it ever comes back.

---

## Code style

- **C++17**, no `using namespace std`, no `auto` for primitive types in
  function signatures. `auto` for iterators and lambdas is fine.
- 4-space indent (no tabs).
- Brace-on-same-line for control flow and one-liners; brace-on-next-line for
  type definitions and free functions is acceptable but the codebase uses
  same-line predominantly — match what's around you.
- Qt headers prefer `#include <QFoo>` over `<QtModule/QFoo>` except where
  required (`<QtConcurrent/QtConcurrent>`, `<QtCharts/QChart>`).
- Member fields prefixed `m_`. Constants `kCamelCase` or `ALL_CAPS`.
- One class per `.h` / `.cpp` pair; helper structs are fine inline.

### Comments

Match the style of the surrounding code. Two principles:

1. **No comments that describe what the next line does.** Well-named code
   already says that.
2. **Comments explain non-obvious WHY.** A hidden constraint, a workaround
   tied to a specific Qt/distro quirk, why a "naïve" approach was rejected.

Avoid commit-message-style comments ("added X", "fixes #42") — those belong
in `git log`, not the source.

### Security-sensitive code

Anything that touches `pkexec`, file paths under `/`, or constructs shell
commands needs care:

- **Never** interpolate user data into `CommandUtil::exec` / `execStatus`
  (which shell out through `/bin/sh -c`). Use `execProgram(program, args)`
  instead — it bypasses the shell entirely.
- For writing root-owned files, use `CommandUtil::pkexecWriteFile()` —
  it routes through `pkexec install`, not `echo > /file`.
- Validate identifiers (package, service, APT source names) via
  `CommandUtil::isSafeIdentifier()` before passing them anywhere downstream.
- Path-confine: cleanup routines that walk the filesystem must check the
  target stays inside the intended root (`startsWith($HOME/.cache/")`,
  `startsWith("/etc/apt/sources.list.d/")`, etc).

See [SECURITY.md](SECURITY.md) for the disclosure process.

---

## Pull-request workflow

1. **Fork** the repo, create a branch off `main`. Name it after the change:
   `fix/cleaner-var-log`, `feat/per-core-bars`, etc.
2. **Keep PRs small.** A 200-line PR gets merged in a day. A 2000-line PR
   sits for weeks.
3. **One concern per PR.** Refactoring + new feature in the same diff is
   hard to review.
4. **Test what you changed.** If you touched a `Pages/` class, launch the
   binary, open that page, drive it. UI changes need a screenshot in the PR
   description.
5. **Update translations.** If you added `tr()` strings, run
   ```bash
   /usr/lib/qt6/bin/lupdate -no-obsolete \
       gt-stacer/ gt-stacer-core/ \
       -ts translations/gt-stacer_*.ts
   ```
   then provide at least the Arabic translation for the strings you added —
   AR is the reference locale.
6. **No silent dependencies.** Don't introduce a new library without
   discussion. We deliberately avoid QML, libnotify (we shell to
   `notify-send`), libpolkit, etc.

### Commit messages

```
<scope>: <imperative verb> <what changed>

Optional 1-3 line body explaining the why if non-obvious.
```

Examples:

- `cleaner: stop deleting live logs in /var/log`
- `cpu_info: prime sampler synchronously so first read isn't all-zeros`
- `settings: add "auto" theme that follows the system colorScheme`

---

## Translations

GT-STACER ships 19 language files. **Arabic (ar) is the reference** — kept
100 % complete by the maintainer. Everything else is community-driven and
currently at 18-38 % coverage. The way to help:

### Add or improve a translation

1. Identify the file: `translations/gt-stacer_<lang>.ts`. If your language
   isn't there, copy any unfinished file and rename it.
2. Open it in **Qt Linguist** (recommended) or any text editor.

   ```bash
   /usr/lib/qt6/bin/linguist translations/gt-stacer_fr.ts
   ```

3. Translate entries marked `type="unfinished"`. Leave the surrounding XML
   alone; Linguist handles it.
4. Test by compiling:

   ```bash
   /usr/lib/qt6/bin/lrelease translations/gt-stacer_fr.ts
   ```

   If you see `0 unfinished` and the QM size is non-trivial, you're done.
5. Submit a PR with the `.ts` change. Don't commit the `.qm` — CMake
   regenerates it at build time.

### Translation conventions

- **Don't translate**: CSS rules (`font-size: 22px;`), object names
  (`primaryButton`, `dangerButton`, `infoCard`), brand strings
  (`GT-STACER`), version numbers, URLs.
- **Do translate**: every user-facing label, button, tooltip, alert message.
- **Placeholders**: `%1`, `%2`, … keep them in order. The string
  `"%1 reached %2°C"` may be reordered (`"%2°C reached by %1"`) for
  grammar — Qt looks them up by number, not position.
- **RTL languages** (Arabic, Hebrew, Persian): no special action needed in
  the .ts file. The app handles RTL layout when the locale is set.
- **Punctuation**: keep the original's trailing punctuation
  (`?`, `.`, `:`, `…`) so the UI stays consistent.

---

## Reporting bugs

[Open an issue](https://github.com/SalehGNUTUX/GT-STACER/issues) with:

- Distro + version (`lsb_release -a` or `cat /etc/os-release`)
- Qt version (`qmake6 -v` or `apt show qt6-base-dev | grep Version`)
- Steps to reproduce
- Expected vs actual behaviour
- Relevant log output: `~/.config/GNUTUX/gt-stacer/gt-stacer.log`

For **security** bugs, see [SECURITY.md](SECURITY.md) — please don't open a
public issue.

---

## License

By contributing, you agree your changes will be released under the same
**GNU GPL v3 or later** as the rest of the project. No CLA, no copyright
assignment — your name stays on your commits.
