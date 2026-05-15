#!/usr/bin/env bash
# GT-STACER — بناء تلقائي لجميع حزم GNU/Linux (AppImage + DEB + RPM)
# يثبّت المتطلبات تلقائياً ويدعم: Ubuntu/Debian · Fedora/RHEL · Arch Linux
# يدعم: rpmbuild أصلي، أو alien (DEB→RPM) على Debian/Ubuntu
# المطور: GNUTUX — رخصة GPL v3
#
# الاستخدام:
#   ./scripts/build-all.sh [all|appimage|deb|rpm|build|install-deps]
#   ./scripts/build-all.sh all            — بناء كل الحزم (AppImage+DEB+RPM)
#   ./scripts/build-all.sh build          — تهيئة وبناء فقط
#   ./scripts/build-all.sh install-deps   — تثبيت المتطلبات فقط
set -uo pipefail

cd "$(dirname "$0")/.."
ROOT_DIR="$(pwd)"
BUILD_DIR="$ROOT_DIR/build"
RELEASE_DIR="$ROOT_DIR/release"
APP_NAME="gt-stacer"
APP_DISPLAY="GT-STACER"
VERSION="26.05"
CHANNEL="beta"
ARCH="$(uname -m)"
BUILD_TOOL="Unix Makefiles"
BUILD_CMD="make"
QT6_QMAKE=""

mkdir -p "$RELEASE_DIR"

echo "══════════════════════════════════════════════════════════"
echo "   $APP_DISPLAY $VERSION — بناء تلقائي لـ GNU/Linux"
echo "   المعمارية: $ARCH  |  المطور: GNUTUX"
echo "══════════════════════════════════════════════════════════"
echo ""

# ═══════════════════════════════════════════════════════
#  كشف التوزيعة ومدير الحزم
# ═══════════════════════════════════════════════════════
detect_distro() {
    if   [ -f /etc/debian_version ];        then echo "debian"
    elif [ -f /etc/fedora-release ];        then echo "fedora"
    elif [ -f /etc/redhat-release ];        then echo "rhel"
    elif [ -f /etc/arch-release ];          then echo "arch"
    elif [ -f /etc/opensuse-release ] || [ -f /etc/SUSE-brand ]; then echo "suse"
    else echo "unknown"
    fi
}

DISTRO="$(detect_distro)"
echo "🖥️  التوزيعة المكتشفة: $DISTRO"
echo ""

