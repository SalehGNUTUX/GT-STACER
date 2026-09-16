#include "relief_page.h"
#include "../../Widgets/table_util.h"
#include "../../../gt-stacer-core/Tools/relief_tool.h"
#include "../../../gt-stacer-core/Info/memory_info.h"
#include "../../../gt-stacer-core/Info/cpu_info.h"
#include "../../Managers/setting_manager.h"
#include "../../Managers/theme.h"
#include "../../Widgets/status_pill.h"
#include <QBrush>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QString humanKB(qint64 kb)
{
    double mb = kb / 1024.0;
    if (mb >= 1024.0) return QString::number(mb / 1024.0, 'f', 2) + " GB";
    return QString::number(mb, 'f', 0) + " MB";
}

// Warm-cool color for the memory bar: heavier processes (the ones most worth
// freezing) read hotter. Uses the theme palette so it follows dark/light.
QColor barColor(double ratio)
{
    if (ratio >= 0.75) return Theme::red();
    if (ratio >= 0.40) return Theme::yellow();
    return Theme::green();
}
}

ReliefPage::ReliefPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("System Relief"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto *intro = new QLabel(tr(
        "Under heavy load, temporarily freeze idle background apps so the "
        "foreground gets the RAM and CPU it needs. Freezing is fully reversible "
        "— no data is lost — and everything is thawed when you resume or quit."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    // Live pressure readout + status badge + auto-refresh controls.
    auto *live = new QHBoxLayout;
    m_cpuLabel = new QLabel;
    m_ramLabel = new QLabel;
    m_ioLabel  = new QLabel;   // disk-pressure (PSI); hidden if the kernel lacks PSI
    m_badge    = new QLabel;
    m_badge->setObjectName("reliefStatusBadge");
    live->addWidget(m_cpuLabel);
    live->addSpacing(18);
    live->addWidget(m_ramLabel);
    live->addSpacing(18);
    live->addWidget(m_ioLabel);
    live->addSpacing(18);
    live->addWidget(m_badge);
    live->addStretch();
    m_autoRefresh = new QCheckBox(tr("Auto-refresh"));
    m_refreshSecs = new QSpinBox; m_refreshSecs->setRange(2, 120); m_refreshSecs->setSuffix(tr(" s"));
    m_refreshBtn  = new QPushButton(tr("Refresh list"));
    live->addWidget(m_autoRefresh);
    live->addWidget(m_refreshSecs);
    live->addSpacing(10);
    live->addWidget(m_refreshBtn);
    root->addLayout(live);

    m_banner = new QLabel;
    m_banner->setWordWrap(true);
    m_banner->setObjectName("bannerText");
    m_banner->setVisible(false);
    root->addWidget(m_banner);

    // Candidate table.
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({tr("Process"), tr("User"), tr("Memory"), tr("CPU %"), tr("Disk")});
    setupResizableTable(m_table, 0);   // primary column fills; all resizable
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // Action row.
    auto *actions = new QHBoxLayout;
    m_suspendBtn = new QPushButton(tr("Relieve now (freeze selected)"));
    m_suspendBtn->setObjectName("primaryButton");
    m_easeBtn      = new QPushButton(tr("Ease disk I/O (lower priority)"));
    m_easeBtn->setToolTip(tr("Lower the disk-I/O priority of the ticked processes (ionice idle) "
                             "so the foreground gets a responsive disk — without freezing them. "
                             "Works best under the BFQ scheduler."));
    m_resumeBtn    = new QPushButton(tr("Resume all"));
    m_selectAllBtn = new QPushButton(tr("Select all"));
    m_dropCaches   = new QCheckBox(tr("Also drop file caches (frees cached RAM, needs authorization)"));
    actions->addWidget(m_suspendBtn);
    actions->addWidget(m_easeBtn);
    actions->addWidget(m_resumeBtn);
    actions->addWidget(m_selectAllBtn);
    actions->addSpacing(16);
    actions->addWidget(m_dropCaches);
    actions->addStretch();
    root->addLayout(actions);

    // Disk I/O scheduler — switching an old HDD to BFQ is the biggest single win
    // for responsiveness under load. Shown only when a scheduler is readable.
    m_disk = ReliefTool::rootDisk();
    if (!m_disk.isEmpty() && !ReliefTool::ioScheduler(m_disk).isEmpty()) {
        auto *schedRow = new QHBoxLayout;
        m_schedLabel = new QLabel;
        m_schedLabel->setObjectName("infoValue");
        m_bfqBtn = new QPushButton(tr("Switch to BFQ (better responsiveness)"));
        m_bfqBtn->setToolTip(tr("BFQ keeps the desktop responsive while the disk is busy, "
                                "and makes lowering I/O priority actually take effect. "
                                "Applies until reboot; needs authorization."));
        schedRow->addWidget(m_schedLabel);
        schedRow->addSpacing(12);
        schedRow->addWidget(m_bfqBtn);
        schedRow->addStretch();
        root->addLayout(schedRow);
        connect(m_bfqBtn, &QPushButton::clicked, this, &ReliefPage::switchToBfq);
        updateSchedulerUi();
    }

    // Automatic mode — grouped so its purpose (background protection that starts
    // with the app when enabled) reads clearly.
    auto *autoGroup = new QGroupBox(tr("Automatic relief (runs in the background, starts with the app)"));
    autoGroup->setObjectName("reliefGroup");
    auto *autoRow = new QHBoxLayout(autoGroup);
    m_autoCheck = new QCheckBox(tr("Enable"));
    m_cpuThresh = new QSpinBox; m_cpuThresh->setRange(0, 100); m_cpuThresh->setSuffix(tr(" % CPU"));
    m_ramThresh = new QSpinBox; m_ramThresh->setRange(0, 100); m_ramThresh->setSuffix(tr(" % RAM"));
    m_holdSecs  = new QSpinBox; m_holdSecs->setRange(2, 120);  m_holdSecs->setSuffix(tr(" s"));
    autoRow->addWidget(m_autoCheck);
    autoRow->addSpacing(12);
    autoRow->addWidget(new QLabel(tr("when over")));
    autoRow->addWidget(m_cpuThresh);
    autoRow->addWidget(new QLabel(tr("or")));
    autoRow->addWidget(m_ramThresh);
    // Disk-pressure trigger — only where the kernel exposes PSI.
    if (ReliefTool::ioPressure() >= 0) {
        m_ioThresh = new QSpinBox; m_ioThresh->setRange(0, 100); m_ioThresh->setSuffix(tr(" % disk"));
        autoRow->addWidget(new QLabel(tr("or")));
        autoRow->addWidget(m_ioThresh);
    }
    autoRow->addWidget(new QLabel(tr("for")));
    autoRow->addWidget(m_holdSecs);
    autoRow->addStretch();
    root->addWidget(autoGroup);

    connect(m_refreshBtn,   &QPushButton::clicked, this, &ReliefPage::refreshCandidates);
    connect(m_suspendBtn,   &QPushButton::clicked, this, &ReliefPage::suspendSelected);
    connect(m_easeBtn,      &QPushButton::clicked, this, &ReliefPage::easeSelected);
    connect(m_resumeBtn,    &QPushButton::clicked, this, &ReliefPage::resumeAll);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &ReliefPage::toggleSelectAll);
    connect(m_autoCheck,    &QCheckBox::toggled,   this, &ReliefPage::onAutoToggled);

    // Persist every setting the moment it changes.
    connect(m_autoRefresh, &QCheckBox::toggled, this, [this](bool){ m_refreshAccum = 0; saveSettings(); });
    connect(m_dropCaches,  &QCheckBox::toggled, this, [this](bool){ saveSettings(); });
    auto persist = [this](int){ saveSettings(); };
    connect(m_cpuThresh,   QOverload<int>::of(&QSpinBox::valueChanged), this, persist);
    connect(m_ramThresh,   QOverload<int>::of(&QSpinBox::valueChanged), this, persist);
    if (m_ioThresh) connect(m_ioThresh, QOverload<int>::of(&QSpinBox::valueChanged), this, persist);
    connect(m_holdSecs,    QOverload<int>::of(&QSpinBox::valueChanged), this, persist);
    connect(m_refreshSecs, QOverload<int>::of(&QSpinBox::valueChanged), this, persist);

    // One monitor drives the live labels, the auto-refresh cadence, and the
    // auto-mode watchdog. Its interval and tray-survival (keepAlive) are managed
    // by applyMonitorCadence() from state + visibility, so it costs nothing when
    // idle and hidden.
    m_monitor = new QTimer(this);
    m_monitor->setObjectName("reliefMonitor");
    m_monitor->setInterval(1000);
    connect(m_monitor, &QTimer::timeout, this, &ReliefPage::tickMonitor);

    loadSettings();
    refreshCandidates();
    updateLiveLabels();
    updateStatusBadge();
    updateBanner();
    applyMonitorCadence();   // may already start the background watchdog at launch
}

ReliefPage::~ReliefPage()
{
    // Safety net: never leave a process frozen behind us.
    if (!m_suspended.isEmpty())
        ReliefTool::resumePids(QVector<int>(m_suspended.begin(), m_suspended.end()));
}

void ReliefPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
}

void ReliefPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_refreshAccum = 0;
    refreshCandidates();     // never show a stale list when the page opens
    updateLiveLabels();
    applyMonitorCadence();
}

void ReliefPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    applyMonitorCadence();   // drop to background cadence, or stop if not watching
}

void ReliefPage::loadSettings()
{
    auto *s = SettingManager::instance();
    m_loading = true;
    m_cpuThresh->setValue(s->reliefCpuThreshold());
    m_ramThresh->setValue(s->reliefRamThreshold());
    if (m_ioThresh) m_ioThresh->setValue(s->reliefIoThreshold());
    m_holdSecs->setValue(s->reliefHoldSeconds());
    m_dropCaches->setChecked(s->reliefDropCaches());
    m_autoRefresh->setChecked(s->reliefAutoRefresh());
    m_refreshSecs->setValue(s->reliefRefreshSeconds());
    m_autoCheck->setChecked(s->reliefAutoMode());   // last: onAutoToggled runs live
    m_loading = false;
}

void ReliefPage::saveSettings()
{
    if (m_loading) return;
    auto *s = SettingManager::instance();
    s->setReliefAutoMode(m_autoCheck->isChecked());
    s->setReliefCpuThreshold(m_cpuThresh->value());
    s->setReliefRamThreshold(m_ramThresh->value());
    if (m_ioThresh) s->setReliefIoThreshold(m_ioThresh->value());
    s->setReliefHoldSeconds(m_holdSecs->value());
    s->setReliefDropCaches(m_dropCaches->isChecked());
    s->setReliefAutoRefresh(m_autoRefresh->isChecked());
    s->setReliefRefreshSeconds(m_refreshSecs->value());
}

