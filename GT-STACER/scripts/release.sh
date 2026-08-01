#!/usr/bin/env bash
# release.sh — one-shot release automation for GT-STACER.
#
# Run this MANUALLY once the version bump + docs + website are updated locally.
# It automates the whole PUBLISHING.md procedure:
#   1. build all four packages (DEB/RPM/AppImage/Flatpak)
#   2. verify them + generate SHA256SUMS.txt
#   3. fill the real size + SHA-256 into RELEASE_NOTES + the website download cards
#   4. push the app -> repo GT-STACER/ subfolder and the website -> repo root
#   5. create the GitHub release (tag GT-STACER_<ver>_STABLE, marked latest)
#   6. verify the published DEB installs metadata (deps) and downloads clean
#
# The version is read from CMakeLists.txt, so this script is reusable every release.
#
# Usage:
#   ./scripts/release.sh              # full release (asks once before pushing)
#   ./scripts/release.sh --dry-run    # build + fill + show the plan; NO push/release
#   ./scripts/release.sh --skip-build # reuse packages already in release/
#   ./scripts/release.sh --yes        # don't prompt before the push
set -uo pipefail

# ── paths ────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(dirname "$SCRIPT_DIR")"                     # GT-STACER-APP
WORKSPACE="$(dirname "$APP_DIR")"                      # holds both projects
WEB_DIR="$WORKSPACE/GT-STACER-WEB"
REL="$APP_DIR/release"
REPO="SalehGNUTUX/GT-STACER"
REMOTE="https://github.com/$REPO.git"

# ── flags ────────────────────────────────────────────────────────────────────
DRY=0; SKIP_BUILD=0; YES=0
for a in "$@"; do case "$a" in
  --dry-run)    DRY=1 ;;
  --skip-build) SKIP_BUILD=1 ;;
  --yes|-y)     YES=1 ;;
  -h|--help)    sed -n '2,26p' "$0" | sed 's/^# \?//'; exit 0 ;;
  *) echo "unknown flag: $a (see --help)"; exit 2 ;;
esac; done

# ── pretty ───────────────────────────────────────────────────────────────────
say(){ printf '\n\033[1;34m▶ %s\033[0m\n' "$*"; }
ok(){  printf '  \033[32m✓\033[0m %s\n' "$*"; }
warn(){ printf '  \033[33m⚠ %s\033[0m\n' "$*"; }
die(){ printf '  \033[31m✗ %s\033[0m\n' "$*" >&2; exit 1; }
confirm(){ (( YES )) && return 0; read -rp "  $1 [y/N] " r; [[ $r == y || $r == Y ]]; }

# ── version ──────────────────────────────────────────────────────────────────
VERSION=$(grep -oP 'APP_VERSION="\K[0-9.]+' "$APP_DIR/CMakeLists.txt") \
  || die "cannot read APP_VERSION from CMakeLists.txt"
TAG="GT-STACER_${VERSION}_STABLE"
TITLE="GT-STACER ${VERSION} STABLE"
NOTES="$APP_DIR/RELEASE_NOTES_${VERSION}.md"
say "GT-STACER release  —  version $VERSION  tag $TAG  (dry-run=$DRY)"

# ── preflight ────────────────────────────────────────────────────────────────
say "Preflight"
command -v gh   >/dev/null || die "gh (GitHub CLI) is not installed"
command -v git  >/dev/null || die "git is not installed"
command -v rsync>/dev/null || die "rsync is not installed"
gh auth status >/dev/null 2>&1 || die "gh is not authenticated — run: gh auth login"
[[ -f "$NOTES"   ]] || die "missing release notes: $NOTES"
[[ -d "$WEB_DIR" ]] || die "website dir not found: $WEB_DIR"
for f in packaging/build-deb.sh packaging/build-rpm.sh packaging/build-appimage.sh scripts/build-all.sh; do
  grep -q "VERSION=\"$VERSION\"" "$APP_DIR/$f" \
    || die "version mismatch: $f is not VERSION=\"$VERSION\" (bump it first)"
done
node --check "$WEB_DIR/assets/app.js" 2>/dev/null && ok "website app.js parses" || warn "could not node --check app.js"
ok "gh authed · notes present · packaging scripts at $VERSION"

# ── build ────────────────────────────────────────────────────────────────────
if (( SKIP_BUILD )); then
  say "Build — skipped (--skip-build)"
