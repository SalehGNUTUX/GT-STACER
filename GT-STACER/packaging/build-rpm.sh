#!/usr/bin/env bash
# GT-STACER RPM Package Builder
# Developer: GNUTUX | License: GPL v3
# Usage: ./packaging/build-rpm.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build-rpm"
APP_NAME="gt-stacer"
VERSION="26.05"
RELEASE="1.beta"
ARCH="$(uname -m)"

echo "======================================"
echo "  Building $APP_NAME $VERSION .rpm"
echo "  Arch: $ARCH | GNUTUX"
echo "======================================"

# --- Check dependencies ---
for cmd in cmake ninja rpmbuild; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "[ERROR] '$cmd' not found."
        echo "  Fedora/RHEL: sudo dnf install rpm-build cmake ninja-build"
        exit 1
    fi
done

# --- Build first ---
echo "[1/3] Building with CMake..."
cmake -B "$BUILD_DIR/cmake" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "$ROOT_DIR"
cmake --build "$BUILD_DIR/cmake" --parallel "$(nproc)"

# --- Prepare RPM build tree ---
echo "[2/3] Preparing RPM build tree..."
RPM_BUILD="$BUILD_DIR/rpmbuild"
mkdir -p "$RPM_BUILD"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# Create tarball of source
TARNAME="${APP_NAME}-${VERSION}"
mkdir -p "/tmp/${TARNAME}"
cp -r "$ROOT_DIR"/* "/tmp/${TARNAME}/" 2>/dev/null || true
tar -czf "$RPM_BUILD/SOURCES/${TARNAME}.tar.gz" -C /tmp "${TARNAME}"
rm -rf "/tmp/${TARNAME}"

# --- Write spec file ---
cat > "$RPM_BUILD/SPECS/${APP_NAME}.spec" <<SPEC
Name:           ${APP_NAME}
Version:        ${VERSION}
Release:        ${RELEASE}%{?dist}
Summary:        Linux System Optimizer and Monitor by GNUTUX
License:        GPLv3+
URL:            https://github.com/SalehGNUTUX/GT-STACER
Source0:        ${TARNAME}.tar.gz
BuildArch:      ${ARCH}

BuildRequires:  cmake >= 3.24
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.2
BuildRequires:  qt6-qtsvg-devel >= 6.2

Requires:       qt6-qtbase >= 6.2
Requires:       qt6-qtsvg >= 6.2
Requires:       systemd
Requires:       polkit

Recommends:     flatpak

%description
GT-STACER is a modern Linux system optimizer and monitoring tool by GNUTUX.

Features include:
- Dashboard with real-time CPU, GPU, memory, disk, and battery status
- Process manager with kill support
- Systemd service manager
- Startup applications manager
- System cleaner (cache, logs, trash)
- Package uninstaller (APT + Flatpak + Snap)
- APT source manager
- Temperature and GPU monitoring
- /etc/hosts editor
- Dark and light themes
- Wayland compatible

Licensed under the GNU General Public License v3 or later.

%prep
%autosetup

%build
%cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%post
update-desktop-database %{_datadir}/applications &>/dev/null || :
gtk-update-icon-cache -f %{_datadir}/icons/hicolor &>/dev/null || :

%postun
update-desktop-database %{_datadir}/applications &>/dev/null || :
gtk-update-icon-cache -f %{_datadir}/icons/hicolor &>/dev/null || :

%files
%license LICENSE
%{_bindir}/gt-stacer
%{_datadir}/applications/gt-stacer.desktop
%{_datadir}/icons/hicolor/*/apps/gt-stacer.png

%changelog
* $(date "+%a %b %d %Y") GNUTUX <gnutux.arabic@gmail.com> - ${VERSION}-${RELEASE}
- Initial GT-STACER release based on modernized Stacer fork
- Qt6, C++17, GPU monitoring, Flatpak support, Temperature sensors
- Wayland compatible, modern dark/light themes
SPEC

# --- Build RPM ---
echo "[3/3] Building RPM..."
rpmbuild \
    --define "_topdir $RPM_BUILD" \
    -bb "$RPM_BUILD/SPECS/${APP_NAME}.spec"

# Copy output
find "$RPM_BUILD/RPMS" -name "*.rpm" -exec cp {} "$ROOT_DIR/" \;
OUTPUT_FILE=$(find "$ROOT_DIR" -maxdepth 1 -name "${APP_NAME}*.rpm" | head -1)

echo ""
echo "======================================"
echo "  RPM package built: $OUTPUT_FILE"
echo "  Install: sudo dnf install $OUTPUT_FILE"
echo "======================================"