void ReliefPage::applyMonitorCadence()
{
    const bool watching = m_autoCheck->isChecked();
    // Only survive minimize-to-tray while actively watching — otherwise the
    // App-level hide logic is allowed to stop us so we cost nothing in the tray.
    m_monitor->setProperty("keepAlive", watching);

    if (isVisible()) {
        m_monitor->setInterval(1000);
        if (!m_monitor->isActive()) m_monitor->start();
    } else if (watching) {
        m_monitor->setInterval(2000);            // relaxed cadence in the background
        if (!m_monitor->isActive()) m_monitor->start();
    } else {
        m_monitor->stop();
    }
}

void ReliefPage::refreshCandidates()
{
    const auto cands = ReliefTool::candidates();

    qint64 maxKB = 1;
    for (const auto &c : cands) maxKB = std::max(maxKB, c.memoryKB);

    // Remember the user's manual check choices so an auto-refresh never wipes them.
    QHash<int, Qt::CheckState> prev;
    for (int r = 0; r < m_table->rowCount(); ++r)
        if (auto *it = m_table->item(r, 0))
            prev.insert(it->data(Qt::UserRole).toInt(), it->checkState());

    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(cands.size());

    auto ensure = [this](int r, int c) -> QTableWidgetItem * {
        auto *it = m_table->item(r, c);
        if (!it) { it = new QTableWidgetItem; m_table->setItem(r, c, it); }
        return it;
    };

    for (int r = 0; r < cands.size(); ++r) {
        const auto &c = cands[r];

        auto *nameItem = m_table->item(r, 0);
        if (!nameItem) {
            nameItem = new QTableWidgetItem;
            nameItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            m_table->setItem(r, 0, nameItem);
        }
        nameItem->setText(c.name);
        nameItem->setData(Qt::UserRole, c.pid);
        // Keep an existing choice; otherwise pre-check the heavier processes.
        const Qt::CheckState cs = prev.contains(c.pid)
            ? prev.value(c.pid)
            : (c.memoryKB >= 150 * 1024 ? Qt::Checked : Qt::Unchecked);
        if (nameItem->checkState() != cs) nameItem->setCheckState(cs);

        ensure(r, 1)->setText(c.user);

        auto *memItem = ensure(r, 2);
        memItem->setText(humanKB(c.memoryKB));
        memItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        // Proportional bar behind the value via an object-relative gradient.
        const double ratio = std::clamp(double(c.memoryKB) / double(maxKB), 0.0, 1.0);
        QColor col = barColor(ratio); col.setAlpha(80);
        QLinearGradient g(0, 0, 1, 0);
        g.setCoordinateMode(QGradient::ObjectBoundingMode);
        g.setColorAt(0.0, col);
        g.setColorAt(std::max(0.0001, ratio), col);
        if (ratio < 1.0) {
            g.setColorAt(std::min(ratio + 0.0001, 1.0), Qt::transparent);
            g.setColorAt(1.0, Qt::transparent);
        }
        memItem->setBackground(QBrush(g));
        memItem->setData(Qt::UserRole, static_cast<qlonglong>(c.memoryKB));

        auto *cpuItem = ensure(r, 3);
        cpuItem->setText(QString::number(c.cpuPercent, 'f', 1));
        cpuItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // Disk I/O rate — the "who is thrashing the disk" column.
        auto *ioItem = ensure(r, 4);
        QString ioText = QStringLiteral("—");
        if (c.ioKBps >= 1024.0)     ioText = QString::number(c.ioKBps / 1024.0, 'f', 1) + " MB/s";
        else if (c.ioKBps >= 1.0)   ioText = QString::number(c.ioKBps, 'f', 0) + " KB/s";
        ioItem->setText(ioText);
        ioItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (c.ioKBps >= 1024.0) ioItem->setForeground(Theme::red());     // heavy disk user
        else if (c.ioKBps >= 200.0) ioItem->setForeground(Theme::yellow());
    }

    m_table->setUpdatesEnabled(true);
    updateStatusBadge();
    updateBanner();
}

