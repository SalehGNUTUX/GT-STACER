<div align="center">

<img src="gt-stacer/static/icons/gt-stacer.png" width="140" alt="GT-STACER Logo"/>

# GT-STACER

### Linux System Optimizer and Monitor
**محسّن ومراقب نظام لينكس**

[![Version](https://img.shields.io/badge/version-26.04-blue?style=flat-square)](https://github.com/SalehGNUTUX/GT-STACER/releases)
[![License](https://img.shields.io/badge/license-GPL%20v3-green?style=flat-square)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6.2+-41CD52?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io)
[![C++](https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![Platform](https://img.shields.io/badge/platform-GNU%2FLinux-FCC624?style=flat-square&logo=linux&logoColor=black)](https://kernel.org)
[![Developer](https://img.shields.io/badge/developer-GNUTUX-9b59b6?style=flat-square)](https://salehgnutux.github.io/GT-STACER)

[🌐 الموقع الرسمي](https://salehgnutux.github.io/GT-STACER) · [English](#english) · [العربية](#arabic)

</div>

---

<a name="arabic"></a>

## العربية

### نبذة عن المشروع

GT-STACER هو انشقاق (fork) حديث ومطوَّر من مشروع [Stacer](https://github.com/oguzhaninan/Stacer) الشهير بقلم Oguzhan INAN، أُعيد بناؤه بالكامل باستخدام **Qt6** و**C++17** ليتوافق مع بيئة غنو/لينكس لعام 2026. يوفر واجهة رسومية موحّدة وعصرية لمراقبة النظام وإدارته مع دعم شامل لجميع التوزيعات ومدراء الحزم.

> 🙏 **الإسناد:** مستوحى من [Stacer](https://github.com/oguzhaninan/Stacer) — شكراً لـ Oguzhan INAN  
> 👨‍💻 **التطوير:** GNUTUX · 📄 **الرخصة:** GNU General Public License v3

---

### ✨ ما الجديد في GT-STACER مقارنةً بالأصل

| الجانب | Stacer 1.1.0 (2019) | GT-STACER 26.04 (2026) |
|---|---|---|
| إطار العمل | Qt5 (EOL) | **Qt6 ≥ 6.2** |
| معيار C++ | C++11 | **C++17** |
| لوحة التحكم | تقدّم خطي | **مقاييس دائرية متحركة** مع نص فرعي واضح |
| الشريط الجانبي | نص فقط | **أيقونات SVG + طيّ (220↔64 px)** مع إبقاء الشعار |
| GPU | ✗ | ✅ Intel · AMD · NVIDIA |
| درجات الحرارة | ✗ | ✅ hwmon · thermal zones |
| البطارية | ✗ | ✅ مقياس دائري + وقت متبقٍّ |
| سرعة الشبكة | ✅ | ✅ حساب فرق بين قراءتين حقيقيتين |
| Flatpak | ✗ | ✅ |
| مدراء الحزم | APT · Snap | **28+ مدير** |
| أنظمة init | systemd فقط | systemd · OpenRC · runit · s6 · SysV |
| مؤشرات التحميل | ✗ | ✅ Loading overlay متحرك على كل عملية |
| شريط المهام | ✗ | ✅ CPU% وRAM% في الـ Tooltip لحظياً |
| شاشة الترحيب | ✗ | ✅ 6 شرائح تعريفية عند أول تشغيل |
| Wayland | جزئي | ✅ كامل |
| العربية | ✅ | ✅ ar_MA · RTL · أرقام غربية 🇲🇦 |
| سلوك الإغلاق | إنهاء | ✅ قابل للضبط (تصغير للشريط / إنهاء) |
| الثيم | QSS بسيط | **Catppuccin** dark/light |

---

### المزايا التفصيلية

#### 📊 لوحة التحكم (Dashboard)
- **مقاييس دائرية متحركة** للمعالج · الذاكرة · القرص · Swap — تتغير ألوانها (أخضر→أصفر→برتقالي→أحمر) حسب الاستخدام
- النص الفرعي (مثل `4.6 GB / 7.65 GB`) يظهر **أسفل اسم المقياس** خارج الدائرة — لا تداخل
- بطاقة معلومات النظام: اسم الجهاز · التوزيعة · النواة · نظام التهيئة · الـ Uptime بعداد حي
- بطاقة الشبكة: الواجهة الفعلية · IPv4 · **سرعة الاستقبال والإرسال لحظياً**
- بطاقة GPU تظهر تلقائياً (Intel/AMD/NVIDIA) · بطاقة البطارية للأجهزة المحمولة

#### 🖥️ الواجهة وتجربة المستخدم
- **شريط جانبي ذكي:** أيقونات SVG لكل قسم + زر طيّ يُظهر الشعار الصغير في وضع الطيّ
- **شاشة ترحيب:** 6 شرائح تعريفية عند أول تشغيل فقط
- **مؤشرات تحميل:** Spinner متحرك على كل عملية تأخذ وقتاً (فحص · تنظيف · تحميل حزم)
- **شريط المهام:** يعرض `CPU: X% | RAM: Y%` في الـ Tooltip ويُحدَّث كل 3 ثوانٍ
- **سلوك الإغلاق:** افتراضي = تصغير للشريط، قابل للتغيير من الإعدادات

#### 🔧 إدارة النظام
- **العمليات:** ترتيب · بحث · إنهاء مع تأكيد
- **الخدمات:** start/stop/enable/disable لـ systemd · OpenRC · runit · s6 · SysV
- **System Cleaner:** فحص في خلفية منفصلة مع spinner · أعمدة قابلة للتوسيع · ألوان حسب الحجم
- **مصادر APT:** نقر مزدوج يفتح نافذة تعديل كاملة · Tooltip يعرض URL كامل

#### 📦 دعم مدراء الحزم (28+ مدير)

| الفئة | المدراء |
|---|---|
| Debian / Ubuntu | APT |
| Fedora / RHEL | DNF · DNF5 · YUM · TDNF |
| Arch Linux | Pacman · Yay · Paru (AUR) |
| openSUSE / SUSE | Zypper |
| Void Linux | XBPS |
| Alpine Linux | APK |
| Gentoo / Funtoo | Portage (emerge) |
| NixOS | Nix |
| Fedora Silverblue / Kinoite | rpm-ostree |
| Slackware | Pkgtool · Slackpkg · slapt-get |
| Solus | Eopkg |
| Clear Linux | swupd |
| GNU Guix | Guix |
| Vanilla OS | Apx |
| عالمي | Flatpak · Snap · AppImage · Homebrew |
| بيئات اللغات | Conda · Mamba · pip3 · Cargo · npm |

#### 🌐 دعم اللغات (19 لغة)

| اللغة | الحالة | العلم |
|---|---|---|
| العربية (المغرب) | ✅ مكتملة | 🇲🇦 |
| English | ✅ افتراضية | 🇬🇧 |
| Deutsch | 🔄 قابلة للمساهمة | 🇩🇪 |
| Français | 🔄 قابلة للمساهمة | 🇫🇷 |
| हिन्दी | 🔄 قابلة للمساهمة | 🇮🇳 |
| Italiano | 🔄 قابلة للمساهمة | 🇮🇹 |
| Nederlands | 🔄 قابلة للمساهمة | 🇳🇱 |
| Polski | 🔄 قابلة للمساهمة | 🇵🇱 |
| Português | 🔄 قابلة للمساهمة | 🇧🇷 |
| Русский | 🔄 قابلة للمساهمة | 🇷🇺 |
| Svenska | 🔄 قابلة للمساهمة | 🇸🇪 |
| Türkçe | 🔄 قابلة للمساهمة | 🇹🇷 |
| Українська | 🔄 قابلة للمساهمة | 🇺🇦 |
| Tiếng Việt | 🔄 قابلة للمساهمة | 🇻🇳 |
| 简体中文 | 🔄 قابلة للمساهمة | 🇨🇳 |
| 繁體中文 | 🔄 قابلة للمساهمة | 🇹🇼 |
| ಕನ್ನಡ / മലയാളം / Occitan | 🔄 قابلة للمساهمة | — |

> 🌍 لإضافة ترجمة: عدّل `translations/gt-stacer_XX.ts` وأرسل Pull Request.

---

### التثبيت

#### ⚡ AppImage — لا تثبيت مطلوب
```bash
chmod +x GT-STACER-26.04-x86_64.AppImage
./GT-STACER-26.04-x86_64.AppImage
```

#### Debian / Ubuntu / Linux Mint / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.04_amd64.deb
sudo apt-get install -f          # إصلاح الاعتماديات إن وُجد نقص
```

> **ملاحظة Debian 13 (Trixie):** الحزمة تستخدم `Recommends` للـ polkit لتتوافق مع `polkitd`. الأوامر التي تحتاج صلاحيات (مثل إدارة الخدمات) تتطلب تثبيت أحد: `polkitd` أو `policykit-1`.

#### Fedora / RHEL / AlmaLinux / Rocky
```bash
sudo dnf install gt-stacer-26.04-1.x86_64.rpm
```

#### Arch Linux / Manjaro
```bash
# قريباً عبر AUR
yay -S gt-stacer
```

---

### البناء من المصدر

#### المتطلبات

| الأداة | الإصدار الأدنى |
|---|---|
| CMake | 3.24 |
| g++ أو clang++ | C++17 |
| Qt6 | 6.2+ (Base · Charts · Svg · Network · Concurrent) |
| ninja أو make | أي إصدار |
| lrelease (qt6-l10n-tools) | للترجمات (اختياري) |

#### بناء تلقائي كامل
```bash
git clone https://github.com/SalehGNUTUX/GT-STACER.git
cd GT-STACER
./scripts/build-all.sh all       # يثبّت الاعتماديات + AppImage + DEB + RPM
```

#### أوامر منفردة
```bash
./scripts/build-all.sh install-deps   # تثبيت الاعتماديات فقط
./scripts/build-all.sh build          # بناء فقط (بدون تحزيم)
./scripts/build-all.sh deb
./scripts/build-all.sh rpm
./scripts/build-all.sh appimage
```

#### يدوي

**Debian/Ubuntu:**
```bash
sudo apt install build-essential cmake qt6-base-dev qt6-charts-dev \
                 qt6-svg-dev qt6-tools-dev qt6-l10n-tools
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
./build/gt-stacer/gt-stacer
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtcharts-devel \
                 qt6-qtsvg-devel qt6-qttools-devel
```

**Arch:**
```bash
sudo pacman -S base-devel cmake qt6-base qt6-charts qt6-svg qt6-tools
```

---

### بنية المشروع

```
GT-STACER/
├── gt-stacer-core/          # مكتبة C++ أساسية (static) — بدون UI
│   ├── Info/                # قراءة /proc و /sys
│   │                        # cpu · memory · disk · network* · gpu · temperature · battery · process · system
│   ├── Tools/               # تعديل النظام
│   │                        # service (multi-init) · package (28+) · startup · apt_source
│   └── Utils/               # command_util · file_util · format_util
│
├── gt-stacer/               # تطبيق Qt6 Widgets
│   ├── main.cpp             # setQuitOnLastWindowClosed(false) + WelcomeDialog
│   ├── app.h/cpp            # QMainWindow + إعادة الترجمة عند الإقلاع
│   ├── Managers/            # AppManager · SettingManager · InfoManager · ToolManager
│   ├── Widgets/             # CircularGauge · LoadingOverlay · Sidebar · sidebar_icons.h
│   ├── Dialogs/             # WelcomeDialog (onboarding) · EditSourceDialog (APT)
│   ├── Pages/               # Dashboard (programmatic) · Resources · Processes · Services
│   │                        # StartupApps · SystemCleaner · Uninstaller
│   │                        # AptSourceManager · Settings · Helpers
│   └── static/
│       ├── icons/gt-stacer.png
│       └── themes/dark/style.qss · light/style.qss
│
├── translations/            # Qt Linguist (.ts) — ar مكتملة · 18 أخرى stubs
├── scripts/build-all.sh     # بناء تلقائي (Debian/Fedora/Arch/SUSE)
├── packaging/               # debian/control · rpm/*.spec · build-*.sh
├── applications/gt-stacer.desktop
├── icons/hicolor/           # 16·32·48·64·128·256 px
├── CMakeLists.txt
├── CLAUDE.md                # إرشادات للمطورين
└── LICENSE                  # GNU GPL v3
```

> *`NetworkInfo` تحسب السرعة من فرق `rx_bytes/tx_bytes` بين استدعاءين. الاستدعاء الأول يُرجع speed=0.

---

### التقنيات

| المكوّن | التفاصيل |
|---|---|
| اللغة | C++17 |
| إطار العمل | Qt 6.2+ — Widgets · Charts · Svg · Network · Concurrent |
| نظام البناء | CMake 3.24+ مع GNUInstallDirs |
| الترجمات | Qt Linguist: `.ts` → `.qm` via `lrelease` |
| الحوكمة | `pkexec` (polkit) لجميع العمليات المميزة |
| الأيقونات | FreeDesktop HiColor + SVG مدمجة في الشريط الجانبي |
| الرخصة | GNU GPL v3 |

---

### المساهمة

- **الترجمات** — افتح `translations/gt-stacer_XX.ts` وأكمله، ثم أرسل Pull Request
- **دعم توزيعات جديدة** — أضف كشف المدير في `gt-stacer-core/Tools/package_tool.cpp`
- **تقارير الأخطاء** — افتح Issue مع اسم التوزيعة ونسخة Qt

#### إضافة ترجمة
```bash
# تحديث .ts بالنصوص الجديدة
/usr/lib/qt6/bin/lupdate gt-stacer/ -ts translations/gt-stacer_XX.ts
# اختبار البناء
./scripts/build-all.sh build
```

---

### الترخيص

```
GT-STACER — Linux System Optimizer and Monitor
Copyright (C) 2026 GNUTUX <gnutux.arabic@gmail.com>

Based on Stacer by Oguzhan INAN
Copyright (C) 2017-2021 Oguzhan INAN <https://github.com/oguzhaninan/Stacer>

GNU General Public License v3 or later.
```

[LICENSE](LICENSE) · [gnu.org/licenses/gpl-3.0](https://www.gnu.org/licenses/gpl-3.0.html)

---

<a name="english"></a>

## English

### About

GT-STACER is a modernized fork of [Stacer](https://github.com/oguzhaninan/Stacer) by Oguzhan INAN, rebuilt with **Qt6** and **C++17** for GNU/Linux 2026. It provides a modern graphical interface for system monitoring and management across all major Linux distributions.

> 🙏 **Attribution:** Inspired by [Stacer](https://github.com/oguzhaninan/Stacer) — thank you Oguzhan INAN.  
> 🌐 [salehgnutux.github.io/GT-STACER](https://salehgnutux.github.io/GT-STACER)

### Key Features

- **Animated circular gauges** for CPU, RAM, Disk, Swap — sub-text below the gauge, never inside
- **Real-time network speed** (RX/TX computed from byte-delta between samples)
- **GPU monitoring** — Intel/AMD/NVIDIA auto-detected
- **Temperature sensors** — hwmon + thermal zones
- **Battery** — charge gauge + estimated time remaining
- **28+ package managers** auto-detected (APT, DNF, Pacman, Zypper, Flatpak, Snap, XBPS, APK, Portage, Nix, …)
- **Service manager** — systemd · OpenRC · runit · s6 · SysV
- **System Cleaner** — async scan with spinner, color-coded sizes
- **APT Source Manager** — double-click to edit, full-URL tooltips
- **Collapsible sidebar** — SVG icons, logo stays visible when collapsed
- **System tray** — CPU%/RAM% tooltip updated every 3 s
- **19 languages** — Arabic (complete, RTL, 🇲🇦), others ready for contributors
- **Configurable close** — minimize to tray (default) or quit

### Quick Start

```bash
git clone https://github.com/SalehGNUTUX/GT-STACER.git
cd GT-STACER
./scripts/build-all.sh all
```

### Dependencies

| | Debian/Ubuntu | Fedora | Arch |
|---|---|---|---|
| Build | `build-essential cmake` | `gcc-c++ cmake` | `base-devel cmake` |
| Qt6 | `qt6-base-dev qt6-charts-dev qt6-svg-dev qt6-tools-dev` | `qt6-qtbase-devel qt6-qtcharts-devel qt6-qtsvg-devel` | `qt6-base qt6-charts qt6-svg qt6-tools` |
| Translations | `qt6-l10n-tools` | `qt6-linguist` | `qt6-tools` |

Minimum: **CMake 3.24 · C++17 · Qt 6.2**

---

<div align="center">

Made with ❤️ by **[GNUTUX](https://github.com/SalehGNUTUX)** · [Website](https://salehgnutux.github.io/GT-STACER) · [Repository](https://github.com/SalehGNUTUX/GT-STACER) · GPL v3

<sub>Inspired by <a href="https://github.com/oguzhaninan/Stacer">Stacer</a> — the original Linux optimizer by Oguzhan INAN</sub>

</div>