else
  say "Build — moving old-version packages out of release/ (alien globs release/*.deb)"
  mkdir -p "$APP_DIR/release-archive"
  find "$REL" -maxdepth 1 -type f \
       \( -name '*.deb' -o -name '*.rpm' -o -name '*.AppImage' -o -name '*.flatpak' \) \
       ! -name "*${VERSION}*" -exec mv -t "$APP_DIR/release-archive/" {} + 2>/dev/null || true
  rm -f "$REL/GT-STACER_${VERSION}_amd64.deb" "$REL"/gt-stacer-"${VERSION}"-*.rpm 2>/dev/null || true

  say "Build — all four packages (a few minutes; -Os + LTO)"
  # sudo no-op shim: build deps are already installed on the build box, and this
  # avoids install_deps hanging on a password prompt. If a build dep is missing,
  # run `./scripts/build-all.sh install-deps` once with real sudo first.
  SHIM=$(mktemp -d); printf '#!/bin/sh\nexit 0\n' > "$SHIM/sudo"; chmod +x "$SHIM/sudo"
  ( cd "$APP_DIR" && PATH="$SHIM:$PATH" ./scripts/build-all.sh all ) || { rm -rf "$SHIM"; die "build failed"; }
  rm -rf "$SHIM"
fi

# ── locate + verify artifacts ────────────────────────────────────────────────
say "Verify artifacts"
APPIMAGE="$REL/GT-STACER-${VERSION}-x86_64.AppImage"
DEB="$REL/GT-STACER_${VERSION}_amd64.deb"
RPM=$(ls "$REL"/gt-stacer-"${VERSION}"-*.x86_64.rpm 2>/dev/null | head -1)
FLATPAK="$REL/GT-STACER-${VERSION}-x86_64.flatpak"
for f in "$APPIMAGE" "$DEB" "$RPM" "$FLATPAK"; do
  [[ -n "$f" && -f "$f" ]] || die "missing artifact (did the build finish?): $f"
done
DEPS=$(dpkg-deb -f "$DEB" Depends 2>/dev/null)
grep -q libqt6dbus6    <<<"$DEPS" || die "DEB does not declare libqt6dbus6 — add it to qt_deps in scripts/build-all.sh AND packaging/build-deb.sh"
grep -q libqt6network6 <<<"$DEPS" || warn "DEB does not declare libqt6network6"
ok "4 artifacts present · DEB declares libqt6dbus6"
ok "RPM = $(basename "$RPM")"

# ── checksums ────────────────────────────────────────────────────────────────
say "Checksums"
( cd "$REL" && sha256sum \
    "$(basename "$APPIMAGE")" "$(basename "$DEB")" "$(basename "$RPM")" "$(basename "$FLATPAK")" \
    > SHA256SUMS.txt )
ok "SHA256SUMS.txt:"
sed 's/^/      /' "$REL/SHA256SUMS.txt"

# ── fill size + SHA into RELEASE_NOTES + website download cards ───────────────
say "Fill size + SHA-256 in RELEASE_NOTES and website"
python3 - "$REL" "$NOTES" "$WEB_DIR/assets/app.js" <<'PY'
import sys, re, subprocess, os
reldir, notes, appjs = sys.argv[1], sys.argv[2], sys.argv[3]
sha = {}
for line in open(os.path.join(reldir, "SHA256SUMS.txt"), encoding="utf-8"):
    h, fn = line.split(maxsplit=1); sha[fn.strip()] = h
def human(fn):  # du -h "52M" -> "52 MB"
    s = subprocess.check_output(["du","-h",os.path.join(reldir,fn)]).split()[0].decode()
    return re.sub(r'([KMG])$', lambda m: ' '+m.group(1)+'B', s)
size = {fn: human(fn) for fn in sha}

# RELEASE_NOTES table rows:  | `filename` | SIZE | `SHA` |
# newline='' preserves the file's existing line endings (app.js is CRLF) so the
# fill only changes the size/sha substrings, not every line.
t = open(notes, encoding="utf-8", newline='').read(); n=0
for fn in sha:
    pat = re.compile(r'(\|\s*`' + re.escape(fn) + r'`\s*\|)[^|]*\|[^|]*\|')
    repl = r'\1 ' + size[fn] + f' | `{sha[fn]}` |'
    t, c = pat.subn(repl, t); n += c
open(notes, "w", encoding="utf-8", newline='').write(t)
print(f"    RELEASE_NOTES: filled {n} row(s)")