# ═══════════════════════════════════════════════════════
#  تثبيت المتطلبات حسب التوزيعة
# ═══════════════════════════════════════════════════════
install_deps() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📦 تثبيت متطلبات البناء..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # التحقق من وجود sudo أو تجاوزه كـ root
    local SUDO=""
    if [ "$(id -u)" -eq 0 ]; then
        SUDO=""
    elif command -v sudo &>/dev/null; then
        SUDO="sudo"
    else
        echo "⚠️  لا يوجد sudo ولست root — تخطي التثبيت التلقائي"
        echo "   ثبّت المتطلبات يدوياً ثم أعد التشغيل"
        return 0
    fi

    case "$DISTRO" in
    debian)
        export DEBIAN_FRONTEND=noninteractive
        $SUDO apt-get update -qq 2>/dev/null || true

        # --- أدوات البناء الأساسية ---
        $SUDO apt-get install -y \
            build-essential g++ cmake make \
            pkg-config patchelf fakeroot dpkg-dev \
            libgl-dev libglu1-mesa-dev \
            wget curl rsync 2>/dev/null || true

        # --- ninja (اختياري — يُسرّع البناء) ---
        $SUDO apt-get install -y ninja-build 2>/dev/null || true

        # --- Qt6 (المتطلبات الأساسية للمشروع) ---
        local QT6_PKGS=(
            qt6-base-dev
            qt6-tools-dev
            qt6-svg-dev
            qt6-tools-dev
        )
        # حزم اختيارية (تجاهل الفشل)
        local QT6_OPT=(
            qt6-l10n-tools
            libqt6charts6-dev
            libqt6svg6-dev
            libqt6network6-dev
            libqt6concurrent6t64
        )

        if apt-cache show qt6-base-dev &>/dev/null 2>&1; then
            if ! $SUDO apt-get install -y "${QT6_PKGS[@]}" 2>/dev/null; then
                echo "⚠️  تعذّر التثبيت التلقائي لـ Qt6 (ربما بسبب صلاحيات sudo)"
                echo "   ثبّت يدوياً:"
                echo "   sudo apt-get install qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools"
            fi
            for pkg in "${QT6_OPT[@]}"; do
                $SUDO apt-get install -y "$pkg" 2>/dev/null || true
            done
        else
            echo "⚠️  Qt6 غير موجود في المستودعات. على Ubuntu < 22.04:"
            echo "   sudo add-apt-repository ppa:ubuntuhandbook1/ppa"
            echo "   sudo apt-get update"
        fi

        # --- أدوات التحزيم ---
        $SUDO apt-get install -y fuse libfuse2t64 2>/dev/null || \
        $SUDO apt-get install -y fuse libfuse2 2>/dev/null || true
        $SUDO apt-get install -y alien rpm 2>/dev/null || true
        ;;

    fedora|rhel)
        local pm
        command -v dnf5 &>/dev/null && pm="dnf5" || pm="dnf"
        command -v dnf  &>/dev/null || pm="yum"

        $SUDO $pm install -y \
            gcc-c++ cmake make ninja-build \
            mesa-libGL-devel \
            qt6-qtbase-devel \
            qt6-qttools-devel \
            qt6-qtsvg-devel \
            qt6-qttools-devel \
            rpm-build patchelf wget curl \
            fuse fuse-libs 2>/dev/null || true
        ;;

    arch)
        $SUDO pacman -Sy --noconfirm \
            base-devel cmake ninja make \
            qt6-base qt6-svg qt6-tools \
            fakeroot patchelf fuse2 wget curl 2>/dev/null || true
        ;;

    suse)
        $SUDO zypper install -yn \
            gcc-c++ cmake make ninja \
            libqt6-qtbase-devel \
            libqt6-qttools-devel \
            libqt6-qtsvg-devel \
            rpm-build patchelf wget curl fuse 2>/dev/null || true
        ;;

    *)
        echo "⚠️  توزيعة غير معروفة. المتطلبات اليدوية:"
        echo "   - cmake >= 3.24"
        echo "   - g++ أو clang++ (C++17)"
        echo "   - make أو ninja"
        echo "   - Qt6: base-dev, charts-dev, svg-dev, network"
        echo "   - dpkg-dev أو rpm-build لحزم DEB/RPM"
        ;;
    esac

    echo "✅ المتطلبات مثبّتة (أو كانت موجودة)"
    echo ""
}