void ReliefPage::toggleSelectAll()
{
    const Qt::CheckState cs = m_allSelected ? Qt::Checked : Qt::Unchecked;
    for (int r = 0; r < m_table->rowCount(); ++r)
        if (auto *it = m_table->item(r, 0)) it->setCheckState(cs);
    m_allSelected = !m_allSelected;
    m_selectAllBtn->setText(m_allSelected ? tr("Select all") : tr("Deselect all"));
}

void ReliefPage::suspendSelected()
{
    QVector<int> pids;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto *item = m_table->item(r, 0);
        if (item && item->checkState() == Qt::Checked)
            pids << item->data(Qt::UserRole).toInt();
    }
    if (pids.isEmpty()) {
        QMessageBox::information(this, tr("Nothing selected"),
            tr("Tick the processes you want to freeze first."));
        return;
    }

    const int frozen = ReliefTool::suspendPids(pids);
    for (int pid : pids) m_suspended.insert(pid);

    QString extra;
    if (m_dropCaches->isChecked()) {
        if (ReliefTool::dropCaches()) extra = tr(" File caches dropped.");
        else                          extra = tr(" (cache drop was not authorized)");
    }
    refreshCandidates();
    m_banner->setText(tr("Froze %1 process(es) to relieve pressure.%2 "
                         "Use “Resume all” to thaw them.").arg(frozen).arg(extra));
    m_banner->setVisible(true);
    updateStatusBadge();
    updateBanner();
}

