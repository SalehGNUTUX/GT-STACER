#!/usr/bin/env bash
# GT-STACER AppImage Builder
# Developer: GNUTUX | License: GPL v3
# Usage: ./packaging/build-appimage.sh [--arch x86_64|aarch64]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build-appimage"
APPDIR="$BUILD_DIR/AppDir"
APP_NAME="GT-STACER"
APP_ID="org.gnutux.gt-stacer"
VERSION="26.04"
ARCH="${1:-x86_64}"

echo "======================================"
echo "  Building $APP_NAME $VERSION AppImage"
echo "  Arch: $ARCH | GNUTUX"
echo "======================================"

# --- Check dependencies ---
for cmd in cmake ninja linuxdeploy linuxdeploy-plugin-qt; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "[ERROR] '$cmd' not found. Install it first."
        echo "  linuxdeploy: https://github.com/linuxdeploy/linuxdeploy/releases"
        echo "  linuxdeploy-plugin-qt: https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases"
        exit 1
    fi
done

# --- Configure & Build ---
echo "[1/4] Configuring with CMake..."
cmake -B "$BUILD_DIR/cmake" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "$ROOT_DIR"

echo "[2/4] Building..."
cmake --build "$BUILD_DIR/cmake" --parallel "$(nproc)"

echo "[3/4] Installing to AppDir..."
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR/cmake"

# Copy desktop file and icon to AppDir root (AppImage requirement)
cp "$APPDIR/usr/share/applications/gt-stacer.desktop" "$APPDIR/"
cp "$APPDIR/usr/share/icons/hicolor/256x256/apps/gt-stacer.png" "$APPDIR/"

# AppStream metadata
mkdir -p "$APPDIR/usr/share/metainfo"
cat > "$APPDIR/usr/share/metainfo/${APP_ID}.appdata.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>${APP_ID}</id>
  <name>${APP_NAME}</name>
  <summary>Linux System Optimizer and Monitor</summary>
  <metadata_license>MIT</metadata_license>
  <project_license>GPL-3.0+</project_license>
  <description>
    <p>GT-STACER is a modern Linux system optimizer and monitor by GNUTUX.
       Manage processes, services, startup apps, packages (apt/flatpak/snap),
       monitor CPU/GPU/memory/temperature, clean your system, and more.</p>
  </description>
  <developer_name>GNUTUX</developer_name>
  <url type="homepage">https://github.com/SalehGNUTUX/GT-STACER</url>
  <releases>
    <release version="${VERSION}" date="2026-04-26"/>
  </releases>
</component>
EOF

echo "[4/4] Creating AppImage..."
OUTPUT_FILE="$ROOT_DIR/GT-STACER-${VERSION}-${ARCH}.AppImage"

APPIMAGE_EXTRACT_AND_RUN=1 \
QMAKE="$(command -v qmake6 || command -v qmake)" \
linuxdeploy \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

# Move AppImage to project root
mv ./*.AppImage "$OUTPUT_FILE" 2>/dev/null || true

echo ""
echo "======================================"
echo "  AppImage built: $OUTPUT_FILE"
echo "======================================"