# ═══════════════════════════════════════════════════════
#  التحقق من المتطلبات الأساسية
# ═══════════════════════════════════════════════════════
check_requirements() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "🔍 فحص المتطلبات..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    local errors=0

    # --- cmake ---
    if ! command -v cmake &>/dev/null; then
        echo "   ❌ cmake غير مثبّت"
        echo "      Debian/Ubuntu: sudo apt install cmake"
        echo "      Fedora:        sudo dnf install cmake"
        errors=$((errors + 1))
    else
        echo "   ✅ cmake:    $(cmake --version | head -1 | awk '{print $3}')"
    fi

    # --- مترجم C++ ---
    local CXX_COMPILER=""
    for cxx in g++ clang++ c++; do
        command -v "$cxx" &>/dev/null && { CXX_COMPILER="$(command -v "$cxx")"; break; }
    done
    if [ -z "$CXX_COMPILER" ]; then
        echo "   ❌ مترجم C++ غير موجود (g++ أو clang++)"
        echo "      Debian/Ubuntu: sudo apt install build-essential"
        errors=$((errors + 1))
    else
        local cxx_ver
        cxx_ver="$("$CXX_COMPILER" --version 2>&1 | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)"
        echo "   ✅ C++:      $(basename "$CXX_COMPILER") $cxx_ver"
    fi

    # --- أداة البناء (ninja أولاً ثم make) ---
    if command -v ninja &>/dev/null; then
        BUILD_TOOL="Ninja"
        BUILD_CMD="ninja"
        echo "   ✅ builder:  ninja $(ninja --version 2>/dev/null || true)"
    elif command -v ninja-build &>/dev/null; then
        BUILD_TOOL="Ninja"
        BUILD_CMD="ninja-build"
        # إنشاء رابط رمزي محلي
        mkdir -p "$ROOT_DIR/.local-bin"
        ln -sf "$(command -v ninja-build)" "$ROOT_DIR/.local-bin/ninja"
        export PATH="$ROOT_DIR/.local-bin:$PATH"
        echo "   ✅ builder:  ninja-build (alias created)"
    elif command -v make &>/dev/null; then
        BUILD_TOOL="Unix Makefiles"
        BUILD_CMD="make"
        echo "   ✅ builder:  make $(make --version | head -1 | awk '{print $3}')"
    else
        echo "   ❌ لا يوجد make أو ninja"
        echo "      Debian/Ubuntu: sudo apt install make"
        errors=$((errors + 1))
    fi

    # --- Qt6 ---
    QT6_QMAKE=""
    # البحث في PATH وفي المسارات الشائعة
    local _qm_candidates=(
        qmake6 qmake qt6-qmake
        /usr/lib/qt6/bin/qmake
        /usr/lib/x86_64-linux-gnu/qt6/bin/qmake
        /usr/lib/aarch64-linux-gnu/qt6/bin/qmake
        /usr/lib/armhf-linux-gnu/qt6/bin/qmake
    )
    for qm in "${_qm_candidates[@]}"; do
        local _qm_path=""
        command -v "$qm" &>/dev/null && _qm_path="$(command -v "$qm")"
        [ -z "$_qm_path" ] && [ -x "$qm" ] && _qm_path="$qm"
        [ -z "$_qm_path" ] && continue
        local _qt_ver
        _qt_ver="$("$_qm_path" -query QT_VERSION 2>/dev/null || echo "")"
        if [[ "$_qt_ver" == 6* ]]; then
            QT6_QMAKE="$_qm_path"
            break
        fi
    done
    # فحص عبر pkg-config كبديل
    if [ -z "$QT6_QMAKE" ] && pkg-config --exists Qt6Core 2>/dev/null; then
        QT6_QMAKE="qmake"  # سيُستخدم فقط لـ linuxdeploy
    fi

    if [ -z "$QT6_QMAKE" ]; then
        echo "   ❌ Qt6 غير مثبّت أو qmake6 غير موجود"
        echo "      Debian/Ubuntu: sudo apt install qt6-base-dev qt6-svg-dev qt6-tools-dev"
        echo "      Fedora:        sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel"
        errors=$((errors + 1))
    else
        local qt_ver
        qt_ver="$("$QT6_QMAKE" -query QT_VERSION 2>/dev/null || pkg-config --modversion Qt6Core 2>/dev/null || echo "?")"
        echo "   ✅ Qt6:      $qt_ver  ($QT6_QMAKE)"

        # فحص الوحدات المطلوبة
        local qt_prefix
        qt_prefix="$("$QT6_QMAKE" -query QT_INSTALL_LIBS 2>/dev/null || echo "/usr/lib")"
        local missing_mods=()
        for mod in Qt6Charts Qt6Svg Qt6Network; do
            if ! (find "$qt_prefix" /usr/lib -name "${mod}Config.cmake" 2>/dev/null | grep -q .); then
                if ! pkg-config --exists "$mod" 2>/dev/null; then
                    missing_mods+=("$mod")
                fi
            fi
        done
        if [ ${#missing_mods[@]} -gt 0 ]; then
            echo "   ⚠️  وحدات Qt6 مفقودة: ${missing_mods[*]}"
            echo "      Debian/Ubuntu: sudo apt install qt6-svg-dev qt6-tools-dev"
            echo "      Fedora:        sudo dnf install qt6-qtsvg-devel qt6-qttools-devel"
            echo "      سيفشل cmake — نفّذ install-deps أولاً"
            errors=$((errors + 1))
        fi
    fi

    echo ""
    if [ "$errors" -gt 0 ]; then
        echo "❌ $errors خطأ في المتطلبات — نفّذ أولاً:"
        echo "   ./scripts/build-all.sh install-deps"
        exit 1
    fi
    echo "✅ جميع المتطلبات متوفرة"
    echo ""
}

# ═══════════════════════════════════════════════════════
#  تنزيل linuxdeploy + plugin-qt (إذا غير موجود)
# ═══════════════════════════════════════════════════════
TOOLS_DIR="$ROOT_DIR/.build-tools"
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-${ARCH}.AppImage"
LINUXDEPLOY_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-${ARCH}.AppImage"

download_linuxdeploy() {
    mkdir -p "$TOOLS_DIR"

    if [ ! -f "$LINUXDEPLOY" ]; then
        echo "📥 تنزيل linuxdeploy..."
        local url="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
        if command -v wget &>/dev/null; then
            wget -q --show-progress -O "$LINUXDEPLOY" "$url"
        elif command -v curl &>/dev/null; then
            curl -L --progress-bar -o "$LINUXDEPLOY" "$url"
        else
            echo "❌ يلزم wget أو curl للتنزيل"
            exit 1
        fi
        chmod +x "$LINUXDEPLOY"
        echo "   ✅ linuxdeploy تم تنزيله"
    else
        echo "   ✅ linuxdeploy موجود"
    fi

    if [ ! -f "$LINUXDEPLOY_QT" ]; then
        echo "📥 تنزيل linuxdeploy-plugin-qt..."
        local url="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage"
        if command -v wget &>/dev/null; then
            wget -q --show-progress -O "$LINUXDEPLOY_QT" "$url"
        else
            curl -L --progress-bar -o "$LINUXDEPLOY_QT" "$url"
        fi
        chmod +x "$LINUXDEPLOY_QT"
        echo "   ✅ linuxdeploy-plugin-qt تم تنزيله"
    else
        echo "   ✅ linuxdeploy-plugin-qt موجود"
    fi
    echo ""
}

# ═══════════════════════════════════════════════════════
#  بناء المشروع عبر CMake
# ═══════════════════════════════════════════════════════
cmake_build() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "🔨 تهيئة CMake وبناء المشروع..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    # تهيئة CMake
    cmake -B "$BUILD_DIR" \
        -G "$BUILD_TOOL" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        "$ROOT_DIR"

    local cmake_status=$?
    if [ $cmake_status -ne 0 ]; then
        echo "❌ فشل cmake في التهيئة (كود الخطأ: $cmake_status)"
        exit 1
    fi

    # بناء
    cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || echo 4)"

    local build_status=$?
    if [ $build_status -ne 0 ]; then
        echo "❌ فشل البناء (كود الخطأ: $build_status)"
        exit 1
    fi

    echo "✅ اكتمل البناء بنجاح"
    echo ""
}