void ReliefPage::resumeAll()
{
    if (m_suspended.isEmpty()) return;
    const QVector<int> pids(m_suspended.begin(), m_suspended.end());
    ReliefTool::resumePids(pids);
    m_suspended.clear();
    m_overSeconds = m_underSeconds = 0;
    refreshCandidates();
    m_banner->setText(tr("All frozen processes have been resumed."));
    m_banner->setVisible(true);
    updateStatusBadge();
}

QVector<int> ReliefPage::checkedPids() const
{
    QVector<int> pids;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto *item = m_table->item(r, 0);
        if (item && item->checkState() == Qt::Checked)
            pids << item->data(Qt::UserRole).toInt();
    }
    return pids;
}

void ReliefPage::easeSelected()
{
    const QVector<int> pids = checkedPids();
    if (pids.isEmpty()) {
        QMessageBox::information(this, tr("Nothing selected"),
            tr("Tick the processes whose disk usage you want to de-prioritize first."));
        return;
    }
    const int n = ReliefTool::easeIoPids(pids);
    QString extra;
    if (m_disk.isEmpty() || ReliefTool::ioScheduler(m_disk) != "bfq")
        extra = " " + tr("Tip: switch the disk to BFQ below to make this take full effect.");
    m_banner->setText(tr("Lowered disk-I/O priority of %1 process(es) to idle — they keep "
                         "running, the foreground gets the disk.%2").arg(n).arg(extra));
    m_banner->setVisible(true);
}

void ReliefPage::switchToBfq()
{
    if (m_disk.isEmpty()) return;
    if (ReliefTool::setScheduler(m_disk, "bfq")) {
        m_banner->setText(tr("Disk %1 switched to the BFQ scheduler — the desktop should stay "
                             "responsive under disk load. (Resets to the default on reboot.)").arg(m_disk));
    } else {
        m_banner->setText(tr("Could not switch to BFQ — the kernel may not provide it "
                             "(module 'bfq'), or authorization was declined."));
    }
    m_banner->setVisible(true);
    updateSchedulerUi();
}

void ReliefPage::updateSchedulerUi()
{
    if (!m_schedLabel || m_disk.isEmpty()) return;
    const QString active = ReliefTool::ioScheduler(m_disk);
    m_schedLabel->setText(tr("Disk %1 scheduler: %2").arg(m_disk, active));
    // Offer BFQ whenever it is not already active — setScheduler tries to load
    // the module, so the option must appear even when it is not yet listed.
    if (m_bfqBtn) m_bfqBtn->setVisible(active != "bfq");
}

void ReliefPage::onAutoToggled(bool on)
{
    m_overSeconds = m_underSeconds = 0;
    saveSettings();
    applyMonitorCadence();
    if (on) {
        m_banner->setText(tr("Automatic mode is watching. Idle apps will be "
                             "frozen under sustained pressure and thawed when it clears."));
        m_banner->setVisible(true);
    }
    updateStatusBadge();
    updateBanner();
}

