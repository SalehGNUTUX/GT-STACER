Name:           gt-stacer
Version:        26.04
Release:        1%{?dist}
Summary:        Linux System Optimizer and Monitor by GNUTUX
License:        GPLv3+
URL:            https://github.com/SalehGNUTUX/GT-STACER
Source0:        gt-stacer-%{version}.tar.gz

BuildRequires:  cmake >= 3.24
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.2
BuildRequires:  qt6-qtcharts-devel
BuildRequires:  qt6-qtsvg-devel

Requires:       qt6-qtbase >= 6.2
Requires:       qt6-qtcharts
Requires:       qt6-qtsvg
Requires:       systemd
Requires:       polkit
Recommends:     flatpak

%description
GT-STACER is a modern Linux system optimizer and monitoring tool by GNUTUX.
A modernized fork of Stacer rebuilt with Qt6, C++17 for GNU/Linux 2026.

Features: Dashboard, CPU/GPU/Memory monitoring, temperature sensors,
battery information, process manager, systemd service management,
startup apps, system cleaner, package manager (APT+Flatpak+Snap),
APT repository manager, Wayland support, dark/light themes.

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
* Sat Apr 26 2026 GNUTUX <gnutux.arabic@gmail.com> - 26.04-1
- Initial release: GT-STACER modernized fork of Stacer
- Qt6 + C++17, GPU monitoring, Flatpak, Temperature, Wayland
