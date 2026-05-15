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
| 26.05 beta    | ✅ current                |
| 26.04 alpha   | ⚠️ security fixes only    |
| older         | ❌ unsupported            |

We do **not** publish patches for unsupported versions. The fix lands in the
current branch and the next release.

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
  `PackageTool::remove`, `ServiceTool::{start,stop,enable,disable}`,
  `AptSourceTool::{add,remove,setEnabled}`, `HelpersPage::saveHosts`,
  `HelpersPage::flushDns`, `HelpersPage::applySwappiness`,
  `SystemCleanerPage::cleanCategory` for root strategies.
- **`CommandUtil::pkexecWriteFile`** — temp-file-then-install pattern.
- **`CommandUtil::isSafeIdentifier`** — input sanitisation gatekeeper.
- **Path traversal** in cleaner / cache walkers (`/var/log`, `~/.cache`,
  `~/.local/share/Trash`).
- **TOCTOU** between scan and clean phases in System Cleaner.
- **Notification spoofing** if libnotify args were ever shell-built.
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

## Past advisories

| ID    | Date       | Severity | Summary                                              |
|-------|------------|----------|------------------------------------------------------|
| —     | 2026-05-14 | High     | Command-injection in `/etc/hosts` editor (v26.04). Fixed in v26.05 by routing writes through `pkexec install` instead of `pkexec sh -c 'echo "%1" > …'`. Discovered internally during code review; no known exploitation in the wild. |

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

Security review of v26.05 was performed during the 2026-05-14 audit; all
findings from that pass are listed above.

Thanks to **Oguzhan INAN** for the original Stacer codebase and to every
contributor who reports issues responsibly.