void ReliefPage::tickMonitor()
{
    // Self-heal: nothing to do if we're hidden and not watching — stop so we
    // cost zero until the page is shown again or auto-mode is re-enabled.
    if (!isVisible() && !m_autoCheck->isChecked()) { m_monitor->stop(); return; }

    // Non-blocking CPU: delta between successive /proc/stat reads (no sleep).
    const CpuStat cur = CpuInfo::readStat();
    if (m_haveStat) m_lastCpu = CpuInfo::calcUsage(m_prevStat, cur);
    m_prevStat = cur;
    m_haveStat = true;

    if (isVisible()) {
        updateLiveLabels();
        // Auto-refresh the visible list on its own cadence (ticks are 1 s here).
        if (m_autoRefresh->isChecked() && ++m_refreshAccum >= m_refreshSecs->value()) {
            m_refreshAccum = 0;
            refreshCandidates();
        }
    }

    if (!m_autoCheck->isChecked()) return;

    const double ram = MemoryInfo::memory().ramPercent();
    const double io  = ReliefTool::ioPressure();   // fresh even in the background
    if (io >= 0) m_lastIo = io;
    const bool overCpu = m_cpuThresh->value() > 0 && m_lastCpu >= m_cpuThresh->value();
    const bool overRam = m_ramThresh->value() > 0 && ram        >= m_ramThresh->value();
    const bool overIo  = m_ioThresh && m_ioThresh->value() > 0 && io >= m_ioThresh->value();
    const bool over    = overCpu || overRam || overIo;
    const int  hold    = m_holdSecs->value();

    if (over) {
        m_underSeconds = 0;
        if (++m_overSeconds >= hold) {
            m_overSeconds = 0;
            // Freeze every current candidate that isn't already frozen.
            QVector<int> pids;
            for (const auto &c : ReliefTool::candidates())
                if (!m_suspended.contains(c.pid)) pids << c.pid;
            if (!pids.isEmpty()) {
                ReliefTool::suspendPids(pids);
                for (int pid : pids) m_suspended.insert(pid);
                if (m_dropCaches->isChecked()) ReliefTool::dropCaches();
                if (isVisible()) refreshCandidates();
                m_banner->setText(tr("⚡ Auto relief: froze %1 idle app(s) under load.")
                                      .arg(pids.size()));
                m_banner->setVisible(true);
                updateStatusBadge();
            }
        }
    } else {
        m_overSeconds = 0;
        if (!m_suspended.isEmpty() && ++m_underSeconds >= hold) {
            m_underSeconds = 0;
            resumeAll();
            m_banner->setText(tr("⚡ Pressure cleared — resumed all frozen apps."));
            m_banner->setVisible(true);
        }
    }
}

void ReliefPage::updateLiveLabels()
{
    const double ram = MemoryInfo::memory().ramPercent();
    m_cpuLabel->setText(tr("CPU: %1%").arg(QString::number(m_lastCpu, 'f', 0)));
    m_ramLabel->setText(tr("RAM: %1%").arg(QString::number(ram, 'f', 0)));
    auto colorFor = [](double v) {
        return (v >= 90 ? Theme::red() : v >= 75 ? Theme::yellow() : Theme::green()).name();
    };
    m_cpuLabel->setStyleSheet(QString("font-weight:bold;color:%1;").arg(colorFor(m_lastCpu)));
    m_ramLabel->setStyleSheet(QString("font-weight:bold;color:%1;").arg(colorFor(ram)));

    // Disk pressure (PSI): the "frozen but CPU/RAM are fine" signal.
    m_lastIo = ReliefTool::ioPressure();
    if (m_lastIo >= 0) {
        auto colorForIo = [](double v){ return (v >= 50 ? Theme::red() : v >= 25 ? Theme::yellow() : Theme::green()).name(); };
        m_ioLabel->setText(tr("Disk: %1%").arg(QString::number(m_lastIo, 'f', 0)));
        m_ioLabel->setStyleSheet(QString("font-weight:bold;color:%1;").arg(colorForIo(m_lastIo)));
        m_ioLabel->setVisible(true);
    } else {
        m_ioLabel->setVisible(false);
    }
}

void ReliefPage::updateStatusBadge()
{
    QString text; QColor color;
    if (!m_suspended.isEmpty()) {
        text  = tr("Frozen %1 app(s)").arg(m_suspended.size());
        color = Theme::red();
    } else if (m_autoCheck->isChecked()) {
        text  = tr("Watching");
        color = Theme::blue();
    } else {
        text  = tr("Idle");
        color = Theme::green();
    }
    setStatusPill(m_badge, text, color);
}

void ReliefPage::updateBanner()
{
    m_resumeBtn->setEnabled(!m_suspended.isEmpty());
    if (m_suspended.isEmpty() && !m_autoCheck->isChecked())
        m_banner->setVisible(false);
}
