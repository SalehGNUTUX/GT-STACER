# Security policy

GT-STACER runs privileged operations through `pkexec` (polkit) on behalf of
the user — package removal, service control, `/etc/hosts` editing, log
cleanup, kernel sysctl tuning. A vulnerability in that path can mean
arbitrary code execution as **root**, so we take this seriously.

If you find anything that lets an unprivileged caller escalate, please
report it before disclosing.

---

## Supported versions

| Version       | Supported                |
|---------------|--------------------------|
| 26.11.3 stable | ✅ current (recommended) |
| 26.11.2 stable | ⚠️ superseded by 26.11.3  |
| 26.11.1 stable | ⚠️ superseded              |
| 26.11 stable  | ⚠️ superseded              |
| 26.10 stable  | ⚠️ security fixes only    |
| 26.09 stable  | ❌ unsupported            |
| 26.08 stable  | ❌ unsupported            |
| 26.07 stable  | ❌ unsupported            |
| older         | ❌ unsupported            |

We do **not** publish patches for unsupported versions. The fix lands in the
current branch and the next release.

> **Fixed in 26.11:** a command-injection vulnerability in package search — the
> query was passed to a shell (`sh -c`), so a crafted term could run arbitrary
> commands. It is now passed as argv (no shell) and validated. Every
> install / upgrade / remove path added in 26.11 goes through
> `CommandUtil::execProgram` with `isSafeIdentifier`-validated names; store
> add-on and AppImage removal are path-guarded. Users on 26.10 or earlier should
> upgrade.

---

## How to report a vulnerability

**Do not open a public GitHub issue.** Public disclosure of an
unpatched vulnerability puts every running install at risk.

### Preferred channels

1. **Email** — `gnutux.arabic@gmail.com` with subject prefix
   `[GT-STACER security]`. PGP is welcome but not required.
2. **GitHub private vulnerability report** —
   <https://github.com/SalehGNUTUX/GT-STACER/security/advisories/new>
   (visible to maintainers only).

Please include:

- A clear description of the vulnerability and its impact.
- Steps to reproduce (commands, sample input, distro/version).
- The affected version (`gt-stacer --version` or commit hash).
- Any proof-of-concept code or shell session capture.
- Your preferred name / handle for credit in the changelog (or "anonymous").

### What to expect

| Phase                | Target turnaround                     |
|----------------------|---------------------------------------|
| Acknowledgement      | within 72 hours                       |
| Initial assessment   | within 7 days                         |
| Fix + release        | depends on severity, typically ≤ 30 d |
| Public disclosure    | after the fix ships, with credit      |

If the issue is **critical** (remote code execution, root escalation from
a non-administrator user), expect us to move faster and to coordinate
disclosure with you.

---

## In scope

These are the surfaces we consider security-relevant:

- **`pkexec` callers** — any path that constructs a privileged command:
  `PackageTool::remove`, `PackageTool::cleanCache`, `ServiceTool::{start,stop,enable,disable}`,
  `AptSourceTool::{add,remove,setEnabled}`, `HelpersPage::saveHosts`,
  `HelpersPage::flushDns`, `HelpersPage::applySwappiness`,
  `SystemCleanerPage::cleanCategory` for root strategies,
  `PkgCacheDialog::cleanSelected`, `UninstallerPage::uninstallSelected`
  (multi-package loop).
- **`CommandUtil::pkexecWriteFile`** — temp-file-then-install pattern.
- **`CommandUtil::isSafeIdentifier`** — input-sanitisation gatekeeper.
- **`CommandUtil::execProgram(prog, args)`** — replaces the previous
  shell-out path. All sensitive callers must use it, never `exec()` /
  `execStatus()` which go through `/bin/sh -c`.
- **Path traversal** in cleaner / cache walkers (`/var/log`, `~/.cache`,
  `~/.local/share/Trash`, `/var/cache/<manager>`, snap/flatpak app dirs).
- **TOCTOU** between scan and clean phases in System Cleaner, including
  the new Package Cache and Flatpak/Snap drill-down dialogs.
- **Notification spoofing** if `notify-send` args were ever shell-built.
- **Translation injection** — `.ts` files end up parsed at runtime, so
  malicious `<source>`/`<translation>` payloads count if they can poison a
  build.

## Out of scope

