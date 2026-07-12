#!/usr/bin/env bash
# GT-STACER Flatpak builder.
#
# Produces a portable `.flatpak` bundle (single file) that any Flatpak user can
# install with `flatpak install ./GT-STACER-<version>-x86_64.flatpak`.
#
# Usage:
#   ./packaging/build-flatpak.sh
#   ./packaging/build-flatpak.sh --install   # also install locally
#   ./packaging/build-flatpak.sh --repo-only # produce a repo dir, no bundle

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
FLATPAK_DIR="$SCRIPT_DIR/flatpak"
MANIFEST="$FLATPAK_DIR/org.gnutux.gt-stacer.yaml"
APP_ID="org.gnutux.gt-stacer"
ARCH="$(uname -m)"

# Pull version from CMakeLists so the bundle is named consistently with the
# other release artifacts.
VERSION="$(grep -E '^\s*APP_VERSION="[0-9]+\.[0-9]+"' "$ROOT_DIR/CMakeLists.txt" \
            | head -1 | sed -E 's/.*"([0-9]+\.[0-9]+)".*/\1/')"
OUT_BUNDLE="$ROOT_DIR/release/GT-STACER-${VERSION}-${ARCH}.flatpak"

INSTALL=0
REPO_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --install)   INSTALL=1   ;;
        --repo-only) REPO_ONLY=1 ;;
    esac
done

echo "═══════════════════════════════════════════════════════════"
echo "  Building Flatpak bundle"
echo "  App-id : $APP_ID"
echo "  Version: $VERSION"
echo "  Arch   : $ARCH"
echo "  Output : $OUT_BUNDLE"
echo "═══════════════════════════════════════════════════════════"
echo

# ── 1. Pre-flight ───────────────────────────────────────────
for cmd in flatpak flatpak-builder; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "✗ '$cmd' not installed."
        echo "  Debian/Ubuntu: sudo apt install flatpak flatpak-builder"
        echo "  Fedora:        sudo dnf install flatpak flatpak-builder"
        echo "  Arch:          sudo pacman -S flatpak flatpak-builder"
        exit 1
    fi
done

# ── 2. Ensure runtimes are present ─────────────────────────
RUNTIME_VERSION="$(grep -E '^runtime-version:' "$MANIFEST" | sed -E "s/.*'([0-9.]+)'/\1/")"
RUNTIME_REF="org.kde.Platform//${RUNTIME_VERSION}"
SDK_REF="org.kde.Sdk//${RUNTIME_VERSION}"

echo "[1/4] Checking runtimes (org.kde.Platform/Sdk ${RUNTIME_VERSION})"
if ! flatpak --user list --runtime 2>/dev/null | grep -q "org.kde.Platform.*${RUNTIME_VERSION}"; then
    echo "    → Installing $RUNTIME_REF from flathub"
    flatpak --user install -y --noninteractive flathub "$RUNTIME_REF" || {
        echo "    Hint: make sure flathub is configured:"
        echo "      flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo"
        exit 1
    }
fi
if ! flatpak --user list --runtime 2>/dev/null | grep -q "org.kde.Sdk.*${RUNTIME_VERSION}"; then
    echo "    → Installing $SDK_REF from flathub"
    flatpak --user install -y --noninteractive flathub "$SDK_REF"
fi
echo "    ✓ Runtimes ready"
echo

# ── 3. Build ────────────────────────────────────────────────
BUILD_DIR="$ROOT_DIR/build-flatpak"
REPO_DIR="$ROOT_DIR/build-flatpak-repo"
rm -rf "$BUILD_DIR"

echo "[2/4] Running flatpak-builder (this takes a few minutes)"
flatpak-builder \
    --user \
    --force-clean \
    --repo="$REPO_DIR" \
    --install-deps-from=flathub \
    "$BUILD_DIR" \
    "$MANIFEST"
echo

if (( REPO_ONLY )); then
    echo "[3/4] --repo-only requested — skipping bundle"
    echo "    Repo at: $REPO_DIR"
    exit 0
fi

# ── 4. Bundle ───────────────────────────────────────────────
mkdir -p "$ROOT_DIR/release"
echo "[3/4] Creating single-file bundle"
flatpak build-bundle "$REPO_DIR" "$OUT_BUNDLE" "$APP_ID"
echo "    ✓ $OUT_BUNDLE ($(du -h "$OUT_BUNDLE" | cut -f1))"
echo

if (( INSTALL )); then
    echo "[4/4] Installing locally"
    flatpak --user install -y --reinstall --noninteractive "$OUT_BUNDLE"
    echo "    ✓ Installed — launch with: flatpak run $APP_ID"
else
    echo "[4/4] Skipped install (pass --install to install locally)"
fi

echo
echo "═══════════════════════════════════════════════════════════"
echo "  ✅ Flatpak bundle: $OUT_BUNDLE"
echo "  Install: flatpak install --user $OUT_BUNDLE"
echo "  Run:     flatpak run $APP_ID"
echo "═══════════════════════════════════════════════════════════"
