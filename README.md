<div align="center">

<img src="https://github.com/SalehGNUTUX/GT-STACER/blob/main/GT-STACER.png" width="140" alt="GT-STACER Logo"/>

# GT-STACER

### Linux System Optimizer and Monitor
**محسّن ومراقب نظام لينكس**

[![Version](https://img.shields.io/badge/version-26.04--alpha-blueviolet?style=flat-square)](https://github.com/SalehGNUTUX/GT-STACER/releases)
[![Status](https://img.shields.io/badge/status-alpha-orange?style=flat-square)](#-المرحلة-الحالية)
[![License](https://img.shields.io/badge/license-GPL%20v3-green?style=flat-square)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6.8-41CD52?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io)
[![C++](https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![Platform](https://img.shields.io/badge/platform-GNU%2FLinux-FCC624?style=flat-square&logo=linux&logoColor=black)](https://kernel.org)
[![Developer](https://img.shields.io/badge/developer-GNUTUX-9b59b6?style=flat-square)](https://salehgnutux.github.io/GT-STACER)

[🌐 الموقع الرسمي](https://salehgnutux.github.io/GT-STACER) · [English](#english) · [العربية](#arabic)

</div>

---

<a name="arabic"></a>

## العربية

### 📢 المرحلة الحالية

> **الإصدار الحالي: GT-STACER 26.04 ALPHA**
>
> 🧪 **طور ألفا التجريبي** — البرنامج مستقر وقابل للاستخدام اليومي، لكنه لا يزال في مراحله التجريبية الأولى.
>
> ✅ **يعمل بشكل مناسب** على معظم التوزيعات • ⚠️ قد تحتوي بعض المزايا على أخطاء طفيفة • 📋 نرحب بتقاريركم

---

### نبذة عن المشروع

GT-STACER هو انشقاق (fork) حديث ومطوَّر من مشروع [Stacer](https://github.com/oguzhaninan/Stacer) الشهير، أُعيد بناؤه بالكامل باستخدام **Qt6** و**C++17** ليتوافق مع بيئة غنو/لينكس لعام 2026. يوفر واجهة رسومية موحّدة وعصرية لمراقبة النظام وإدارته مع دعم شامل لجميع التوزيعات ومدراء الحزم.

> 🙏 **الإسناد:** مستوحى من [Stacer](https://github.com/oguzhaninan/Stacer) — شكراً لـ Oguzhan INAN  
> 👨‍💻 **التطوير:** GNUTUX · 📄 **الرخصة:** GNU General Public License v3  
> 🌐 **الموقع:** [salehgnutux.github.io/GT-STACER](https://salehgnutux.github.io/GT-STACER)

---

### ✅ ما تم إنجازه حتى الآن (الإصدار 26.04 ALPHA)

#### 🖥️ الواجهة وتجربة المستخدم
- [x] **مقاييس دائرية متحركة** في لوحة التحكم (CPU · RAM · Disk · Swap) مع ألوان متغيرة حسب الاستخدام
- [x] **شريط جانبي ذكي** مع أيقونات SVG قابلة للطيّ (220↔64 px)
- [x] **شاشة ترحيب** من 6 شرائح تعريفية عند أول تشغيل
- [x] **مؤشرات تحميل متحركة** (spinner) على العمليات المطولة
- [x] **شريط مهام** يعرض CPU% و RAM% ويُحدَّث كل 3 ثوانٍ
- [x] **سلوك إغلاق قابل للتخصيص** (تصغير للشريط / إنهاء كامل)
- [x] **ثيمات Catppuccin** (داكن/فاتح) مع أيقونات SVG مدمجة

#### 🔧 إدارة النظام
- [x] **مراقبة العمليات** مع ترتيب وبحث وإنهاء مع تأكيد
- [x] **إدارة الخدمات** لـ 5 أنظمة init (systemd · OpenRC · runit · s6 · SysV)
- [x] **منظف النظام** مع فحص غير متزامن ومؤشر تحميل دوار
- [x] **مدير مصادر APT** مع نافذة تعديل كاملة وTooltip لعرض الـ URL
- [x] **إدارة برامج بدء التشغيل**
- [x] **إلغاء تثبيت الحزم** (Uninstaller)

#### 📦 دعم مدراء الحزم
- [x] **14+ مدير حزم** مدمج: APT · DNF · DNF5 · YUM · Pacman · Yay · Paru · Zypper · XBPS · APK · Portage · Flatpak · Snap · AppImage
- [x] دعم إضافي لـ: Homebrew · pip3 · Cargo · npm · Conda · Mamba
- [x] اكتشاف تلقائي لمدير الحزم المناسب لتوزيعتك

#### 📊 مراقبة النظام
- [x] **GPU** — دعم Intel · AMD · NVIDIA مع كشف تلقائي
- [x] **درجات الحرارة** — مراقبة عبر hwmon والمناطق الحرارية
- [x] **البطارية** — مقياس دائري للشحن + الوقت المتبقي التقديري
- [x] **سرعة الشبكة** — حساب حقيقي بفرق القراءات (RX/TX)
- [x] **قرص Swap** — مراقبة وتفاصيل في لوحة التحكم
- [x] **بطاقة معلومات النظام** — اسم الجهاز · التوزيعة · النواة · نظام init · المدة (uptime)

#### 🌐 اللغات والتدويل
- [x] **العربية (المغرب)** — مكتملة 100% 🇲🇦
- [x] **الإنجليزية** — افتراضية 🇬🇧
- [x] **RTL كامل** مع أرقام غربية
- [x] **18 لغة إضافية** جاهزة للمساهمة (stubs)

#### 🛠️ التقنيات
- [x] Qt 6.8+ مع Widgets · Charts · Svg · Network · Concurrent
- [x] C++17 مع بناء عبر CMake 3.24+
- [x] دعم كامل لـ Wayland
- [x] حزم AppImage · DEB · RPM جاهزة للتنزيل

---

### 🗺️ خارطة الطريق — ما هو قادم

#### 🔜 الإصدار 26.05 BETA (قيد التخطيط)
- [ ] **إحصائيات مُفصَّلة للبطارية** — رسم بياني للاستهلاك وتاريخ الشحن
- [ ] **تنبيهات حرارية** — إشعار عند تجاوز درجة الحرارة حداً معيناً
- [ ] **تحسين أداء المراقبة** — تقليل استهلاك CPU عند العمل في الخلفية
- [ ] **دعم مدراء حزم إضافيين** — Nix · Guix · rpm-ostree · Eopkg · swupd
- [ ] **تحسين RTL** — دعم كامل للغات العربية والفارسية والأردية
- [ ] **اختبارات آلية** — وحدة اختبارات للنواة (gt-stacer-core)

#### 🎯 الإصدار 26.06 STABLE (مستهدف)
- [ ] **رسوم بيانية تاريخية** للموارد (CPU · RAM · شبكة · حرارة)
- [ ] **مُراقب ملفات** — تتبع التغييرات في المجلدات المحددة
- [ ] **مدير شبكة مبسَّط** — عرض الاتصالات النشطة وإحصائيات لكل تطبيق
- [ ] **دعم AppImage مُحسَّن** — تحديثات تلقائية
- [ ] **تكامل مع Flatpak** — إدارة الصلاحيات والبيانات
- [ ] **تحسين إمكانية الوصول** — دعم قارئات الشاشة والتنقل بلوحة المفاتيح

#### 💡 أفكار للإصدارات المستقبلية
- [ ] **مراقب أداء مُتقدِّم** (مثل htop مُدمج في الواجهة)
- [ ] **مُدير أقراص** — SMART واختبارات الأداء
- [ ] **مدير طاقة** — إعدادات متقدمة لتوفير الطاقة
- [ ] **وضع محمول** — تشغيل من USB دون تثبيت مع حفظ الإعدادات
- [ ] **تكامل مع KDE/GNOME** — إضافات للوحة النظام
- [ ] **تطبيق مرافق CLI** (`gt-stacer-cli`) للخوادم بدون واجهة رسومية

---

### 📥 التثبيت

#### ⚡ AppImage — لا تثبيت مطلوب
```bash
chmod +x GT-STACER-26.04-x86_64-ALPHA.AppImage
./GT-STACER-26.04-x86_64-ALPHA.AppImage
```

#### Debian / Ubuntu / Linux Mint / Kali / Trixie+
```bash
sudo dpkg -i GT-STACER_26.04_amd64-ALPHA.deb
sudo apt-get install -f
```

#### Fedora / RHEL / AlmaLinux / Rocky
```bash
sudo dnf install gt-stacer-26.04.x86_64_ALPHA.rpm
```

#### Arch Linux / Manjaro
```bash
# قريباً عبر AUR
yay -S gt-stacer
```

> 📦 **جميع إصدارات التنزيل:** [GitHub Releases](https://github.com/SalehGNUTUX/GT-STACER/releases)
>
> 📂 **بناء من المصدر:** راجع [قسم البناء من المصدر](#-البناء-من-المصدر) أدناه

---

### 🏗️ البناء من المصدر

#### المتطلبات

| الأداة | الإصدار الأدنى |
|---|---|
| CMake | 3.24 |
| g++ أو clang++ | C++17 |
| Qt6 | 6.2+ (Base · Charts · Svg · Network · Concurrent) |
| ninja أو make | أي إصدار |

#### بناء سريع
```bash
git clone https://github.com/SalehGNUTUX/GT-STACER.git
cd GT-STACER
./scripts/build-all.sh all
```

#### بناء يدوي
```bash
# Debian/Ubuntu
sudo apt install build-essential cmake qt6-base-dev qt6-charts-dev qt6-svg-dev qt6-tools-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
./build/gt-stacer/gt-stacer
```

---

### 🤝 المساهمة

المشروع في مرحلة ألفا ويحتاج إلى مساهمين! يمكنك المساعدة في:

- 🐛 **تقارير الأخطاء** — افتح Issue مع وصف دقيق للمشكلة (التوزيعة، إصدار Qt، الخطوات)
- 🌍 **الترجمات** — أضف أو أكمل ملف `translations/gt-stacer_XX.ts`
- 💻 **الكود** — أرسل Pull Request (راجع `CLAUDE.md` لإرشادات التطوير)
- 🎨 **تصميم** — اقترح تحسينات للواجهة أو أيقونات جديدة
- 📖 **توثيق** — حسّن التوثيق أو اكتب أدلة استخدام

> 🌍 **لإضافة لغة جديدة:** افتح `translations/gt-stacer_XX.ts` وعدّل عليه، ثم أرسل Pull Request.

---

### 📋 جدول المنجزات التفصيلي

<details>
<summary><b>🔍 اضغط لعرض الجدول الكامل للمنجزات</b></summary>

| الميزة | الحالة | إصدار | ملاحظات |
|---|---|---|---|
| لوحة تحكم بمقاييس دائرية | ✅ مكتمل | 26.04-A | ألوان متغيرة حسب الاستخدام |
| مراقبة GPU (Intel/AMD/NVIDIA) | ✅ مكتمل | 26.04-A | كشف تلقائي |
| درجات الحرارة | ✅ مكتمل | 26.04-A | hwmon + thermal zones |
| مراقبة البطارية | ✅ مكتمل | 26.04-A | مقياس + وقت متبقٍّ |
| سرعة الشبكة | ✅ مكتمل | 26.04-A | حساب دلتا بين قراءتين |
| شريط جانبي مع SVG | ✅ مكتمل | 26.04-A | طيّ + أيقونات |
| شاشة ترحيب | ✅ مكتمل | 26.04-A | 6 شرائح عند التشغيل الأول |
| شريط مهام | ✅ مكتمل | 26.04-A | CPU% · RAM% مباشر |
| سلوك إغلاق قابل للتخصيص | ✅ مكتمل | 26.04-A | تصغير/إنهاء |
| 5 أنظمة init | ✅ مكتمل | 26.04-A | systemd · OpenRC · runit · s6 · SysV |
| 14+ مدير حزم | ✅ مكتمل | 26.04-A | اكتشاف تلقائي |
| منظف النظام | ✅ مكتمل | 26.04-A | فحص غير متزامن + spinner |
| مدير مصادر APT | ✅ مكتمل | 26.04-A | تحرير كامل للمصادر |
| دعم Wayland | ✅ مكتمل | 26.04-A | كامل |
| العربية (RTL) | ✅ مكتمل | 26.04-A | مع أرقام غربية |
| ثيمات Catppuccin | ✅ مكتمل | 26.04-A | داكن/فاتح |
| حزمة AppImage | ✅ مكتمل | 26.04-A | 53 MB |
| حزمة DEB | ✅ مكتمل | 26.04-A | 1.69 MB |
| حزمة RPM | ✅ مكتمل | 26.04-A | 7.86 MB |
| إحصائيات تاريخية | 🔄 قيد التخطيط | 26.06 | رسوم بيانية |
| تنبيهات حرارية | 🔄 قيد التخطيط | 26.05 | إشعارات |
| اختبارات آلية | 🔄 قيد التخطيط | 26.05 | CI/CD |
| تطبيق CLI مرافق | 💡 فكرة مستقبلية | — | للخوادم |

</details>

---

### 🌐 الموقع الرسمي

تم إطلاق الموقع الرسمي للمشروع مع:

- 🌍 **دعم كامل للعربية والإنجليزية** مع تبديل فوري
- 📸 **معرض لقطات شاشة تفاعلي** داخل إطار حاسوب محمول
- 🎨 **وضعان: داكن وفاتح** مع حفظ التفضيلات تلقائياً
- 📥 **قسم تنزيل** مع تجزئة SHA256 للتحقق
- 📊 **جدول مقارنة** مع Stacer الأصلي
- 📱 **تصميم متجاوب** لجميع الأجهزة

🔗 **[salehgnutux.github.io/GT-STACER](https://salehgnutux.github.io/GT-STACER)**

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

### 📢 Current Phase

> **Current Release: GT-STACER 26.04 ALPHA**
>
> 🧪 **Alpha Phase** — The application is stable and usable for daily tasks, though still in early experimental stages.
>
> ✅ **Works properly** on most distributions • ⚠️ Some features may have minor bugs • 📋 Bug reports welcome

---

### ✅ Completed Milestones (26.04 ALPHA)

- 🖥️ **Animated circular gauges** for CPU, RAM, Disk & Swap
- 🎨 **Catppuccin dark/light themes** with SVG icons
- 🔋 **Battery & GPU monitoring** (Intel/AMD/NVIDIA)
- 🌡️ **Temperature sensors** via hwmon
- 📦 **14+ package managers** auto-detected
- ⚙️ **5 init systems** (systemd, OpenRC, runit, s6, SysV)
- 🧹 **System cleaner** with async scanning
- 🌐 **Bilingual Arabic/English** (full RTL, ar_MA)
- 🪟 **Wayland** full support
- 📥 **AppImage, DEB, RPM** packages

---

### 🗺️ Roadmap

- **v26.05 BETA** — Thermal alerts, more package managers, performance optimizations
- **v26.06 STABLE** — Historical charts, network manager, Flatpak integration
- **Future Ideas** — CLI companion, advanced power management, disk manager

---

### Quick Start

```bash
git clone https://github.com/SalehGNUTUX/GT-STACER.git
cd GT-STACER
./scripts/build-all.sh all
```

---

<div align="center">

Made with ❤️ by **[GNUTUX](https://github.com/SalehGNUTUX)** · [Website](https://salehgnutux.github.io/GT-STACER) · [Repository](https://github.com/SalehGNUTUX/GT-STACER) · GPL v3

<sub>Inspired by <a href="https://github.com/oguzhaninan/Stacer">Stacer</a> — the original Linux optimizer by Oguzhan INAN</sub>

</div>