# ═══════════════════════════════════════════════════════
#  بناء AppImage
# ═══════════════════════════════════════════════════════
build_appimage() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📦 بناء AppImage..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    local appdir="$BUILD_DIR/AppDir"
    rm -rf "$appdir"

    # تثبيت إلى AppDir
    DESTDIR="$appdir" cmake --install "$BUILD_DIR"

    # التأكد من نسخ ملفات الترجمة (مهم لعمل الترجمة في AppImage)
    local qm_dir="$BUILD_DIR/i18n"
    local trans_dest="$appdir/usr/share/gt-stacer/translations"
    mkdir -p "$trans_dest"
    if [ -d "$qm_dir" ]; then
        cp "$qm_dir"/*.qm "$trans_dest/" 2>/dev/null || true
        echo "   ✅ الترجمات نُسخت: $(ls "$trans_dest"/*.qm 2>/dev/null | wc -l) ملف"
    fi

    # إنشاء مجلد translations لـ linuxdeploy-plugin-qt
    mkdir -p "$appdir/usr/translations"
    # نسخ QM إلى usr/translations أيضاً (مطلوب لـ linuxdeploy)
    cp "$trans_dest"/*.qm "$appdir/usr/translations/" 2>/dev/null || true

    # نسخ ملفات AppImage المطلوبة
    cp "$appdir/usr/share/applications/gt-stacer.desktop" "$appdir/"
    cp "$appdir/usr/share/icons/hicolor/256x256/apps/gt-stacer.png" "$appdir/"

    # بناء AppImage
    local output="$RELEASE_DIR/${APP_DISPLAY}-${VERSION}-${ARCH}.AppImage"

    # Force the Wayland platform plugin in too — without it the AppImage
    # cannot start on a Wayland session (which is now the default on GNOME 45+
    # and KDE Plasma 6). linuxdeploy-plugin-qt picks it up via this env var.
    APPIMAGE_EXTRACT_AND_RUN=1 \
    QMAKE="$QT6_QMAKE" \
    EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so" \
    "$LINUXDEPLOY" \
        --appdir "$appdir" \
        --plugin qt \
        --output appimage \
        2>&1

    # نقل المخرج
    local found
    found="$(find "$ROOT_DIR" -maxdepth 1 -name "*.AppImage" 2>/dev/null | head -1)"
    if [ -n "$found" ]; then
        mv "$found" "$output"
        echo "✅ AppImage: $(basename "$output")"
        BUILT+=("AppImage")
        return 0
    else
        echo "⚠️  لم يُعثر على ملف AppImage"
        FAILED+=("AppImage")
        return 1
    fi
}

# ═══════════════════════════════════════════════════════
#  بناء DEB
# ═══════════════════════════════════════════════════════
build_deb() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📦 بناء حزمة DEB..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    local pkgdir="$BUILD_DIR/deb-pkg"
    rm -rf "$pkgdir"

    # تثبيت إلى مجلد الحزمة
    DESTDIR="$pkgdir" cmake --install "$BUILD_DIR"

    # بيانات DEBIAN
    mkdir -p "$pkgdir/DEBIAN"
    local installed_size
    installed_size="$(du -sk "$pkgdir/usr" 2>/dev/null | cut -f1)"

    # تحديد الاعتماديات حسب التوزيعة
    # Qt6 — نستخدم OR للتوافق مع Debian 12 (libqt6core6) و Debian 13+ (libqt6core6t64)
    local qt_deps="libqt6core6t64 (>= 6.2) | libqt6core6 (>= 6.2), libqt6gui6t64 (>= 6.2) | libqt6gui6 (>= 6.2), libqt6widgets6t64 (>= 6.2) | libqt6widgets6 (>= 6.2), libqt6svg6 (>= 6.2)"

    cat > "$pkgdir/DEBIAN/control" <<EOF
Package: ${APP_NAME}
Version: ${VERSION}
Architecture: $(dpkg --print-architecture 2>/dev/null || echo "$ARCH")
Maintainer: GNUTUX <gnutux.arabic@gmail.com>
Installed-Size: ${installed_size}
Depends: ${qt_deps}
Recommends: flatpak, polkitd | policykit-1 | polkit
Section: utils
Priority: optional
Homepage: https://github.com/SalehGNUTUX/GT-STACER
Description: Linux System Optimizer and Monitor (GNUTUX)
 GT-STACER is a modern Linux system optimizer and monitoring tool.
 Developed by GNUTUX. Based on Stacer. Licensed under GPL v3.
 .
 Features: CPU/GPU/Memory/Disk/Temp monitoring, Process manager,
 systemd Services, Startup apps, System cleaner, Package manager
 (APT + Flatpak + Snap), APT sources, Wayland support.
EOF

    cat > "$pkgdir/DEBIAN/postinst" <<'SCRIPT'
#!/bin/sh
set -e
command -v update-icon-caches >/dev/null 2>&1 && update-icon-caches /usr/share/icons/hicolor || true
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database /usr/share/applications || true
SCRIPT
    chmod 755 "$pkgdir/DEBIAN/postinst"

    cat > "$pkgdir/DEBIAN/postrm" <<'SCRIPT'
#!/bin/sh
set -e
command -v update-icon-caches >/dev/null 2>&1 && update-icon-caches /usr/share/icons/hicolor || true
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database /usr/share/applications || true
SCRIPT
    chmod 755 "$pkgdir/DEBIAN/postrm"

    local output="$RELEASE_DIR/${APP_DISPLAY}_${VERSION}_$(dpkg --print-architecture 2>/dev/null || echo "$ARCH").deb"

    if dpkg-deb --build --root-owner-group "$pkgdir" "$output" 2>&1; then
        echo "✅ DEB: $(basename "$output")"
        BUILT+=("DEB")
        DEB_FILE="$output"
        return 0
    else
        echo "⚠️  فشل بناء DEB"
        FAILED+=("DEB")
        return 1
    fi
}

# ═══════════════════════════════════════════════════════
#  بناء RPM (rpmbuild أصلي ← alien+rpmbuild ← تعليمات)
# ═══════════════════════════════════════════════════════
DEB_FILE=""

build_rpm() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📦 بناء حزمة RPM..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # ١. محاولة rpmbuild الأصلي
    if command -v rpmbuild &>/dev/null; then
        echo "   → محاولة rpmbuild الأصلي..."
        if _build_rpm_native; then
            return 0
        fi
        echo "   ⚠ فشل rpmbuild الأصلي — محاولة alien..."
    fi

    # ٢. alien (DEB → RPM)
    if command -v alien &>/dev/null && command -v rpmbuild &>/dev/null; then
        echo "   → محاولة alien (DEB→RPM)..."
        if _build_rpm_alien; then
            return 0
        fi
    fi

    # ٣. تعليمات للمستخدم
    echo ""
    echo "❌ تعذّر بناء RPM. الحلول:"
    echo ""
    if [ "$DISTRO" = "debian" ]; then
        echo "   الحل الأسهل على Debian/Ubuntu:"
        echo "   sudo apt install alien rpm"
        echo "   ثم أعد التشغيل: $0 rpm"
    else
        echo "   على Fedora/RHEL: sudo dnf install rpm-build"
        echo "   ثم أعد التشغيل: $0 rpm"
    fi
    FAILED+=("RPM")
    return 1
}

_build_rpm_native() {
    local rpmbase="$BUILD_DIR/rpmbuild"
    mkdir -p "$rpmbase"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

    # إنشاء tarball للمصادر
    local tarname="${APP_NAME}-${VERSION}"
    local tmpdir
    tmpdir="$(mktemp -d)"
    mkdir -p "$tmpdir/$tarname"
    # نسخ الملفات المصدرية (بدون مجلدات البناء)
    rsync -a --exclude='.build-tools' --exclude='build/' --exclude='build-*' \
          --exclude='release/' --exclude='.git/' \
          "$ROOT_DIR/" "$tmpdir/$tarname/" 2>/dev/null || \
    cp -r "$ROOT_DIR"/* "$tmpdir/$tarname/" 2>/dev/null || true

    tar -czf "$rpmbase/SOURCES/${tarname}.tar.gz" -C "$tmpdir" "$tarname"
    rm -rf "$tmpdir"

    # spec file
    cat > "$rpmbase/SPECS/${APP_NAME}.spec" <<SPEC
Name:           ${APP_NAME}
Version:        ${VERSION}
Release:        1%{?dist}
Summary:        Linux System Optimizer and Monitor by GNUTUX
License:        GPLv3+
URL:            https://github.com/SalehGNUTUX/GT-STACER
Source0:        ${APP_NAME}-${VERSION}.tar.gz

BuildRequires:  cmake >= 3.24
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.2
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  qt6-qttools-devel

Requires:       qt6-qtbase >= 6.2
Requires:       polkit
Recommends:     flatpak

%description
GT-STACER is a modern Linux system optimizer and monitoring tool by GNUTUX.
Licensed under the GNU General Public License v3 or later.

%prep
%autosetup

%build
cmake -B _build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build _build --parallel \$(nproc)

%install
DESTDIR=%{buildroot} cmake --install _build

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
* $(date "+%a %b %d %Y") GNUTUX <gnutux.arabic@gmail.com> - ${VERSION}-1
- إصدار GT-STACER الأول — Qt6, C++17, GPU, Flatpak, Wayland
SPEC

    if rpmbuild \
        --define "_topdir $rpmbase" \
        -bb "$rpmbase/SPECS/${APP_NAME}.spec" 2>&1; then
        local rpm_file
        rpm_file="$(find "$rpmbase/RPMS" -name "*.rpm" | head -1)"
        if [ -n "$rpm_file" ]; then
            local output="$RELEASE_DIR/$(basename "$rpm_file")"
            cp "$rpm_file" "$output"
            echo "✅ RPM: $(basename "$output")"
            BUILT+=("RPM")
            return 0
        fi
    fi
    return 1
}

_build_rpm_alien() {
    # تأكد من وجود DEB
    local deb_file="$DEB_FILE"
    if [ -z "$deb_file" ] || [ ! -f "$deb_file" ]; then
        deb_file="$(ls "$RELEASE_DIR"/*.deb 2>/dev/null | head -1)"
    fi
    if [ -z "$deb_file" ]; then
        echo "   ⚠ لا يوجد ملف DEB — جارٍ بناء DEB أولاً..."
        build_deb || return 1
        deb_file="$DEB_FILE"
    fi

    local tmpdir
    tmpdir="$(mktemp -d)"
    cp "$deb_file" "$tmpdir/"

    if ! (cd "$tmpdir" && fakeroot alien -r -g "$(basename "$deb_file")") 2>&1; then
        rm -rf "$tmpdir"
        return 1
    fi

    local spec
    spec="$(find "$tmpdir" -name "*.spec" | head -1)"
    if [ -z "$spec" ]; then
        rm -rf "$tmpdir"
        return 1
    fi

    # إصلاح حقل Summary
    sed -i 's/^Summary:[[:space:]]*$/Summary: Linux System Optimizer and Monitor by GNUTUX/' "$spec"

    local pkgdir
    pkgdir="$(dirname "$spec")"

    if (cd "$pkgdir" && fakeroot rpmbuild \
        --buildroot="$pkgdir" \
        -bb --target "$ARCH" \
        "$(basename "$spec")") 2>&1; then
        local rpm_file
        rpm_file="$(find "$tmpdir" -name "*.rpm" | head -1)"
        if [ -n "$rpm_file" ]; then
            local output="$RELEASE_DIR/$(basename "$rpm_file")"
            cp "$rpm_file" "$output"
            rm -rf "$tmpdir"
            echo "✅ RPM (alien): $(basename "$output")"
            BUILT+=("RPM (alien)")
            return 0
        fi
    fi
    rm -rf "$tmpdir"
    return 1
}

# ═══════════════════════════════════════════════════════
#  المتتبع والتقرير النهائي
# ═══════════════════════════════════════════════════════
BUILT=()
FAILED=()

print_report() {
    echo ""
    echo "══════════════════════════════════════════════════════════"
    [ ${#BUILT[@]}  -gt 0 ] && echo "   ✅ نجح:  ${BUILT[*]}"
    [ ${#FAILED[@]} -gt 0 ] && echo "   ❌ فشل:  ${FAILED[*]}"
    echo "══════════════════════════════════════════════════════════"
    echo ""
    echo "الحزم الجاهزة في release/:"
    ls -lh "$RELEASE_DIR"/*.AppImage "$RELEASE_DIR"/*.deb "$RELEASE_DIR"/*.rpm 2>/dev/null \
        | awk '{print "   " $5 "  " $NF}' \
        || echo "   (لا توجد مخرجات)"
    echo ""
    echo "💡 تثبيت DEB:      sudo dpkg -i release/*.deb"
    echo "💡 تثبيت RPM:      sudo rpm -i release/*.rpm"
    echo "💡 تشغيل AppImage: chmod +x release/*.AppImage && ./release/*.AppImage"
    echo ""
    echo "🌐 المشروع: https://github.com/SalehGNUTUX/GT-STACER"
    echo ""
    [ ${#BUILT[@]} -eq 0 ] && exit 1 || exit 0
}

# ═══════════════════════════════════════════════════════
#  نقطة الدخول — اختيار الهدف
# ═══════════════════════════════════════════════════════
TARGET="${1:-all}"

case "$TARGET" in
install-deps)
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "ℹ️  تأكّد من تشغيل هذا الأمر في ترمنال تفاعلي"
    echo "   حتى تعمل صلاحيات sudo بشكل صحيح."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    install_deps
    ;;
build)
    install_deps
    check_requirements
    cmake_build
    echo "✅ البناء جاهز في: $BUILD_DIR"
    ;;
appimage)
    install_deps
    check_requirements
    download_linuxdeploy
    cmake_build
    build_appimage
    print_report
    ;;
deb)
    install_deps
    check_requirements
    cmake_build
    build_deb
    print_report
    ;;
rpm)
    install_deps
    check_requirements
    cmake_build
    build_rpm
    print_report
    ;;
all)
    install_deps
    check_requirements
    download_linuxdeploy
    cmake_build
    build_appimage
    build_deb
    build_rpm
    print_report
    ;;
*)
    echo "الاستخدام: $0 [all|appimage|deb|rpm|build|install-deps]"
    echo ""
    echo "  all          — بناء جميع الحزم (AppImage + DEB + RPM)"
    echo "  appimage     — بناء AppImage فقط"
    echo "  deb          — بناء حزمة DEB فقط"
    echo "  rpm          — بناء حزمة RPM فقط"
    echo "  build        — تهيئة وبناء فقط (بدون تحزيم)"
    echo "  install-deps — تثبيت المتطلبات فقط"
    exit 1
    ;;
esac