# website download cards: size:/sha256: precede filename: within each card
j = open(appjs, encoding="utf-8", newline='').read()
def card(m):
    fn = m.group('fn')
    if fn not in sha: return m.group(0)
    body = m.group(0)
    body = re.sub(r"size:\s*'[^']*'",   f"size: '{size[fn]}'", body, count=1)
    body = re.sub(r"sha256:\s*'[^']*'", f"sha256: '{sha[fn]}'", body, count=1)
    return body
pat = re.compile(r"size:\s*'[^']*',\s*sha256:\s*'[^']*',\s*url:[^\n]*\n\s*filename:\s*'(?P<fn>[^']+)'", re.S)
j, c = pat.subn(card, j)
open(appjs, "w", encoding="utf-8", newline='').write(j)
print(f"    website app.js: filled {c} card(s)")
PY
node --check "$WEB_DIR/assets/app.js" >/dev/null 2>&1 || die "app.js broke after fill — inspect it"
ok "docs + website updated with real checksums"

if (( DRY )); then
  say "DRY-RUN complete — packages built, checksums filled locally. Nothing pushed."
  echo "  Re-run without --dry-run to push + create the release."
  exit 0
fi

# ── push (clone + rsync; no local .git) ──────────────────────────────────────
say "Push — cloning $REPO"
confirm "Build looks good. Push code + website to main and publish release $TAG?" || die "aborted by user"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
git clone --depth 1 "$REMOTE" "$TMP/repo" >/dev/null 2>&1 || die "clone failed"

# app -> repo/GT-STACER/  (exclude build artifacts, caches, local-only files)
rsync -a --delete \
  --exclude='build*/' --exclude='release/' --exclude='release-archive/' \
  --exclude='.build-tools/' --exclude='.flatpak-builder/' --exclude='.git/' \
  --exclude='.claude/' --exclude='CLAUDE.md' --exclude='claude*.txt' \
  --exclude='*.log' --exclude='Text File.txt' --exclude='.DS_Store' \
  "$APP_DIR/" "$TMP/repo/GT-STACER/"

# website -> repo root: NEVER --delete at root (would wipe GT-STACER/ and .github/)
rsync -a "$WEB_DIR/index.html" "$WEB_DIR/sw.js" "$WEB_DIR/manifest.webmanifest" "$TMP/repo/"
rsync -a --delete "$WEB_DIR/assets/" "$TMP/repo/assets/"
rsync -a --delete "$WEB_DIR/fonts/"  "$TMP/repo/fonts/"
rsync -a --delete "$WEB_DIR/images/" "$TMP/repo/images/"

( cd "$TMP/repo" \
  && git add -A \
  && [[ -z "$(git status --short | grep '\.github')" ]] || die "refusing to push: .github/ would change" )
CHANGES=$(cd "$TMP/repo" && git status --short | wc -l)
[[ "$CHANGES" -gt 0 ]] || { warn "no changes to push"; }
( cd "$TMP/repo" \
  && git commit -q -m "$VERSION مستقر: نشر آليّ عبر release.sh

الحُزَم على GitHub Release ($TAG)، والموقع والكود محدَّثان." \
  && git push origin HEAD:main ) || die "git push failed — fix remote auth and retry"
ok "pushed $CHANGES change(s) to main"

# ── github release ───────────────────────────────────────────────────────────
say "GitHub release $TAG"
gh release create "$TAG" --repo "$REPO" \
  --title "$TITLE" --notes-file "$NOTES" --latest --target main \
  "$APPIMAGE" "$DEB" "$RPM" "$FLATPAK" "$REL/SHA256SUMS.txt" \
  || die "gh release create failed"
ok "release published: https://github.com/$REPO/releases/tag/$TAG"

# ── verify the published DEB ─────────────────────────────────────────────────
say "Verify published DEB"
VT=$(mktemp -d)
gh release download "$TAG" --repo "$REPO" --pattern "*_amd64.deb" --dir "$VT" >/dev/null 2>&1 \
  && [[ "$(sha256sum "$VT"/*.deb | cut -d' ' -f1)" == "${sha_deb:-$(grep _amd64.deb "$REL/SHA256SUMS.txt" | cut -d' ' -f1)}" ]] \
  && ok "published DEB checksum matches" || warn "could not verify published DEB checksum"
rm -rf "$VT"

say "Done — GT-STACER $VERSION is live."
echo "  Next: install the published DEB and confirm it launches (pkexec dpkg -i ...; gt-stacer)."
