        (function() {
            // ── State ──
            let currentLang = localStorage.getItem('gtstacer-lang') || 'en';
            let currentTheme = localStorage.getItem('gtstacer-theme') || 'dark';
            let currentScreenshotIndex = 0;

            // ── Translations ──
            const i18n = {
                en: {
                    nav_features: 'Features',
                    nav_screenshots: 'Screenshots',
                    nav_comparison: 'Comparison',
                    nav_download: 'Download',
                    hero_title1: 'GNU/Linux System',
                    hero_title2: 'Optimizer & Monitor',
                    hero_desc: 'A modern fork of Stacer, rebuilt with Qt6 & C++17 for GNU/Linux 2026. Monitor, clean, and manage your system with a beautiful unified interface.',
                    hero_download: '⬇ Download Now',
                    hero_explore: 'Explore Features',
                    features_tag: 'Features',
                    features_title: 'What Makes GT-STACER Different',
                    features_subtitle: 'A complete overhaul with modern tooling, broader hardware support, and a refined user experience.',
                    screenshots_tag: 'Screenshots',
                    screenshots_title: 'See GT-STACER in Action',
                    screenshots_subtitle: 'Browse through the interface — screenshots follow your selected language and theme.',
                    comparison_tag: 'Comparison',
                    comparison_title: 'GT-STACER vs Original Stacer',
                    comparison_subtitle: 'See how far we\'ve come since the original Stacer 1.1.0 (2019).',
                    download_tag: 'Download',
                    download_title: 'Get GT-STACER 26.09 STABLE',
                    download_subtitle: 'Choose your package format. SHA256 checksums included for verification.',
                    download_note: 'All downloads are from the 26.09 STABLE GitHub release. Verify the SHA256 sums against the page on',
                    footer_made: 'Made with',
                    footer_by: 'by',
                    footer_inspired: 'Inspired by',
                    footer_original: 'by Oguzhan INAN',
                    footer_rights: 'Released under the GNU GPL v3 — free software, no warranty.',
                    nav_roadmap: 'Roadmap',
                    nav_changelog: 'Changelog',
                    roadmap_tag: 'Roadmap',
                    roadmap_title: "What's next",
                    roadmap_subtitle: 'A rolling plan — feedback and pull requests welcome.',
                    changelog_tag: 'Changelog',
                    changelog_title: 'Recent changes',
                    changelog_subtitle: 'For the full history, see the CHANGELOG.md on GitHub.',
                    changelog_full: 'View full changelog on GitHub →',
                    sha256_label: 'SHA256:',
                    copy_sha: 'Click to copy SHA256',
                    sha_copied: 'SHA256 copied!',
                    pkg_appimage: 'AppImage',
                    pkg_rpm: 'RPM Package',
                    pkg_deb: 'DEB Package',
                    pkg_flatpak: 'Flatpak Bundle',
                    pkg_appimage_desc: 'Portable — no installation required',
                    pkg_rpm_desc: 'For Fedora, RHEL, AlmaLinux, Rocky, openSUSE',
                    pkg_deb_desc: 'For Debian, Ubuntu, Mint, Kali, Trixie+',
                    pkg_flatpak_desc: 'Sandboxed — runs on any distro with Flatpak',
                    table_header_aspect: 'Aspect',
                    table_header_stacer: 'Stacer 1.1.0 (2019)',
                    table_header_gtstacer: 'GT-STACER 26.09 (2026)',
                },
                ar: {
                    nav_features: 'المزايا',
                    nav_screenshots: 'لقطات الشاشة',
                    nav_comparison: 'المقارنة',
                    nav_download: 'التنزيل',
                    hero_title1: 'محسّن ومراقب',
                    hero_title2: 'نظام غنو/لينكس',
                    hero_desc: 'انشقاق حديث من مشروع Stacer، أُعيد بناؤه بـ Qt6 و C++17 ليناسب بيئة غنو/لينكس لعام 2026. راقب نظامك ونظّفه وأدره بواجهة موحّدة وجميلة.',
                    hero_download: '⬇ حمّل الآن',
                    hero_explore: 'استكشف المزايا',
                    features_tag: 'المزايا',
                    features_title: 'ما الذي يميّز GT-STACER',
                    features_subtitle: 'تطوير شامل بأدوات حديثة، دعم أوسع للعتاد، وتجربة مستخدم محسّنة.',
                    screenshots_tag: 'لقطات الشاشة',
                    screenshots_title: 'شاهد GT-STACER أثناء العمل',
                    screenshots_subtitle: 'تصفّح الواجهة — تتغيّر لقطات الشاشة حسب اللغة والسمة المختارتين.',
                    comparison_tag: 'المقارنة',
                    comparison_title: 'GT-STACER مقارنةً بـ Stacer الأصلي',
                    comparison_subtitle: 'شاهد كم تطوّرنا منذ الإصدار الأصلي Stacer 1.1.0 (2019).',
                    download_tag: 'التنزيل',
                    download_title: 'حمّل GT-STACER 26.09 STABLE',
                    download_subtitle: 'اختر صيغة الحزمة المناسبة. مرفق تجزئة SHA256 للتحقق.',
                    download_note: 'جميع التنزيلات من إصدار 26.09 STABLE على GitHub. للتحقق من سلامة الملفات، طابق تجزئة SHA256 مع',
                    footer_made: 'صُنع بـ',
                    footer_by: 'بواسطة',
                    footer_inspired: 'مستوحى من',
                    footer_original: 'لـ Oguzhan INAN',
                    footer_rights: 'مُصدَر تحت رخصة GNU GPL v3 — برنامج حر، بلا ضمان.',
                    nav_roadmap: 'الخريطة',
                    nav_changelog: 'سجل التغييرات',
                    roadmap_tag: 'الخريطة',
                    roadmap_title: 'ما هو قادم',
                    roadmap_subtitle: 'خطة متجدّدة — الملاحظات وطلبات الدمج مرحّب بها.',
                    changelog_tag: 'سجل التغييرات',
                    changelog_title: 'أحدث التغييرات',
                    changelog_subtitle: 'للسجل الكامل، راجع CHANGELOG.md على GitHub.',
                    changelog_full: 'عرض السجل الكامل على GitHub ←',
                    sha256_label: 'SHA256:',
                    copy_sha: 'انقر لنسخ SHA256',
                    sha_copied: 'تم نسخ SHA256!',
                    pkg_appimage: 'AppImage',
                    pkg_rpm: 'حزمة RPM',
                    pkg_deb: 'حزمة DEB',
                    pkg_flatpak: 'حزمة Flatpak',
                    pkg_appimage_desc: 'محمول — لا يتطلب تثبيتاً',
                    pkg_rpm_desc: 'لـ Fedora و RHEL و AlmaLinux و Rocky و openSUSE',
                    pkg_deb_desc: 'لـ Debian و Ubuntu و Mint و Kali و Trixie+',
                    pkg_flatpak_desc: 'معزولة في sandbox — تعمل في أي توزيعة فيها Flatpak',
                    table_header_aspect: 'الجانب',
                    table_header_stacer: 'Stacer 1.1.0 (2019)',
                    table_header_gtstacer: 'GT-STACER 26.09 (2026)',
                }
            };

            // ── Roadmap data ────────────────────────────────────────
            // Status: "shipped" (✓ in v26.05 beta), "planned" (next release), "future" (later).
            const roadmapData = {
                en: [
                    { ver: 'v26.05 beta', status: 'shipped',
                      title: 'Security & performance pass',
                      bullets: [
                        'Command-injection fix in /etc/hosts editor',
                        'Background CPU sampler — no UI blocking',
                        '~70% less RAM after removing QtCharts',
                        '18 translation files compiled (Arabic 100%)',
                      ] },
                    { ver: 'v26.05 beta', status: 'shipped',
                      title: 'UX redesign',
                      bullets: [
                        'System Cleaner — icon cards + per-category selection',
                        'App-cache details dialog with search & sort',
                        'Startup Apps — toggle, icons, add-from-system',
                        'Quit confirmation dialog (Stacer-style)',
                      ] },
                    { ver: 'v26.06 stable', status: 'shipped',
                      title: 'First stable — every beta regression fixed',
                      bullets: [
                        'Multi-field process search (PID + name + cmdline + user)',
                        '6 process actions + right-click menu + critical-process guard',
                        'Multi-package uninstall (Ctrl/Shift-click)',
                        'Flatpak / Snap drill-down + dynamic package-cache label',
                        'Configurable notifications panel + timer-pause on hide',
                        '19 .qm translation files at 100% coverage',
                        'New Flatpak distribution (org.gnutux.gt-stacer on KDE 6.9)',
                      ] },
                    { ver: 'v26.07 stable', status: 'shipped',
                      title: 'System Relief · Power timer · light theme',
                      bullets: [
                        'System Relief — freeze idle apps (SIGSTOP) to relieve RAM/CPU, fully reversible',
                        'Power timer — schedule shutdown / restart / suspend / hibernate via logind',
                        'Uninstaller detects externally-installed apps (/opt, scripts, AppImages)',
                        'Light theme rebuilt — modern Catppuccin Latte at full parity with dark',
                        'Autostart fixes + Arabic UI at 100% (486/486)',
                      ] },
                    { ver: 'v26.08 stable', status: 'shipped',
                      title: 'Network & power tools',
                      bullets: [
                        'Connections — live TCP/UDP sockets (ss) with owning process, filter & auto-refresh',
                        'Power — profile switching (power-profiles-daemon / cpufreq) + laptop battery charge-limit',
                        'Cross-desktop keep-awake (D-Bus) that detects the desktop\'s own block',
                        'Firewall — ufw / firewalld enable + port rules, one authorization per action',
                      ] },
                    { ver: 'v26.09 stable', status: 'shipped',
                      title: 'Backup, snapshots & recovery',
                      bullets: [
                        'Backup — Timeshift / Snapper / ZFS snapshots with the ideal engine auto-selected for your filesystem',
                        'Home backup — rsync mirror to another disk with live progress',
                        'Recovery — PhotoRec front-end that carves deleted files, with per-type sorting',
                        'Restore point offered before irreversible System Cleaner cleanups',
                      ] },
                    { ver: 'v26.10', status: 'planned',
                      title: 'Plugin system',
                      bullets: [
                        'Lua/Python plugin API',
                        'Community plugin registry',
                        'Sandboxed execution',
                      ] },
                    { ver: 'v26.11', status: 'future',
                      title: 'Scheduling & automation',
                      bullets: [
                        'Scheduled snapshots & home backups (systemd timers)',
                        'Rules engine for automatic maintenance',
                        'Health report & notifications',
                      ] },
                ],
                ar: [
                    { ver: 'إصدار 26.05 beta', status: 'shipped',
                      title: 'تحصين أمني وتحسين أداء',
                      bullets: [
                        'إصلاح ثغرة Command Injection في محرر /etc/hosts',
                        'جامع بيانات CPU في خيط منفصل — بلا تجميد الواجهة',
                        'استهلاك ذاكرة أقل بنسبة ~70% بعد إزالة QtCharts',
                        '18 ملف ترجمة (العربية 100%)',
                      ] },
                    { ver: 'إصدار 26.05 beta', status: 'shipped',
                      title: 'إعادة تصميم تجربة المستخدم',
                      bullets: [
                        'منظف النظام — بطاقات أيقونية + اختيار لكل فئة',
                        'حوار تفاصيل ذاكرة التطبيقات مع بحث وفرز',
                        'بدء التشغيل — مفاتيح تفعيل وأيقونات وإضافة من النظام',
                        'حوار تأكيد الإغلاق (على نمط Stacer)',
                      ] },
                    { ver: 'إصدار 26.06 stable', status: 'shipped',
                      title: 'أول إصدار مستقر — كل مشاكل الاختبار حُلَّت',
                      bullets: [
                        'بحث Processes متعدد الحقول (PID + اسم + cmdline + مستخدم)',
                        '6 إجراءات للعمليات + قائمة سياق + تحذير العمليات الحرجة',
                        'إزالة حزم متعددة (Ctrl/Shift-click)',
                        'Flatpak و Snap drill-down + ذاكرة حزم ديناميكية',
                        'لوحة إعدادات تنبيهات + توقف المؤقتات عند الإخفاء',
                        '19 ملف ترجمة بتغطية 100%',
                        'حزمة Flatpak جديدة (org.gnutux.gt-stacer على KDE 6.9)',
                      ] },
                    { ver: 'إصدار 26.07 stable', status: 'shipped',
                      title: 'إنعاش النظام · مؤقّت الطاقة · الوضع الفاتح',
                      bullets: [
                        'إنعاش النظام — تجميد التطبيقات الخاملة (SIGSTOP) لتخفيف الذاكرة/المعالج، قابل للعكس',
                        'مؤقّت الطاقة — جدولة إطفاء / إعادة تشغيل / تعليق / سُبات عبر logind',
                        'إلغاء التثبيت يكشف البرامج المثبَّتة خارجيًّا (/opt، سكربتات، AppImage)',
                        'إعادة بناء الوضع الفاتح — مظهر Latte عصريّ مكافئ للداكن',
                        'إصلاحات بدء التشغيل + الواجهة العربيّة 100% (486/486)',
                      ] },
                    { ver: 'إصدار 26.08 مستقر', status: 'shipped',
                      title: 'أدوات الشبكة والطاقة',
                      bullets: [
                        'الاتصالات — مقابس TCP/UDP حيّة (ss) بالعمليّة المالكة، مع مرشِّح وتحديث تلقائيّ',
                        'الطاقة — تبديل ملفّ الطاقة (power-profiles-daemon / cpufreq) + حدّ شحن البطاريّة للمحمول',
                        'إبقاء اليقظة المتوافق مع الواجهات (D-Bus) يكشف منع سطح المكتب نفسه',
                        'جدار الحماية — تفعيل ufw / firewalld وقواعد المنافذ، استيثاق مرّة لكل إجراء',
                      ] },
                    { ver: 'إصدار 26.09 مستقر', status: 'shipped',
                      title: 'النسخ الاحتياطية واللقطات والاستعادة',
                      bullets: [
                        'النسخ — لقطات Timeshift / Snapper / ZFS مع اختيار المحرّك المثاليّ لنظام ملفّاتك تلقائيّاً',
                        'نسخ المنزل — مرآة rsync إلى قرص آخر بتقدّم حيّ',
                        'الاستعادة — واجهة PhotoRec تستخرج الملفّات المحذوفة مع فرزها حسب النوع',
                        'عرض نقطة استعادة قبل عمليات تنظيف النظام غير القابلة للتراجع',
                      ] },
                    { ver: 'إصدار 26.10', status: 'planned',
                      title: 'نظام إضافات',
                      bullets: [
                        'واجهة برمجة إضافات بـ Lua/Python',
                        'سجلّ إضافات مجتمعي',
                        'تنفيذ معزول (sandbox)',
                      ] },
                    { ver: 'إصدار 26.11', status: 'future',
                      title: 'الجدولة والأتمتة',
                      bullets: [
                        'جدولة اللقطات والنسخ الاحتياطيّ (مؤقّتات systemd)',
                        'محرّك قواعد للصيانة التلقائيّة',
                        'تقرير صحّة النظام وإشعارات',
                      ] },
                ],
            };

            // ── Changelog (most-recent first) ───────────────────────
            const changelogData = {
                en: [
                    { ver: '26.09 stable', date: '2026-08-01', headline: 'Backup, snapshots & recovery · new Backup and Recovery pages · restore point before risky cleans',
                      items: [
                        'Backup (new) — system snapshots via Timeshift, Snapper or ZFS with the ideal engine auto-detected for your filesystem (and a tagged choice when more than one is available)',
                        'Home backup — mirror your home directory to another disk with rsync, showing live progress and skipping caches/trash (no root — your own files)',
                        'Recovery (new) — a PhotoRec front-end that carves lost/deleted files from a disk, partition or image; pick file types and sort each into its own folder; refuses a destination on the source disk',
                        'System Cleaner offers a Timeshift restore point before irreversible root-level cleanups (old kernels, logs, crash dumps); if it fails, nothing is cleaned',
                        'Sidebar reorganized by workflow with distinct icons; status pills on Firewall and Backup',
                        'Settings scrolls on short windows and aligns its form fields in LTR/RTL; every page re-translates after a runtime language switch',
                      ] },
                    { ver: '26.08 stable', date: '2026-07-31', headline: 'Network & power tools · Connections, Power & Firewall pages · cross-desktop keep-awake',
                      items: [
                        'Connections (new) — live TCP/UDP sockets from ss with the owning process, a text filter, sortable columns, and an auto-refresh that only runs while shown',
                        'Power (new) — switch the power profile (power-profiles-daemon, or cpufreq governors as a fallback; works on desktops); on laptops, a battery charge-limit to extend lifespan',
                        'Keep awake — block automatic sleep and screen locking via the freedesktop D-Bus interfaces (PowerManagement/ScreenSaver), with a systemd-inhibit fallback and a single inhibitor',
                        'Two-way desktop integration — our block shows in the desktop\'s power UI, and HasInhibit() detects a block set elsewhere (e.g. KDE\'s) that systemd-inhibit can\'t see; tray badge + desktop notification',
                        'Firewall (new) — enable/disable ufw or firewalld and add/remove port rules; state read without root, and each change authorizes once (mutate + re-list in one pkexec)',
                        'Background efficiency — pages pause their refresh timers when not shown; the Settings power timer keeps counting down while minimised',
                        'Auto language default (follows the system locale, English if unsupported) + flag emoji rendered as icons so they show on KDE; new Qt6::DBus dependency (inside qt6-base)',
                      ] },
                    { ver: '26.07 stable', date: '2026-07-12', headline: 'System Relief · Power timer · light theme rebuilt · external-app uninstall',
                      items: [
                        'System Relief (new) — temporarily freeze idle apps (SIGSTOP) to relieve RAM/CPU, fully reversible; never freezes the terminal/shell/agent that launched it',
                        'Power timer — schedule shutdown / restart / suspend / hibernate with a live countdown (via logind)',
                        'Autostart fixed — freedesktop-compliant .desktop writing, Flatpak/Snap listing, per-entry start delay; the list now refreshes on show',
                        'Uninstaller now finds externally-installed apps (/opt tarballs, install scripts, AppImages) and respects shared installs',
                        'Light theme rebuilt — a central Theme palette drives every widget; modern Catppuccin Latte at full parity with the (unchanged) dark theme',
                        'Arabic UI at 100% (486/486)',
                      ] },
                    { ver: '26.06 stable', date: '2026-05-15', headline: 'First stable · Flatpak distribution · every beta regression fixed',
                      items: [
                        'Multi-field process search — PID + name + cmdline + user (no more "hidden" processes)',
                        'Six process actions + right-click context menu + critical-process safety guard',
                        'Multi-package uninstall with sequential progress (Ctrl/Shift-click)',
                        'Flatpak / Snap drill-down dialogs in System Cleaner',
                        'Dynamic package-cache card — labels the host\'s manager (APT/DNF/Pacman/Zypper/…)',
                        'Configurable notification thresholds — full Settings UI + master toggle',
                        'Timer pause on hide — ~0% CPU when minimised to tray',
                        '19 .qm translation files at 100% coverage (English/Arabic native, French 83% native)',
                        'New Flatpak distribution: org.gnutux.gt-stacer on org.kde.Platform 6.9, with flatpak-spawn --host for all pkexec calls',
                        'Stricter polkit auth model audited and verified end-to-end inside the Flatpak sandbox',
                      ] },
                    { ver: '26.05 beta', date: '2026-05-14', headline: 'Security · performance · UX overhaul',
                      items: [
                        'Fix Command-Injection vulnerability in /etc/hosts editor',
                        'Sanitize all PackageTool/ServiceTool/AptSourceTool inputs',
                        'Replace QtCharts with QPainter LineChart (~70% less RAM)',
                        'CpuSampler runs on a dedicated worker thread',
                        '/proc-based ProcessInfo (no more `ps` shell-out)',
                        'In-place model updates preserve selection',
                        'Lazy page construction at first navigation',
                        'System Cleaner: icon-card grid with per-category checkboxes',
                        'Per-app cache management dialog with search & sort',
                        'Startup Apps redesign: icons, toggles, add-from-system',
                        'Quit-confirm dialog with remember-my-choice',
                        'libnotify-based alerts for temperature/disk/battery thresholds',
                        'Keyboard shortcuts (Ctrl+1..0, F1, Ctrl+R, Ctrl+Q)',
                        'Auto-follow system theme (Qt 6.5+ colorScheme)',
                        'Wayland taskbar icon fix (setDesktopFileName)',
                        '18 .qm translation files; Arabic at 100%',
                      ] },
                    { ver: '26.04 alpha', date: '2026-04-27', headline: 'First public alpha',
                      items: [
                        'Initial Qt6 / C++17 port from Stacer 1.x',
                        '28+ package manager detection',
                        'GPU monitoring (Intel / AMD / NVIDIA)',
                        'Temperature sensors via hwmon + thermal zones',
                        'AppImage / DEB / RPM packaging scripts',
                      ] },
                ],
                ar: [
                    { ver: '26.09 stable', date: '2026-08-01', headline: 'النسخ الاحتياطية واللقطات والاستعادة · صفحتا النسخ والاستعادة · نقطة استعادة قبل التنظيف الخطر',
                      items: [
                        'النسخ (جديد) — لقطات نظام عبر Timeshift أو Snapper أو ZFS مع كشف المحرّك المثاليّ لنظام ملفّاتك تلقائيّاً (ووسم للاختيار عند تعدّد المحرّكات)',
                        'نسخ المنزل — مرآة لمجلّد المنزل إلى قرص آخر بـ rsync مع تقدّم حيّ وتخطّي الكاش والمهملات (بلا كلمة مرور — ملفّاتك أنت)',
                        'الاستعادة (جديد) — واجهة PhotoRec تستخرج الملفّات المفقودة/المحذوفة من قرص أو قسم أو صورة؛ اختر الأنواع وافرزها في مجلّدات؛ وترفض وجهةً على قرص المصدر',
                        'يعرض «منظّف النظام» إنشاء نقطة استعادة Timeshift قبل عمليات التنظيف الجذريّة غير القابلة للتراجع (النوى القديمة، السجلّات)؛ وإن فشلت لا يُنظَّف شيء',
                        'إعادة ترتيب الشريط الجانبيّ حسب المهمّة بأيقونات مميّزة؛ وشارات حالة على «جدار الحماية» و«النسخ»',
                        'صفحة الإعدادات تُمرَّر على النوافذ الصغيرة وتُحاذي حقولها في RTL/LTR؛ وكلّ الصفحات تُترجَم بعد تبديل اللغة أثناء التشغيل',
                      ] },
                    { ver: '26.08 stable', date: '2026-07-31', headline: 'أدوات الشبكة والطاقة · صفحات الاتصالات والطاقة وجدار الحماية · إبقاء اليقظة المتوافق مع الواجهات',
                      items: [
                        'الاتصالات (جديد) — مقابس TCP/UDP حيّة من ss بالعمليّة المالكة، مع مرشِّح نصّيّ وأعمدة قابلة للفرز وتحديث تلقائيّ يعمل عند ظهور الصفحة فقط',
                        'الطاقة (جديد) — تبديل ملفّ الطاقة (power-profiles-daemon أو حاكمات cpufreq بديلاً؛ يعمل على المكتبيّ)؛ وعلى المحمول حدّ شحن البطاريّة لإطالة عمرها',
                        'إبقاء اليقظة — منع النوم وقفل الشاشة التلقائيّين عبر واجهات D-Bus القياسيّة (PowerManagement/ScreenSaver) مع بديل systemd-inhibit ومُثبِّط واحد',
                        'تكامل ثنائيّ مع سطح المكتب — منعُنا يظهر في أداة طاقة السطح، وHasInhibit() يكشف منعاً مضبوطاً من مكان آخر (مثل KDE) لا يراه systemd-inhibit؛ شارة في الشريط وإشعار نظام',
                        'جدار الحماية (جديد) — تفعيل/تعطيل ufw أو firewalld وإدارة قواعد المنافذ؛ الحالة تُقرأ بلا كلمة مرور، وكل تغيير يستوثق مرّة (تعديل + إعادة سرد في pkexec واحد)',
                        'كفاءة الخلفية — الصفحات توقف مؤقّتاتها عند التنقّل بعيداً؛ ومؤقّت الطاقة يواصل العدّ عند التصغير',
                        'لغة «تلقائيّ» مبدئيّة (تتبع لغة النظام، إنجليزيّة إن لم تُدعم) + أعلام كأيقونات تظهر على KDE؛ اعتماديّة Qt6::DBus جديدة (ضمن qt6-base)',
                      ] },
                    { ver: '26.07 stable', date: '2026-07-12', headline: 'إنعاش النظام · مؤقّت الطاقة · إعادة بناء الوضع الفاتح · كشف البرامج الخارجيّة',
                      items: [
                        'إنعاش النظام (جديد) — تجميد مؤقّت للتطبيقات الخاملة (SIGSTOP) لتخفيف الذاكرة/المعالج، قابل للعكس تماماً؛ لا يجمّد أبداً الطرفية أو الصدفة التي أطلقته',
                        'مؤقّت الطاقة — جدولة إطفاء / إعادة تشغيل / تعليق / سُبات مع عدّاد حيّ (عبر logind)',
                        'إصلاح برامج بدء التشغيل — كتابة .desktop متوافقة مع freedesktop، إدراج Flatpak/Snap، تأخير بدء لكل عنصر؛ القائمة تتحدّث عند العرض',
                        'إلغاء التثبيت يكشف البرامج المثبَّتة خارجيًّا (حزم /opt، سكربتات، AppImage) مع احترام التبعيّات المشتركة',
                        'إعادة بناء الوضع الفاتح — مُعِين ألوان مركزيّ يقود كل العناصر؛ مظهر Latte عصريّ مكافئ للوضع الداكن',
                        'الواجهة العربيّة 100% (486/486)',
                      ] },
                    { ver: '26.06 stable', date: '2026-05-15', headline: 'أول إصدار مستقر · توزيع عبر Flatpak · إصلاح جميع مشاكل الإصدار التجريبي',
                      items: [
                        'بحث Processes متعدد الحقول — PID + الاسم + cmdline + المستخدم (لا مزيد من "العمليات المخفية")',
                        'ستة إجراءات للعمليات + قائمة سياق بالنقر الأيمن + حارس للعمليات الحرجة',
                        'إزالة حزم متعددة مع شريط تقدم تتابعي (Ctrl/Shift-click)',
                        'حوارات Flatpak و Snap drill-down داخل منظف النظام',
                        'بطاقة ذاكرة الحزم الديناميكية — تأخذ اسم مدير الحزم من النظام المضيف (APT/DNF/Pacman/Zypper/…)',
                        'عتبات تنبيهات قابلة للتخصيص — واجهة إعدادات كاملة + مفتاح رئيسي',
                        'إيقاف المؤقتات عند الإخفاء — ~0% من المعالج عند التصغير إلى الشريط',
                        '19 ملف ترجمة .qm بتغطية 100% (الإنجليزية/العربية أصلية، الفرنسية 83%)',
                        'حزمة Flatpak جديدة: org.gnutux.gt-stacer على org.kde.Platform 6.9، مع flatpak-spawn --host لكل استدعاءات pkexec',
                        'تدقيق نموذج صلاحيات polkit وتأكيده داخل بيئة Flatpak معزولة',
                      ] },
                    { ver: '26.05 beta', date: '2026-05-14', headline: 'أمن · أداء · إعادة هيكلة تجربة الاستخدام',
                      items: [
                        'إصلاح ثغرة Command-Injection في محرر /etc/hosts',
                        'تعقيم جميع مدخلات PackageTool/ServiceTool/AptSourceTool',
                        'استبدال QtCharts بـ LineChart مبني على QPainter (~70% توفير ذاكرة)',
                        'CpuSampler يعمل في خيط منفصل',
                        'قراءة العمليات من /proc مباشرة (لا shell-out لـ ps)',
                        'تحديث الجداول in-place يحفظ التحديد',
                        'بناء الصفحات عند أول زيارة (Lazy)',
                        'منظف النظام: شبكة بطاقات أيقونية مع checkbox لكل فئة',
                        'حوار إدارة كاش التطبيقات بالبحث والفرز',
                        'إعادة تصميم بدء التشغيل: أيقونات + toggles + إضافة من النظام',
                        'حوار تأكيد الإغلاق مع "تذكّر خياري"',
                        'تنبيهات libnotify لعتبات الحرارة/القرص/البطارية',
                        'اختصارات لوحة المفاتيح (Ctrl+1..0, F1, Ctrl+R, Ctrl+Q)',
                        'تتبع ثيم النظام تلقائياً (Qt 6.5+)',
                        'إصلاح أيقونة Wayland في شريط المهام',
                        '18 ملف ترجمة .qm؛ العربية مكتملة 100%',
                      ] },
                    { ver: '26.04 alpha', date: '2026-04-27', headline: 'أول إصدار تجريبي عام',
                      items: [
                        'النقل الأولي إلى Qt6 / C++17 من Stacer 1.x',
                        'كشف 28+ مدير حزم',
                        'مراقبة GPU (Intel / AMD / NVIDIA)',
                        'مستشعرات الحرارة عبر hwmon + مناطق حرارية',
                        'سكريبتات تحزيم AppImage / DEB / RPM',
                      ] },
                ],
            };

            // ── Screenshots per language ──
            const screenshotsData = {
                en: [
                    { file: 'Dashboard.png', title: 'Dashboard' },
                    { file: 'Services.png', title: 'Services' },
                    { file: 'Processes.png', title: 'Processes' },
                    { file: 'Resources.png', title: 'Resources' },
                    { file: 'Uninstaller.png', title: 'Uninstaller' },
                    { file: 'System_Cleaner.png', title: 'System Cleaner' },
                    { file: 'Startup_Apps.png', title: 'Startup Apps' },
                    { file: 'APT_Sources0.png', title: 'APT Sources' },
                    { file: 'Helpers.png', title: 'Helpers' },
                    { file: 'Settings.png', title: 'Settings' },
                    { file: 'System_Relief.png', title: 'System Relief' },
                    { file: 'Connections.png', title: 'Connections' },
                    { file: 'Power.png', title: 'Power' },
                    { file: 'Firewall.png', title: 'Firewall' },
                    { file: 'Backup.png', title: 'Backup & Snapshots' },
                    { file: 'Recovery.png', title: 'File Recovery' },
                ],
                ar: [
                    { file: 'لوحة_التحكم.png', title: 'لوحة التحكم' },
                    { file: 'الخدمات.png', title: 'الخدمات' },
                    { file: 'العمليات.png', title: 'العمليات' },
                    { file: 'الموارد.png', title: 'الموارد' },
                    { file: 'إلغاء_التثبيت.png', title: 'إلغاء التثبيت' },
                    { file: 'منظف_النظام.png', title: 'منظف النظام' },
                    { file: 'بدء_التشغيل.png', title: 'بدء التشغيل' },
                    { file: 'أدوات_مساعدة.png', title: 'أدوات مساعدة' },
                    { file: 'الإعدادات.png', title: 'الإعدادات' },
                    { file: 'مصادر_الحزم.png', title: 'مصادر الحزم' },
                    { file: 'إنعاش_النظام.png', title: 'إنعاش النظام' },
                    { file: 'الاتصالات.png', title: 'الاتصالات' },
                    { file: 'الطاقة.png', title: 'الطاقة' },
                    { file: 'جدار_الحماية.png', title: 'جدار الحماية' },
                    { file: 'النسخ_الاحتياطيّة.png', title: 'النسخ الاحتياطيّة' },
                    { file: 'الاستعادة.png', title: 'الاستعادة' },
                ]
            };

            // ── Features data ──
            const featuresData = [{
                icon: '📊',
                titleKey: 'Dashboard',
                descKey: 'Animated circular gauges for CPU, RAM, Disk & Swap with color-coded thresholds and sub-text labels.',
                titleAr: 'لوحة التحكم',
                descAr: 'مقاييس دائرية متحركة للمعالج والذاكرة والقرص و Swap مع ألوان متغيرة حسب الاستخدام ونصوص فرعية واضحة.',
            }, {
                icon: '🖥️',
                titleKey: 'GPU & Sensors',
                descKey: 'Auto-detects Intel, AMD, and NVIDIA GPUs. Monitors temperatures via hwmon and thermal zones.',
                titleAr: 'GPU والمستشعرات',
                descAr: 'يكتشف تلقائياً Intel و AMD و NVIDIA. يراقب درجات الحرارة عبر hwmon والمناطق الحرارية.',
            }, {
                icon: '🔋',
                titleKey: 'Battery Monitor',
                descKey: 'Circular charge gauge with estimated time remaining — perfect for laptop users.',
                titleAr: 'مراقب البطارية',
                descAr: 'مقياس دائري للشحن مع الوقت المتبقي التقديري — مثالي لمستخدمي الأجهزة المحمولة.',
            }, {
                icon: '📦',
                titleKey: '28+ Package Managers',
                descKey: 'Auto-detects APT, DNF, Pacman, Zypper, Flatpak, Snap, XBPS, APK, Portage, Nix, and more.',
                titleAr: 'أكثر من 28 مدير حزم',
                descAr: 'يكتشف تلقائياً APT و DNF و Pacman و Zypper و Flatpak و Snap و XBPS و APK و Portage و Nix وغيرها.',
            }, {
                icon: '⚙️',
                titleKey: 'Multi-Init Services',
                descKey: 'Manage services across systemd, OpenRC, runit, s6, and SysV init systems.',
                titleAr: 'إدارة خدمات متعددة',
                descAr: 'أدر الخدمات عبر أنظمة init المختلفة: systemd و OpenRC و runit و s6 و SysV.',
            }, {
                icon: '🧹',
                titleKey: 'System Cleaner',
                descKey: 'Async scan with animated spinner, expandable columns, and color-coded sizes.',
                titleAr: 'منظف النظام',
                descAr: 'فحص غير متزامن مع مؤشر تحميل متحرك، أعمدة قابلة للتوسيع، وألوان حسب الحجم.',
            }, {
                icon: '🌐',
                titleKey: '19 Languages',
                descKey: 'Arabic (complete, RTL, 🇲🇦), English, and 17 other languages ready for contributors.',
                titleAr: '19 لغة',
                descAr: 'العربية (مكتملة، RTL، 🇲🇦)، الإنجليزية، و17 لغة أخرى جاهزة للمساهمة.',
            }, {
                icon: '🎨',
                titleKey: 'Catppuccin Themes',
                descKey: 'Beautiful dark and light themes with the Catppuccin color palette, plus configurable close behavior.',
                titleAr: 'ثيمات Catppuccin',
                descAr: 'ثيمات داكنة وفاتحة جميلة بألوان Catppuccin، مع سلوك إغلاق قابل للتخصيص.',
            }, {
                icon: '📋',
                titleKey: 'System Tray',
                descKey: 'CPU% and RAM% displayed in the tray tooltip, updated every 3 seconds.',
                titleAr: 'شريط المهام',
                descAr: 'يعرض CPU% و RAM% في Tooltip الشريط ويُحدَّث كل 3 ثوانٍ.',
            }, {
                icon: '💾',
                titleKey: 'Backup & Snapshots',
                descKey: 'System restore points via Timeshift, Snapper or ZFS — the ideal engine is auto-selected for your filesystem — plus an rsync mirror of your home folder to another disk.',
                titleAr: 'النسخ الاحتياطيّة واللقطات',
                descAr: 'نقاط استعادة عبر Timeshift أو Snapper أو ZFS — يُختار المحرّك المثاليّ لنظام ملفّاتك تلقائيّاً — إضافةً إلى نسخ مجلّد المنزل مرآةً بـ rsync إلى قرص آخر.',
            }, {
                icon: '♻️',
                titleKey: 'File Recovery',
                descKey: 'A PhotoRec front-end that carves lost or deleted files from a disk, partition or image — pick file types and sort each into its own folder.',
                titleAr: 'استعادة الملفّات',
                descAr: 'واجهة رسوميّة لـ PhotoRec تستخرج الملفّات المفقودة أو المحذوفة من قرص أو قسم أو صورة — حدّد الأنواع ويُفرَز كلّ نوع في مجلّده.',
            }, {
                icon: '🛡️',
                titleKey: 'Firewall',
                descKey: 'Enable or disable ufw / firewalld and add or remove port rules — one authorization per action.',
                titleAr: 'جدار الحماية',
                descAr: 'فعّل أو عطّل ufw / firewalld وأضِف أو احذف قواعد المنافذ — استيثاق مرّة واحدة لكلّ إجراء.',
            }, {
                icon: '⏻',
                titleKey: 'Power & Keep-Awake',
                descKey: 'Switch the power profile (power-profiles-daemon / cpufreq), cap the laptop battery charge, and block automatic sleep or screen-locking across desktops via D-Bus.',
                titleAr: 'الطاقة وإبقاء اليقظة',
                descAr: 'بدّل ملفّ الطاقة (power-profiles-daemon / cpufreq)، وحُدّ شحن بطاريّة المحمول، وامنع النوم أو قفل الشاشة تلقائيّاً عبر D-Bus على مختلف الواجهات.',
            }, {
                icon: '🔌',
                titleKey: 'Live Connections',
                descKey: 'Every active TCP/UDP socket and the process that owns it (from ss), with a live filter, sortable columns and page-scoped auto-refresh.',
                titleAr: 'الاتصالات الحيّة',
                descAr: 'كلّ مقبس TCP/UDP نشِط والعمليّة المالكة له (من ss)، مع مرشِّح حيّ وأعمدة قابلة للفرز وتحديث تلقائيّ عند العرض.',
            }, {
                icon: '⚡',
                titleKey: 'System Relief',
                descKey: 'Freeze idle background apps (SIGSTOP) to relieve RAM/CPU pressure and thaw them when it clears — manual or automatic, fully reversible.',
                titleAr: 'إنعاش النظام',
                descAr: 'جمّد التطبيقات الخاملة (SIGSTOP) لتخفيف ضغط الذاكرة/المعالج ثمّ أذِبها عند زواله — يدويّ أو تلقائيّ، قابل للعكس بالكامل.',
            }, ];
            const comparisonRows = [
                { aspect: 'Framework', aspectAr: 'إطار العمل',
                  stacer: 'Qt5 (EOL)', stacerAr: 'Qt5 (منتهٍ)', gtstacer: 'Qt6 ≥ 6.2', gtstacerAr: 'Qt6 ≥ 6.2', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'C++ Standard', aspectAr: 'معيار C++',
                  stacer: 'C++11', stacerAr: 'C++11', gtstacer: 'C++17', gtstacerAr: 'C++17', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Dashboard', aspectAr: 'لوحة التحكّم',
                  stacer: 'Linear progress', stacerAr: 'أشرطة خطّية', gtstacer: 'Animated circular gauges', gtstacerAr: 'عدّادات دائريّة متحرّكة', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Sidebar', aspectAr: 'الشريط الجانبيّ',
                  stacer: 'Text only', stacerAr: 'نصّ فقط', gtstacer: 'SVG icons + collapsible (220↔64 px)', gtstacerAr: 'أيقونات SVG + قابل للطيّ (220↔64 بكسل)', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'GPU Monitoring', aspectAr: 'مراقبة كرت الشاشة',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'Intel · AMD · NVIDIA', gtstacerAr: 'Intel · AMD · NVIDIA', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Temperatures', aspectAr: 'الحرارة',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'hwmon · thermal zones', gtstacerAr: 'hwmon · مناطق حراريّة', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Battery', aspectAr: 'البطاريّة',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'Circular gauge + time remaining', gtstacerAr: 'عدّاد دائريّ + الوقت المتبقّي', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Flatpak Support', aspectAr: 'دعم Flatpak',
                  stacer: '✗', stacerAr: '✗', gtstacer: '✅', gtstacerAr: '✅', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Package Managers', aspectAr: 'مدراء الحزم',
                  stacer: 'APT · Snap', stacerAr: 'APT · Snap', gtstacer: '28+ managers', gtstacerAr: '28+ مديراً', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Init Systems', aspectAr: 'أنظمة التهيئة',
                  stacer: 'systemd only', stacerAr: 'systemd فقط', gtstacer: 'systemd · OpenRC · runit · s6 · SysV', gtstacerAr: 'systemd · OpenRC · runit · s6 · SysV', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Loading Indicators', aspectAr: 'مؤشّرات التحميل',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'Animated spinner overlay', gtstacerAr: 'طبقة دوّار متحرّكة', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'System Tray', aspectAr: 'شريط النظام',
                  stacer: 'Tray icon', stacerAr: 'أيقونة شريط', gtstacer: 'Tray + live CPU% / RAM% tooltip', gtstacerAr: 'شريط + Tooltip حيّ لـ CPU%/RAM%', stacerIcon: '✅', gtstacerIcon: '✅' },
                { aspect: 'Welcome Screen', aspectAr: 'شاشة الترحيب',
                  stacer: '✗', stacerAr: '✗', gtstacer: '6 onboarding slides', gtstacerAr: '6 شرائح تعريفيّة', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Wayland', aspectAr: 'Wayland',
                  stacer: 'Partial', stacerAr: 'جزئيّ', gtstacer: 'Full', gtstacerAr: 'كامل', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Arabic / RTL', aspectAr: 'العربيّة / RTL',
                  stacer: '✅', stacerAr: '✅', gtstacer: '✅ ar_MA · Western numerals 🇲🇦', gtstacerAr: '✅ ar_MA · أرقام غربيّة 🇲🇦', stacerIcon: '✅', gtstacerIcon: '✅' },
                { aspect: 'Close Behavior', aspectAr: 'سلوك الإغلاق',
                  stacer: 'Tray / quit', stacerAr: 'شريط / إنهاء', gtstacer: 'Tray / quit + confirm dialog', gtstacerAr: 'شريط / إنهاء + حوار تأكيد', stacerIcon: '✅', gtstacerIcon: '✅' },
                { aspect: 'Theme', aspectAr: 'السمة',
                  stacer: 'Simple QSS', stacerAr: 'QSS بسيط', gtstacer: 'Catppuccin dark/light', gtstacerAr: 'Catppuccin داكن/فاتح', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'System Relief', aspectAr: 'إنعاش النظام',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'Freeze/thaw idle apps (SIGSTOP) — manual + auto', gtstacerAr: 'تجميد/استئناف الخاملة (SIGSTOP) — يدويّ + تلقائيّ', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Power timer', aspectAr: 'مؤقّت الطاقة',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'Scheduled shutdown / restart / suspend / hibernate', gtstacerAr: 'إطفاء/إعادة/تعليق/سُبات مجدول', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Live Connections', aspectAr: 'الاتصالات الحيّة',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'TCP/UDP sockets + owning process (ss)', gtstacerAr: 'مقابس TCP/UDP + العمليّة المالكة (ss)', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Power profiles', aspectAr: 'ملفّات الطاقة',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'power-profiles-daemon / cpufreq + battery charge-limit', gtstacerAr: 'power-profiles-daemon / cpufreq + حدّ شحن البطاريّة', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Keep-awake', aspectAr: 'إبقاء اليقظة',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'Block sleep / screen-lock via D-Bus (KDE/GNOME…)', gtstacerAr: 'منع النوم/قفل الشاشة عبر D-Bus (KDE/غنوم…)', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Firewall', aspectAr: 'جدار الحماية',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'ufw / firewalld — enable + port rules', gtstacerAr: 'ufw / firewalld — تفعيل + قواعد منافذ', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Backup & snapshots', aspectAr: 'النسخ واللقطات',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'Timeshift / Snapper / ZFS + rsync home mirror', gtstacerAr: 'Timeshift / Snapper / ZFS + مرآة rsync للمنزل', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'File recovery', aspectAr: 'استعادة الملفّات',
                  stacer: '✗', stacerAr: '✗', gtstacer: 'PhotoRec front-end — type-select + per-type sort', gtstacerAr: 'واجهة PhotoRec — تحديد الأنواع + فرز لكلّ نوع', stacerIcon: '❌', gtstacerIcon: '✅' },
            ];

            // ── Download data ──
            // GT-STACER 26.09 STABLE — published 2026-08-01 at
            // https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.09_STABLE
            // NOTE: size + sha256 for each asset are filled after the packages are built.
            const downloadsData = [{
                id: 'appimage',
                icon: `<i class=\"fa-brands fa-linux\" style=\"font-size:3.5rem;color:#d29922;\"></i>`,
                titleKey: 'pkg_appimage',
                descKey: 'pkg_appimage_desc',
                size: '52 MB',
                sha256: 'fa24fb2b82995a4bd277b487ec0d9c8098054900301f795ccb7b9568c64aaf94',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.09_STABLE/GT-STACER-26.09-x86_64.AppImage',
                filename: 'GT-STACER-26.09-x86_64.AppImage',
            }, {
                id: 'deb',
                icon: `<i class=\"fa-brands fa-debian\" style=\"font-size:3.5rem;color:#d70751;\"></i>`,
                titleKey: 'pkg_deb',
                descKey: 'pkg_deb_desc',
                size: '2.1 MB',
                sha256: 'ef3e60e9ac2056d46755d6d7d2db75cc4e29dfb31490ef381a1c2c397d0e6000',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.09_STABLE/GT-STACER_26.09_amd64.deb',
                filename: 'GT-STACER_26.09_amd64.deb',
            }, {
                id: 'rpm',
                icon: `<i class=\"fa-brands fa-redhat\" style=\"font-size:3.5rem;color:#f85149;\"></i>`,
                titleKey: 'pkg_rpm',
                descKey: 'pkg_rpm_desc',
                size: '2.4 MB',
                sha256: '7bb15478891b819ea852cb3d187e15fd8707a0832e2476865160d6aa13ba7ffb',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.09_STABLE/gt-stacer-26.09-2.x86_64.rpm',
                filename: 'gt-stacer-26.09-2.x86_64.rpm',
            }, {
                id: 'flatpak',
                icon: `<i class=\"fa-solid fa-cube\" style=\"font-size:3.5rem;color:#4a90d9;\"></i>`,
                titleKey: 'pkg_flatpak',
                descKey: 'pkg_flatpak_desc',
                size: '2.1 MB',
                sha256: '85c0a86b6df0fde685718212b2bfe3c06992b2ce61d178c0c3a97ca36ffc050a',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.09_STABLE/GT-STACER-26.09-x86_64.flatpak',
                filename: 'GT-STACER-26.09-x86_64.flatpak',
            }, ];

            // ── DOM refs ──
            const $html = document.documentElement;
            const $navbar = document.getElementById('navbar');
            const $themeToggle = document.getElementById('themeToggle');
            const $themeIcon = document.getElementById('themeIcon');
            const $langToggle = document.getElementById('langToggle');
            const $langLabel = document.getElementById('langLabel');
            const $hamburger = document.getElementById('hamburger');
            const $navLinks = document.getElementById('navLinks');
            const $screenshotImg = document.getElementById('screenshotImg');
            const $galleryDots = document.getElementById('galleryDots');
            const $galleryCaption = document.getElementById('galleryCaption');
            const $featuresGrid = document.getElementById('featuresGrid');
            const $comparisonTable = document.getElementById('comparisonTable');
            const $downloadGrid = document.getElementById('downloadGrid');
            const $roadmapGrid = document.getElementById('roadmapGrid');
            const $changelogList = document.getElementById('changelogList');
            const $toast = document.getElementById('toast');

            // ── Theme ──
            function applyTheme(theme) {
                $html.setAttribute('data-theme', theme);
                $themeIcon.textContent = theme === 'dark' ? '☀️' : '🌙';
                localStorage.setItem('gtstacer-theme', theme);
                currentTheme = theme;
                // Screenshots live under a per-theme folder, so re-point them when
                // the theme changes (dots + main image).
                if (typeof updateScreenshot === 'function') {
                    updateScreenshot(true);
                    buildGalleryDots();
                }
            }
            $themeToggle.addEventListener('click', () => {
                applyTheme(currentTheme === 'dark' ? 'light' : 'dark');
            });

            // ── Language ──
            function applyLanguage(lang) {
                currentLang = lang;
                $html.setAttribute('lang', lang);
                $html.setAttribute('dir', lang === 'ar' ? 'rtl' : 'ltr');
                $langLabel.textContent = lang === 'ar' ? 'EN' : 'AR';
                localStorage.setItem('gtstacer-lang', lang);

                // Update all i18n elements
                document.querySelectorAll('[data-i18n]').forEach(el => {
                    const key = el.getAttribute('data-i18n');
                    if (i18n[lang] && i18n[lang][key]) {
                        el.textContent = i18n[lang][key];
                    }
                });

                // Rebuild dynamic sections
                buildFeatures();
                buildComparisonTable();
                buildDownloadCards();
                buildRoadmap();
                buildChangelog();
                updateScreenshot(true);
                buildGalleryDots();
                resetAutoplay();
            }
            $langToggle.addEventListener('click', () => {
                applyLanguage(currentLang === 'en' ? 'ar' : 'en');
            });

            // ── Screenshot Gallery ──
            let autoplayTimer = null;

            function getScreenshots() {
                return screenshotsData[currentLang] || screenshotsData['en'];
            }

            function updateScreenshot(skipFade) {
                const shots = getScreenshots();
                if (shots.length === 0) return;
                const idx = Math.min(currentScreenshotIndex, shots.length - 1);
                const shot = shots[idx];
                const folder = currentLang === 'ar' ? 'AR' : 'EN';

                // Update dots immediately
                const dots = $galleryDots.querySelectorAll('.gallery-dot');
                dots.forEach((dot, i) => dot.classList.toggle('active', i === idx));
                scrollThumbIntoView(idx);

                if (skipFade) {
                    $screenshotImg.src = `images/screenshoots/${folder}/${currentTheme}/${shot.file}`;
                    $screenshotImg.alt = shot.title;
                    $galleryCaption.textContent = shot.title;
                    return;
                }

                // Fade out → swap → fade in
                $screenshotImg.style.opacity = '0';
                setTimeout(() => {
                    $screenshotImg.src = `images/screenshoots/${folder}/${currentTheme}/${shot.file}`;
                    $screenshotImg.alt = shot.title;
                    $galleryCaption.textContent = shot.title;
                    $screenshotImg.onload = () => { $screenshotImg.style.opacity = '1'; };
                    // Fallback if image is cached
                    setTimeout(() => { $screenshotImg.style.opacity = '1'; }, 80);
                }, 400);
            }

            function buildGalleryDots() {
                const shots = getScreenshots();
                const folder = currentLang === 'ar' ? 'AR' : 'EN';
                $galleryDots.innerHTML = '';
                shots.forEach((shot, i) => {
                    const dot = document.createElement('button');
                    dot.className = 'gallery-dot' + (i === currentScreenshotIndex ? ' active' : '');
                    dot.setAttribute('aria-label', shot.title);

                    const thumb = document.createElement('img');
                    thumb.src = `images/screenshoots/${folder}/${currentTheme}/${shot.file}`;
                    thumb.alt = shot.title;
                    thumb.loading = 'lazy';

                    const label = document.createElement('span');
                    label.className = 'gallery-dot-label';
                    label.textContent = shot.title;

                    dot.appendChild(thumb);
                    dot.appendChild(label);

                    dot.addEventListener('click', () => {
                        currentScreenshotIndex = i;
                        resetAutoplay();
                        updateScreenshot();
                    });
                    $galleryDots.appendChild(dot);
                });
            }

            function scrollThumbIntoView(idx) {
                const dots = $galleryDots.querySelectorAll('.gallery-dot');
                if (dots[idx]) {
                    const scroll = document.getElementById('galleryThumbsScroll');
                    const dot = dots[idx];
                    const scrollLeft = dot.offsetLeft - scroll.offsetWidth / 2 + dot.offsetWidth / 2;
                    scroll.scrollTo({ left: scrollLeft, behavior: 'smooth' });
                }
            }

            // ── Autoplay ──
            function startAutoplay() {
                if (autoplayTimer) return;
                autoplayTimer = setInterval(() => {
                    const shots = getScreenshots();
                    currentScreenshotIndex = (currentScreenshotIndex + 1) % shots.length;
                    updateScreenshot();
                }, 4500);
            }

            function stopAutoplay() {
                clearInterval(autoplayTimer);
                autoplayTimer = null;
            }

            function resetAutoplay() {
                stopAutoplay();
                startAutoplay();
            }

            // Pause on hover
            const $laptopContainer = document.querySelector('.laptop-container');
            $laptopContainer.addEventListener('mouseenter', stopAutoplay);
            $laptopContainer.addEventListener('mouseleave', startAutoplay);

            document.getElementById('galleryPrev').addEventListener('click', () => {
                const shots = getScreenshots();
                currentScreenshotIndex = (currentScreenshotIndex - 1 + shots.length) % shots.length;
                resetAutoplay();
                updateScreenshot();
            });
            document.getElementById('galleryNext').addEventListener('click', () => {
                const shots = getScreenshots();
                currentScreenshotIndex = (currentScreenshotIndex + 1) % shots.length;
                resetAutoplay();
                updateScreenshot();
            });

            // Keyboard navigation for gallery
            document.addEventListener('keydown', (e) => {
                const shots = getScreenshots();
                if (e.key === 'ArrowLeft') {
                    currentScreenshotIndex = (currentScreenshotIndex - 1 + shots.length) % shots.length;
                    resetAutoplay();
                    updateScreenshot();
                } else if (e.key === 'ArrowRight') {
                    currentScreenshotIndex = (currentScreenshotIndex + 1) % shots.length;
                    resetAutoplay();
                    updateScreenshot();
                }
            });

            // ── Features ──
            function buildFeatures() {
                const lang = currentLang;
                $featuresGrid.innerHTML = '';
                featuresData.forEach(f => {
                    const card = document.createElement('div');
                    card.className = 'feature-card';
                    card.innerHTML = `
                <div class="icon-circle">${f.icon}</div>
                <h3>${lang === 'ar' ? f.titleAr : f.titleKey}</h3>
                <p>${lang === 'ar' ? f.descAr : f.descKey}</p>
              `;
                    $featuresGrid.appendChild(card);
                });
            }

            // ── Comparison Table ──
            function buildComparisonTable() {
                const lang = currentLang;
                const thAspect = i18n[lang]?.table_header_aspect || 'Aspect';
                const thStacer = i18n[lang]?.table_header_stacer || 'Stacer 1.1.0 (2019)';
                const thGtstacer = i18n[lang]?.table_header_gtstacer || 'GT-STACER 26.05 (2026)';
                $comparisonTable.innerHTML = `
              <thead><tr><th>${thAspect}</th><th>${thStacer}</th><th>${thGtstacer}</th></tr></thead>
              <tbody>
                ${comparisonRows.map(r => {
                  const asp = lang === 'ar' ? (r.aspectAr || r.aspect) : r.aspect;
                  const sta = lang === 'ar' ? (r.stacerAr || r.stacer) : r.stacer;
                  const gt  = lang === 'ar' ? (r.gtstacerAr || r.gtstacer) : r.gtstacer;
                  return `
                  <tr>
                    <td><strong>${asp}</strong></td>
                    <td><span class="${r.stacerIcon === '✅' ? 'check-icon' : r.stacerIcon === '❌' ? 'cross-icon' : ''}">${r.stacerIcon}</span> ${sta}</td>
                    <td><span class="check-icon">${r.gtstacerIcon}</span> <strong>${gt}</strong></td>
                  </tr>
                `;}).join('')}
              </tbody>
            `;
            }

            // ── Download Cards ──
            function buildDownloadCards() {
                const lang = currentLang;
                $downloadGrid.innerHTML = '';
                downloadsData.forEach(d => {
                    const title = i18n[lang]?.[d.titleKey] || d.titleKey;
                    const desc = i18n[lang]?.[d.descKey] || d.descKey;
                    const shaLabel = i18n[lang]?.sha256_label || 'SHA256:';
                    const copyHint = i18n[lang]?.copy_sha || 'Click to copy SHA256';
                    const card = document.createElement('div');
                    card.className = 'download-card';
                    card.innerHTML = `
                <div class="pkg-icon">${d.icon}</div>
                <h4>${title}</h4>
                <p class="pkg-size">${d.size} — ${desc}</p>
                <a href="${d.url}" class="btn btn-primary" target="_blank" rel="noopener" style="margin-bottom:0.8rem;">⬇ ${d.filename}</a>
                <div class="sha256" title="${copyHint}" data-sha256="${d.sha256}">
                  <strong>${shaLabel}</strong> ${d.sha256}
                </div>
              `;
                    $downloadGrid.appendChild(card);
                });

                // SHA256 click-to-copy + expand
                document.querySelectorAll('.sha256').forEach(el => {
                    el.addEventListener('click', function(e) {
                        const sha = this.getAttribute('data-sha256');
                        if (sha) {
                            navigator.clipboard.writeText(sha).then(() => {
                                showToast(i18n[currentLang]?.sha_copied || 'SHA256 copied!');
                            }).catch(() => {
                                showToast(sha.substring(0, 20) + '...');
                            });
                        }
                        this.classList.toggle('expanded');
                    });
                });
            }

            // ── Toast ──
            let toastTimer;

            function showToast(msg) {
                clearTimeout(toastTimer);
                $toast.textContent = msg;
                $toast.classList.add('show');
                toastTimer = setTimeout(() => {
                    $toast.classList.remove('show');
                }, 2200);
            }

            // ── Navbar scroll effect ──
            function onScroll() {
                const scrollY = window.scrollY;
                if (scrollY > 50) {
                    $navbar.classList.add('scrolled');
                } else {
                    $navbar.classList.remove('scrolled');
                }
            }
            window.addEventListener('scroll', onScroll, { passive: true });

            // ── Mobile menu ──
            $hamburger.addEventListener('click', () => {
                $navLinks.classList.toggle('mobile-open');
            });
            // Close mobile menu on link click
            $navLinks.querySelectorAll('a').forEach(link => {
                link.addEventListener('click', () => {
                    $navLinks.classList.remove('mobile-open');
                });
            });

            // ── Roadmap renderer ────────────────────────────────────
            function buildRoadmap() {
                if (!$roadmapGrid) return;
                const items = roadmapData[currentLang] || roadmapData.en;
                $roadmapGrid.innerHTML = '';
                items.forEach((item) => {
                    const card = document.createElement('div');
                    card.className = 'roadmap-card roadmap-' + item.status;
                    const badge =
                        item.status === 'shipped'  ? '✓' :
                        item.status === 'planned'  ? '◷' : '✦';
                    card.innerHTML = `
                        <div class="roadmap-head">
                            <span class="roadmap-badge">${badge}</span>
                            <span class="roadmap-ver">${item.ver}</span>
                        </div>
                        <h3 class="roadmap-title">${item.title}</h3>
                        <ul class="roadmap-list">
                            ${item.bullets.map((b) => `<li>${b}</li>`).join('')}
                        </ul>`;
                    $roadmapGrid.appendChild(card);
                });
            }

            // ── Changelog renderer ─────────────────────────────────
            function buildChangelog() {
                if (!$changelogList) return;
                const items = changelogData[currentLang] || changelogData.en;
                $changelogList.innerHTML = '';
                items.forEach((entry) => {
                    const card = document.createElement('article');
                    card.className = 'changelog-entry';
                    card.innerHTML = `
                        <header class="changelog-header">
                            <span class="changelog-ver">${entry.ver}</span>
                            <time class="changelog-date">${entry.date}</time>
                        </header>
                        <p class="changelog-headline">${entry.headline}</p>
                        <ul class="changelog-items">
                            ${entry.items.map((it) => `<li>${it}</li>`).join('')}
                        </ul>`;
                    $changelogList.appendChild(card);
                });
            }

            // ── Init ──
            function init() {
                applyTheme(currentTheme);
                applyLanguage(currentLang);
                buildGalleryDots();
                updateScreenshot(true);
                buildFeatures();
                buildComparisonTable();
                buildDownloadCards();
                buildRoadmap();
                buildChangelog();
                onScroll();
                startAutoplay();
            }

            // ── Handle laptop frame image loading ──
            const laptopFrameImg = document.getElementById('laptopFrame');
            if (laptopFrameImg) {
                laptopFrameImg.addEventListener('load', () => {
                    // Frame loaded successfully
                });
                laptopFrameImg.addEventListener('error', () => {
                    // If laptop frame fails to load, show screenshot without frame
                    laptopFrameImg.style.display = 'none';
                    const screenArea = document.getElementById('laptopScreen');
                    if (screenArea) {
                        screenArea.style.position = 'relative';
                        screenArea.style.left = '0';
                        screenArea.style.top = '0';
                        screenArea.style.right = '0';
                        screenArea.style.bottom = '0';
                        screenArea.style.borderRadius = '12px';
                        screenArea.style.boxShadow = 'var(--shadow-lg)';
                    }
                });
            }

            // ── Handle screenshot image errors ──
            if ($screenshotImg) {
                $screenshotImg.addEventListener('error', () => {
                    $screenshotImg.src =
                        'data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" width="800" height="500" fill="%231a1d2e"><rect width="800" height="500" fill="%231a1d2e"/><text x="400" y="260" text-anchor="middle" fill="%238485a0" font-size="20" font-family="sans-serif">Screenshot not found</text></svg>';
                });
            }

            init();
            console.log('🚀 GT-STACER website initialized');
            console.log('   Language:', currentLang, '| Theme:', currentTheme);
            console.log('   🌐 https://salehgnutux.github.io/GT-STACER');

            // ── Service worker (PWA) ─────────────────────────────
            // Only registers from a real HTTP origin — file:// is rejected by
            // browsers, so local-disk previews still work normally.
            if ('serviceWorker' in navigator && location.protocol.startsWith('http')) {
                window.addEventListener('load', () => {
                    navigator.serviceWorker.register('sw.js')
                        .then((reg) => console.log('   ✓ SW registered (scope:', reg.scope + ')'))
                        .catch((err) => console.warn('   ✗ SW failed:', err));
                });
            }
        })();
