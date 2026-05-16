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
                    screenshots_subtitle: 'Browse through the interface — screenshots update based on your selected language.',
                    comparison_tag: 'Comparison',
                    comparison_title: 'GT-STACER vs Original Stacer',
                    comparison_subtitle: 'See how far we\'ve come since the original Stacer 1.1.0 (2019).',
                    download_tag: 'Download',
                    download_title: 'Get GT-STACER 26.06 STABLE',
                    download_subtitle: 'Choose your package format. SHA256 checksums included for verification.',
                    download_note: 'All downloads are from the 26.06 STABLE GitHub release. Verify the SHA256 sums against the page on',
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
                    table_header_gtstacer: 'GT-STACER 26.06 (2026)',
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
                    screenshots_subtitle: 'تصفّح الواجهة — تتغيّر لقطات الشاشة حسب اللغة المختارة.',
                    comparison_tag: 'المقارنة',
                    comparison_title: 'GT-STACER مقارنةً بـ Stacer الأصلي',
                    comparison_subtitle: 'شاهد كم تطوّرنا منذ الإصدار الأصلي Stacer 1.1.0 (2019).',
                    download_tag: 'التنزيل',
                    download_title: 'حمّل GT-STACER 26.06 STABLE',
                    download_subtitle: 'اختر صيغة الحزمة المناسبة. مرفق تجزئة SHA256 للتحقق.',
                    download_note: 'جميع التنزيلات من إصدار 26.06 STABLE على GitHub. للتحقق من سلامة الملفات، طابق تجزئة SHA256 مع',
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
                    table_header_gtstacer: 'GT-STACER 26.06 (2026)',
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
                      title: 'Stable release — every beta regression fixed',
                      bullets: [
                        'Multi-field process search (PID + name + cmdline + user)',
                        '6 process actions + right-click menu + critical-process guard',
                        'Multi-package uninstall (Ctrl/Shift-click)',
                        'Flatpak / Snap drill-down + dynamic package-cache label',
                        'Configurable notifications panel + timer-pause on hide',
                        '19 .qm translation files at 100% coverage',
                        'New Flatpak distribution (org.gnutux.gt-stacer on KDE 6.9)',
                      ] },
                    { ver: 'v26.07', status: 'planned',
                      title: 'Network & power tools',
                      bullets: [
                        'Live network connections (ss -tunap)',
                        'TLP / power profiles UI',
                        'Per-app firewall rules (ufw / firewalld)',
                      ] },
                    { ver: 'v26.08', status: 'future',
                      title: 'Backup & snapshots',
                      bullets: [
                        'Btrfs/ZFS snapshot manager',
                        'rsync-based home backup',
                        'Restore points before risky cleans',
                      ] },
                    { ver: 'v26.09', status: 'future',
                      title: 'Plugin system',
                      bullets: [
                        'Lua/Python plugin API',
                        'Community plugin registry',
                        'Sandboxed execution',
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
                      title: 'الإصدار المستقر — كل مشاكل الاختبار حُلَّت',
                      bullets: [
                        'بحث Processes متعدد الحقول (PID + اسم + cmdline + مستخدم)',
                        '6 إجراءات للعمليات + قائمة سياق + تحذير العمليات الحرجة',
                        'إزالة حزم متعددة (Ctrl/Shift-click)',
                        'Flatpak و Snap drill-down + ذاكرة حزم ديناميكية',
                        'لوحة إعدادات تنبيهات + توقف المؤقتات عند الإخفاء',
                        '19 ملف ترجمة بتغطية 100%',
                        'حزمة Flatpak جديدة (org.gnutux.gt-stacer على KDE 6.9)',
                      ] },
                    { ver: 'إصدار 26.07', status: 'planned',
                      title: 'أدوات الشبكة والطاقة',
                      bullets: [
                        'اتصالات الشبكة الحية (ss -tunap)',
                        'واجهة TLP / ملفات الطاقة',
                        'قواعد جدار حماية لكل تطبيق',
                      ] },
                    { ver: 'إصدار 26.08', status: 'future',
                      title: 'النسخ الاحتياطية واللقطات',
                      bullets: [
                        'مدير لقطات Btrfs/ZFS',
                        'نسخ احتياطي بـ rsync',
                        'نقاط استعادة قبل عمليات التنظيف الخطرة',
                      ] },
                    { ver: 'إصدار 26.09', status: 'future',
                      title: 'نظام إضافات',
                      bullets: [
                        'واجهة برمجة إضافات بـ Lua/Python',
                        'سجلّ إضافات مجتمعي',
                        'تنفيذ معزول (sandbox)',
                      ] },
                ],
            };

            // ── Changelog (most-recent first) ───────────────────────
            const changelogData = {
                en: [
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
            }, ];
            const comparisonRows = [
                { aspect: 'Framework', stacer: 'Qt5 (EOL)', gtstacer: 'Qt6 ≥ 6.2', stacerIcon: '⚠️',
                gtstacerIcon: '✅' },
                { aspect: 'C++ Standard', stacer: 'C++11', gtstacer: 'C++17', stacerIcon: '⚠️',
                gtstacerIcon: '✅' },
                { aspect: 'Dashboard', stacer: 'Linear progress', gtstacer: 'Animated circular gauges',
                    stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Sidebar', stacer: 'Text only', gtstacer: 'SVG icons + collapsible (220↔64 px)',
                    stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'GPU Monitoring', stacer: '✗', gtstacer: 'Intel · AMD · NVIDIA', stacerIcon: '❌',
                    gtstacerIcon: '✅' },
                { aspect: 'Temperatures', stacer: '✗', gtstacer: 'hwmon · thermal zones', stacerIcon: '❌',
                    gtstacerIcon: '✅' },
                { aspect: 'Battery', stacer: '✗', gtstacer: 'Circular gauge + time remaining', stacerIcon: '❌',
                    gtstacerIcon: '✅' },
                { aspect: 'Flatpak Support', stacer: '✗', gtstacer: '✅', stacerIcon: '❌', gtstacerIcon: '✅' },
                { aspect: 'Package Managers', stacer: 'APT · Snap', gtstacer: '28+ managers', stacerIcon: '⚠️',
                    gtstacerIcon: '✅' },
                { aspect: 'Init Systems', stacer: 'systemd only', gtstacer: 'systemd · OpenRC · runit · s6 · SysV',
                    stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Loading Indicators', stacer: '✗', gtstacer: 'Animated spinner overlay', stacerIcon: '❌',
                    gtstacerIcon: '✅' },
                { aspect: 'System Tray', stacer: '✗', gtstacer: 'CPU% · RAM% tooltip live', stacerIcon: '❌',
                    gtstacerIcon: '✅' },
                { aspect: 'Welcome Screen', stacer: '✗', gtstacer: '6 onboarding slides', stacerIcon: '❌',
                    gtstacerIcon: '✅' },
                { aspect: 'Wayland', stacer: 'Partial', gtstacer: 'Full', stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Arabic / RTL', stacer: '✅', gtstacer: '✅ ar_MA · Western numerals 🇲🇦',
                stacerIcon: '✅', gtstacerIcon: '✅' },
                { aspect: 'Close Behavior', stacer: 'Quit only', gtstacer: 'Configurable (tray / quit)',
                    stacerIcon: '⚠️', gtstacerIcon: '✅' },
                { aspect: 'Theme', stacer: 'Simple QSS', gtstacer: 'Catppuccin dark/light', stacerIcon: '⚠️',
                    gtstacerIcon: '✅' },
            ];

            // ── Download data ──
            // GT-STACER 26.06 STABLE — published 2026-05-15 at
            // https://github.com/SalehGNUTUX/GT-STACER/releases/tag/GT-STACER_26.06_STABLE
            const downloadsData = [{
                id: 'appimage',
                icon: `<i class=\"fa-brands fa-linux\" style=\"font-size:3.5rem;color:#d29922;\"></i>`,
                titleKey: 'pkg_appimage',
                descKey: 'pkg_appimage_desc',
                size: '54 MB',
                sha256: '7a47118a3319208ed0600d053bc63056f215e6e463f5e0bb0bfbfc9a07e30ac4',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.06_STABLE/GT-STACER-26.06-x86_64.AppImage',
                filename: 'GT-STACER-26.06-x86_64.AppImage',
            }, {
                id: 'deb',
                icon: `<i class=\"fa-brands fa-debian\" style=\"font-size:3.5rem;color:#d70751;\"></i>`,
                titleKey: 'pkg_deb',
                descKey: 'pkg_deb_desc',
                size: '1.8 MB',
                sha256: '89e885d9bfbf1ff7d05856b6a39fb02c34e22c0e36b7693b768d1219be75553b',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.06_STABLE/GT-STACER_26.06_amd64.deb',
                filename: 'GT-STACER_26.06_amd64.deb',
            }, {
                id: 'rpm',
                icon: `<i class=\"fa-brands fa-redhat\" style=\"font-size:3.5rem;color:#f85149;\"></i>`,
                titleKey: 'pkg_rpm',
                descKey: 'pkg_rpm_desc',
                size: '8.2 MB',
                sha256: '5304e80737c8f9e2587adfab67a5e691779d6b2708fd6088d44192cb21d880b2',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.06_STABLE/gt-stacer-26.06-2.x86_64.rpm',
                filename: 'gt-stacer-26.06-2.x86_64.rpm',
            }, {
                id: 'flatpak',
                icon: `<i class=\"fa-solid fa-cube\" style=\"font-size:3.5rem;color:#4a90d9;\"></i>`,
                titleKey: 'pkg_flatpak',
                descKey: 'pkg_flatpak_desc',
                size: '2.0 MB',
                sha256: '7322326e4bd3f1022e6c5a8ab99be991880b95f6c7bfdb5a60210a00650341bc',
                url: 'https://github.com/SalehGNUTUX/GT-STACER/releases/download/GT-STACER_26.06_STABLE/GT-STACER-26.06-x86_64.flatpak',
                filename: 'GT-STACER-26.06-x86_64.flatpak',
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
                    $screenshotImg.src = `images/screenshoots/${folder}/${shot.file}`;
                    $screenshotImg.alt = shot.title;
                    $galleryCaption.textContent = shot.title;
                    return;
                }

                // Fade out → swap → fade in
                $screenshotImg.style.opacity = '0';
                setTimeout(() => {
                    $screenshotImg.src = `images/screenshoots/${folder}/${shot.file}`;
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
                    thumb.src = `images/screenshoots/${folder}/${shot.file}`;
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
                ${comparisonRows.map(r => `
                  <tr>
                    <td><strong>${r.aspect}</strong></td>
                    <td><span class="${r.stacerIcon === '✅' ? 'check-icon' : r.stacerIcon === '❌' ? 'cross-icon' : ''}">${r.stacerIcon}</span> ${r.stacer}</td>
                    <td><span class="check-icon">${r.gtstacerIcon}</span> <strong>${r.gtstacer}</strong></td>
                  </tr>
                `).join('')}
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
