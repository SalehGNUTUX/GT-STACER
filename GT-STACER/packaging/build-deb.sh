#!/usr/bin/env bash
# GT-STACER DEB Package Builder
# Developer: GNUTUX | License: GPL v3
# Usage: ./packaging/build-deb.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build-deb"
PKG_DIR="$BUILD_DIR/pkg"
APP_NAME="gt-stacer"
VERSION="26.05"
CHANNEL="beta"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo 'amd64')"

echo "======================================"
echo "  Building $APP_NAME $VERSION .deb"
echo "  Arch: $ARCH | GNUTUX"
echo "======================================"

# --- Check dependencies ---
for cmd in cmake ninja dpkg-deb; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "[ERROR] '$cmd' not found."
        exit 1
    fi
done

# --- Build ---
echo "[1/4] Building with CMake..."
cmake -B "$BUILD_DIR/cmake" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "$ROOT_DIR"
cmake --build "$BUILD_DIR/cmake" --parallel "$(nproc)"

# --- Install to package staging area ---
echo "[2/4] Staging installation..."
rm -rf "$PKG_DIR"
DESTDIR="$PKG_DIR" cmake --install "$BUILD_DIR/cmake"

# --- Create DEBIAN control directory ---
echo "[3/4] Creating DEBIAN metadata..."
mkdir -p "$PKG_DIR/DEBIAN"

# Calculate installed size (in KB)
INSTALLED_SIZE=$(du -sk "$PKG_DIR/usr" | cut -f1)

cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: gt-stacer
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: GNUTUX <gnutux.arabic@gmail.com>
Installed-Size: ${INSTALLED_SIZE}
Depends: libqt6core6t64 (>= 6.2) | libqt6core6 (>= 6.2), libqt6gui6t64 (>= 6.2) | libqt6gui6 (>= 6.2), libqt6widgets6t64 (>= 6.2) | libqt6widgets6 (>= 6.2), libqt6svg6 (>= 6.2)
Recommends: flatpak, polkitd | policykit-1 | polkit
Section: utils
Priority: optional
Homepage: https://github.com/SalehGNUTUX/GT-STACER
Description: Linux System Optimizer and Monitor (GNUTUX)
 GT-STACER is a modern Linux system optimizer and monitoring tool.
 .
 Features: Dashboard, CPU/GPU/Memory/Disk/Network/Temperature monitoring,
 Process manager, Service manager (systemd), Startup apps, System cleaner,
 Package uninstaller (APT + Flatpak + Snap), APT source manager, Helpers.
 .
 Developed by GNUTUX. Licensed under the GNU GPL v3.
EOF

cat > "$PKG_DIR/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-icon-caches >/dev/null 2>&1; then
    update-icon-caches /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

cat > "$PKG_DIR/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if command -v update-icon-caches >/dev/null 2>&1; then
    update-icon-caches /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
EOF
chmod 755 "$PKG_DIR/DEBIAN/postrm"

# --- Build DEB ---
echo "[4/4] Building .deb package..."
OUTPUT_FILE="$ROOT_DIR/GT-STACER_${VERSION}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$PKG_DIR" "$OUTPUT_FILE"

echo ""
echo "======================================"
echo "  DEB package built: $OUTPUT_FILE"
echo "  Install: sudo dpkg -i $OUTPUT_FILE"
echo "======================================"
