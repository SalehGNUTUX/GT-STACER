#include "relief_page.h"
#include "../../../gt-stacer-core/Tools/relief_tool.h"
#include "../../../gt-stacer-core/Info/memory_info.h"
#include "../../../gt-stacer-core/Info/cpu_info.h"
#include "../../Managers/setting_manager.h"
#include "../../Managers/theme.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString humanKB(qint64 kb)
{
    double mb = kb / 1024.0;
    if (mb >= 1024.0) return QString::number(mb / 1024.0, 'f', 2) + " GB";
    return QString::number(mb, 'f', 0) + " MB";
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

    // Live pressure readout.
    auto *live = new QHBoxLayout;
    m_cpuLabel = new QLabel;
    m_ramLabel = new QLabel;
    live->addWidget(m_cpuLabel);
    live->addSpacing(18);
    live->addWidget(m_ramLabel);
    live->addStretch();
    m_refreshBtn = new QPushButton(tr("Refresh list"));
    live->addWidget(m_refreshBtn);
    root->addLayout(live);

    m_banner = new QLabel;
    m_banner->setWordWrap(true);
    m_banner->setObjectName("bannerText");
    m_banner->setVisible(false);
    root->addWidget(m_banner);

    // Candidate table.
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("Process"), tr("User"), tr("Memory"), tr("CPU %")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // Action row.
    auto *actions = new QHBoxLayout;
    m_suspendBtn = new QPushButton(tr("Relieve now (freeze selected)"));
    m_suspendBtn->setObjectName("primaryButton");
    m_resumeBtn  = new QPushButton(tr("Resume all"));
    m_dropCaches = new QCheckBox(tr("Also drop file caches (frees cached RAM, needs authorization)"));
    actions->addWidget(m_suspendBtn);
    actions->addWidget(m_resumeBtn);
    actions->addSpacing(16);
    actions->addWidget(m_dropCaches);
    actions->addStretch();
    root->addLayout(actions);

    // Automatic mode.
    auto *autoRow = new QHBoxLayout;
    m_autoCheck = new QCheckBox(tr("Automatic mode"));
    m_cpuThresh = new QSpinBox; m_cpuThresh->setRange(0, 100); m_cpuThresh->setValue(90); m_cpuThresh->setSuffix(tr(" % CPU"));
    m_ramThresh = new QSpinBox; m_ramThresh->setRange(0, 100); m_ramThresh->setValue(90); m_ramThresh->setSuffix(tr(" % RAM"));
    m_holdSecs  = new QSpinBox; m_holdSecs->setRange(2, 120);  m_holdSecs->setValue(8);  m_holdSecs->setSuffix(tr(" s"));
    autoRow->addWidget(m_autoCheck);
    autoRow->addSpacing(12);
    autoRow->addWidget(new QLabel(tr("when over")));
    autoRow->addWidget(m_cpuThresh);
    autoRow->addWidget(new QLabel(tr("or")));
    autoRow->addWidget(m_ramThresh);
    autoRow->addWidget(new QLabel(tr("for")));
    autoRow->addWidget(m_holdSecs);
    autoRow->addStretch();
    root->addLayout(autoRow);

    connect(m_refreshBtn, &QPushButton::clicked, this, &ReliefPage::refreshCandidates);
    connect(m_suspendBtn, &QPushButton::clicked, this, &ReliefPage::suspendSelected);
    connect(m_resumeBtn,  &QPushButton::clicked, this, &ReliefPage::resumeAll);
    connect(m_autoCheck,  &QCheckBox::toggled,   this, &ReliefPage::onAutoToggled);

    // One monitor drives both the live labels and the auto-mode watchdog. It is
    // tagged keepAlive so App::hideEvent leaves it running in the tray — the
    // whole point of auto relief is background protection.
    m_monitor = new QTimer(this);
    m_monitor->setObjectName("reliefMonitor");
    m_monitor->setProperty("keepAlive", true);
    m_monitor->setInterval(1000);
    connect(m_monitor, &QTimer::timeout, this, &ReliefPage::tickMonitor);
    m_monitor->start();

    refreshCandidates();
    updateLiveLabels();
    updateBanner();
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

void ReliefPage::refreshCandidates()
{
    const auto cands = ReliefTool::candidates();
    m_table->setRowCount(cands.size());
    for (int r = 0; r < cands.size(); ++r) {
        const auto &c = cands[r];

        auto *nameItem = new QTableWidgetItem(c.name);
        nameItem->setData(Qt::UserRole, c.pid);
        nameItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        // Pre-check the heavier processes for a quick one-click relieve, but
        // never anything currently frozen (those aren't candidates anyway).
        nameItem->setCheckState(c.memoryKB >= 150 * 1024 ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(r, 0, nameItem);

        m_table->setItem(r, 1, new QTableWidgetItem(c.user));
        auto *memItem = new QTableWidgetItem(humanKB(c.memoryKB));
        memItem->setData(Qt::UserRole, static_cast<qlonglong>(c.memoryKB));
        m_table->setItem(r, 2, memItem);
        m_table->setItem(r, 3, new QTableWidgetItem(QString::number(c.cpuPercent, 'f', 1)));
    }
    updateBanner();
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
    updateBanner();
    m_banner->setText(tr("Froze %1 process(es) to relieve pressure.%2 "
                         "Use “Resume all” to thaw them.").arg(frozen).arg(extra));
    m_banner->setVisible(true);
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
}

void ReliefPage::onAutoToggled(bool on)
{
    m_overSeconds = m_underSeconds = 0;
    if (on) {
        m_banner->setText(tr("Automatic mode is watching. Idle apps will be "
                             "frozen under sustained pressure and thawed when it clears."));
        m_banner->setVisible(true);
    }
}

void ReliefPage::tickMonitor()
{
    // Non-blocking CPU: delta between successive /proc/stat reads (no sleep).
    const CpuStat cur = CpuInfo::readStat();
    if (m_haveStat) m_lastCpu = CpuInfo::calcUsage(m_prevStat, cur);
    m_prevStat = cur;
    m_haveStat = true;

    updateLiveLabels();

    if (!m_autoCheck->isChecked()) return;

    const double ram = MemoryInfo::memory().ramPercent();
    const bool overCpu = m_cpuThresh->value() > 0 && m_lastCpu >= m_cpuThresh->value();
    const bool overRam = m_ramThresh->value() > 0 && ram        >= m_ramThresh->value();
    const bool over    = overCpu || overRam;
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
                refreshCandidates();
                m_banner->setText(tr("⚡ Auto relief: froze %1 idle app(s) under load.")
                                      .arg(pids.size()));
                m_banner->setVisible(true);
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
}

void ReliefPage::updateBanner()
{
    m_resumeBtn->setEnabled(!m_suspended.isEmpty());
    if (m_suspended.isEmpty() && !m_autoCheck->isChecked())
        m_banner->setVisible(false);
}
