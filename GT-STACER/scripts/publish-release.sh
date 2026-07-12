#!/usr/bin/env bash
# GT-STACER release publisher.
# Creates a git tag, pushes it, and uploads release/* as GitHub release assets.
#
# Pre-flight checklist:
#   1. `release/` contains all 3 packages + SHA256SUMS.txt
#   2. CHANGELOG.md and RELEASE_NOTES_<ver>.md updated
#   3. The working tree is clean (no uncommitted changes)
#   4. `gh auth status` is authenticated to GitHub
#
# Usage:
#   ./scripts/publish-release.sh             # publish current version from CMakeLists
#   ./scripts/publish-release.sh --dry-run   # show what would happen, no network calls

set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
DRY=0
[[ "${1:-}" == "--dry-run" ]] && DRY=1

# Read the version from CMakeLists.txt so the script can't drift out of sync.
VERSION=$(grep -E '^\s*APP_VERSION="[0-9]+\.[0-9]+"' CMakeLists.txt | head -1 \
            | sed -E 's/.*"([0-9]+\.[0-9]+)".*/\1/')
CHANNEL=$(grep -E '^\s*APP_CHANNEL="[a-z]+"' CMakeLists.txt | head -1 \
            | sed -E 's/.*"([a-z]+)".*/\1/')
TAG="v${VERSION}-${CHANNEL}"
TITLE="GT-STACER ${VERSION} ${CHANNEL}"

NOTES_FILE="RELEASE_NOTES_${VERSION}.md"
if [[ ! -f "$NOTES_FILE" ]]; then
    echo "✗ Missing $NOTES_FILE — write it before publishing." >&2
    exit 1
fi

echo "════════════════════════════════════════════════════"
echo "  Publishing $TITLE"
echo "  Tag:       $TAG"
echo "  Notes:     $NOTES_FILE"
echo "════════════════════════════════════════════════════"
echo

# ── 1. release/ artifacts ────────────────────────────────
echo "[1/4] Checking release/ artifacts"
ASSETS=()
for pat in \
    "release/GT-STACER-${VERSION}-x86_64.AppImage" \
    "release/GT-STACER_${VERSION}_amd64.deb" \
    "release/gt-stacer-${VERSION}-"*.x86_64.rpm \
    "release/SHA256SUMS.txt"; do
    found=$(ls $pat 2>/dev/null || true)
    if [[ -z "$found" ]]; then
        echo "  ✗ Missing: $pat" >&2
        echo "    Run ./scripts/build-all.sh all first." >&2
        exit 1
    fi
    for f in $found; do
        ASSETS+=("$f")
        printf "  ✓ %-55s %s\n" "$f" "$(du -h "$f" | cut -f1)"
    done
done

# ── 2. git tree clean? ───────────────────────────────────
echo
echo "[2/4] Working tree status"
if [[ -d .git ]]; then
    if [[ -n $(git status --porcelain 2>/dev/null) ]]; then
        echo "  ⚠ Working tree has uncommitted changes:"
        git status --short
        echo
        echo "  Commit or stash them, then re-run. Aborting."
        exit 1
    fi
    echo "  ✓ Working tree clean"
else
    echo "  ⚠ Not a git repo — skipping git checks (won't tag or push)."
fi

# ── 3. tag + push ────────────────────────────────────────
echo
echo "[3/4] Tag + push"
if [[ -d .git ]]; then
    if git rev-parse "$TAG" >/dev/null 2>&1; then
        echo "  ✓ Tag $TAG already exists locally."
    else
        echo "  → Creating tag $TAG"
        if (( DRY )); then
            echo "    (dry-run) git tag -a $TAG -m \"$TITLE\""
        else
            git tag -a "$TAG" -m "$TITLE"
        fi
    fi

    echo "  → Pushing tag to origin"
    if (( DRY )); then
        echo "    (dry-run) git push origin $TAG"
    else
        git push origin "$TAG" || {
            echo "  ⚠ git push failed — fix remote auth and retry."
            exit 1
        }
    fi
fi

# ── 4. gh release create ─────────────────────────────────
echo
echo "[4/4] Create GitHub release"
if ! command -v gh >/dev/null 2>&1; then
    echo "  ⚠ gh CLI not installed — install from https://cli.github.com/"
    echo "    Or use the GitHub web UI to create release $TAG and upload:"
    printf "      %s\n" "${ASSETS[@]}"
    exit 0
fi

if ! gh auth status >/dev/null 2>&1; then
    echo "  ⚠ gh not authenticated. Run: gh auth login"
    exit 1
fi

if (( DRY )); then
    echo "  (dry-run) gh release create $TAG \\"
    echo "             --title \"$TITLE\" \\"
    echo "             --notes-file $NOTES_FILE \\"
    [[ "$CHANNEL" != "stable" ]] && echo "             --prerelease \\"
    for a in "${ASSETS[@]}"; do echo "             \"$a\" \\"; done
    echo "             (last line)"
else
    EXTRA=()
    [[ "$CHANNEL" != "stable" ]] && EXTRA+=("--prerelease")
    gh release create "$TAG" \
        --title "$TITLE" \
        --notes-file "$NOTES_FILE" \
        "${EXTRA[@]}" \
        "${ASSETS[@]}"
fi

echo
echo "════════════════════════════════════════════════════"
echo "  ✅ Release published: $TAG"
echo "  https://github.com/SalehGNUTUX/GT-STACER/releases/tag/$TAG"
echo "════════════════════════════════════════════════════"