- Bugs requiring **physical access** or **pre-existing root** to exploit
  (we can't defend the user from themselves once they have `sudo`).
- Theoretical attacks against `polkit` itself — report those to the polkit
  project upstream.
- Issues in third-party packages (Qt6, Wayland compositor, glibc).
- "I deleted important files using the cleaner" — read the warnings; we
  added a confirmation dialog and per-category opt-in in v26.05 precisely
  to make this a deliberate action.
- Self-XSS in About / Settings rich-text labels (no remote input feeds them).

---

## Flatpak distribution

GT-STACER 26.08 also ships as a Flatpak (`org.gnutux.gt-stacer` on
`org.kde.Platform//6.9`). The Flatpak does **not** change the privilege
model — it adds a transparent wrapper:

- **pkexec still gates every privileged op.** Inside the sandbox the binary
  has no `pkexec` at all; `CommandUtil::wrapForHost()` rewrites every
  privileged call as `flatpak-spawn --host pkexec …`, so the polkit Authority
  on the **host** is what authorises the action. The polkit dialog, the
  policies that apply, and the actor on the wire are identical to the
  DEB/RPM/AppImage case.
- **The portal is a chokepoint, not a bypass.** `flatpak-spawn` is gated by
  `org.freedesktop.Flatpak` on the session bus. That permission is granted
  once via the manifest (`--talk-name=org.freedesktop.Flatpak`) and is what
  the user implicitly accepts when installing the Flatpak. An unprivileged
  caller inside the sandbox cannot reach pkexec by any other route.
- **Temp file path widens slightly.** `pkexecWriteFile()` stages its content
  in `~/.cache/gt-stacer-tmp/` under Flatpak instead of `/tmp`, because the
  sandbox's `/tmp` is private to the app. The temp file is still
  user-owned, mode `0600`, removed on close, and the privileged step is
  `pkexec install -o root -m 644 <tmp> <dest>` — no shell, no metacharacter
  expansion. The widening only means a malicious *local* user with the same
  uid sees the staged file briefly; the existing threat model already
  trusts the local uid.
- **Sandbox detection cannot be spoofed by attacker input.** Both signals
  are read-only at process start (`FLATPAK_ID` env var, `/.flatpak-info`
  file) and come from `flatpak run` itself. There is no code path in
  GT-STACER that lets remote/user data influence whether `wrapForHost()`
  fires.
- **Out of scope for the Flatpak build specifically:**
  - Container escape from `org.kde.Platform`. Report those to Flatpak / KDE.
  - Mis-trusted runtime updates pulled by `flatpak update`. We don't
    distribute the runtime.
  - Attacks that require the user to add a malicious extra remote
    (`flatpak --user remote-add …`) before installing.

When reporting a Flatpak-only issue, please include:
`flatpak --version`, `flatpak info org.gnutux.gt-stacer`, and the output of
`flatpak-spawn --host pkexec --version` from inside the sandbox so we can
distinguish app bugs from portal / runtime issues.

---

## Past advisories

| ID    | Date       | Severity | Summary                                              |
|-------|------------|----------|------------------------------------------------------|
| —     | 2026-05-14 | High     | Command-injection in `/etc/hosts` editor (v26.04). Fixed in v26.05 by routing writes through `pkexec install` instead of `pkexec sh -c 'echo "%1" > …'`. Discovered internally during code review; no known exploitation in the wild. |
| —     | 2026-05-15 | Medium   | Several APT/service/cleaner callers still went through `pkexec sh -c …` in v26.05's first cut. All were ported to `execProgram(prog, args)` (no shell) before the v26.05 beta and reverified for v26.06. No exploit reported. |

(This table will grow as we publish further advisories. Each row links to a
GitHub Security Advisory once the fix ships.)

---

## Hardening notes for distributors

If you're packaging GT-STACER for a distro:

- Build with the default release flags — they enable `--gc-sections` and
  `--as-needed`, both of which reduce attack surface marginally.
- Don't ship a setuid binary. **Nothing in GT-STACER should ever be setuid.**
  All privilege escalation goes through `pkexec`.
- The `polkit` rule for our app is the default — no custom policy file is
  installed. If your distro wants finer-grained control, ship one of your
  own; do not patch our `pkexec` callers to skip authorisation.
- The bundled `notify-send` shell-out is intentional — we avoid linking
  libnotify to keep the dependency surface small. Don't replace it with a
  D-Bus call without auditing the input path again.

---

## Credits

Maintained by **GNUTUX** (`gnutux.arabic@gmail.com`).

Security review of v26.05 was performed during the 2026-05-14 audit, and a
follow-up sweep alongside the 26.06 stable rollout on 2026-05-15. All
findings from both passes are listed above.

Thanks to **Oguzhan INAN** for the original Stacer codebase and to every
contributor who reports issues responsibly.
