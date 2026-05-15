#include "dashboard_page.h"
#include "../../Widgets/circular_gauge.h"
#include "../../Managers/info_manager.h"
#include "../../Managers/setting_manager.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Tools/service_tool.h"
#include "../../../gt-stacer-core/Info/system_info.h"
#include "../../../gt-stacer-core/Info/temperature_info.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>

static QFrame *makeSep()
{
    auto *f = new QFrame;
    f->setFrameShape(QFrame::VLine);
    f->setStyleSheet("background:#313244; border:none;");
    f->setFixedWidth(1);
    return f;
}

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent)
{
    buildUi();
    m_uptimeBase = SystemInfo::uptime();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(SettingManager::instance()->updateIntervalMs());
    connect(m_refreshTimer, &QTimer::timeout, this, &DashboardPage::refresh);
    m_refreshTimer->start();

    m_uptimeTimer = new QTimer(this);
    m_uptimeTimer->setInterval(1000);
    connect(m_uptimeTimer, &QTimer::timeout, this, &DashboardPage::tickUptime);
    m_uptimeTimer->start();

    refreshSystemInfo();
    refresh();
}

void DashboardPage::buildUi()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->addWidget(scroll);

    auto *w = new QWidget;
    scroll->setWidget(w);
    auto *root = new QVBoxLayout(w);
    root->setContentsMargins(24,20,24,24);
    root->setSpacing(16);

    // Title
    auto *titleLbl = new QLabel(tr("Dashboard"));
    titleLbl->setStyleSheet("font-size:22px;font-weight:bold;color:#cdd6f4;");
    root->addWidget(titleLbl);

    // ─── System Info Card ───────────────────────────────────────────
    auto *infoCard = new QFrame;
    infoCard->setObjectName("dashCard");
    // Card edges/shadow come from QSS (#dashCard) — avoid QGraphicsDropShadowEffect (expensive).
    auto *ig = new QGridLayout(infoCard);
    ig->setContentsMargins(20,16,20,16);
    ig->setHorizontalSpacing(28); ig->setVerticalSpacing(8);

    auto addCell = [&](int row, int col, const QString &key, QLabel **out){
        auto *k = new QLabel(key);
        k->setObjectName("infoKey");
        auto *v = new QLabel("—");
        v->setObjectName("infoValue");
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        ig->addWidget(k, row*2,   col);
        ig->addWidget(v, row*2+1, col);
        *out = v;
    };
    addCell(0,0,tr("Hostname"),       &m_hostname);
    addCell(0,1,tr("Operating System"),&m_os);
    addCell(0,2,tr("Kernel"),         &m_kernel);
    addCell(0,3,tr("Architecture"),   &m_arch);
    addCell(1,0,tr("Desktop"),        &m_desktop);
    addCell(1,1,tr("Display Server"), &m_display);
    addCell(1,2,tr("Init System"),    &m_initSys);
    addCell(1,3,tr("Uptime"),         &m_uptime);
    addCell(2,0,tr("Processor"),      &m_cpuModel);
    addCell(2,1,tr("CPU Frequency"),  &m_cpuFreq);
    addCell(2,2,tr("Total Memory"),   &m_totalRam);

    for (int c=0;c<4;c++) ig->setColumnStretch(c,1);
    root->addWidget(infoCard);

    // ─── Gauges Card ────────────────────────────────────────────────
    auto *gauCard = new QFrame;
    gauCard->setObjectName("dashCard");
    auto *gl = new QHBoxLayout(gauCard);
    gl->setContentsMargins(16,20,16,20);
    gl->setSpacing(4);

    auto mkGauge = [](const QString &lbl, QColor c) -> CircularGauge* {
        auto *g = new CircularGauge;
        g->setLabel(lbl); g->setAccentColor(c);
        g->setMinimumSize(130,155);
        return g;
    };
    m_cpuGauge  = mkGauge(tr("CPU"),  {0x89,0xb4,0xfa});
    m_ramGauge  = mkGauge(tr("RAM"),  {0xa6,0xe3,0xa1});
    m_diskGauge = mkGauge(tr("Disk"), {0xcb,0xa6,0xf7});
    m_swapGauge = mkGauge(tr("Swap"), {0xf9,0xe2,0xaf});

    gl->addWidget(m_cpuGauge,1);
    gl->addWidget(makeSep());
    gl->addWidget(m_ramGauge,1);
    gl->addWidget(makeSep());
    gl->addWidget(m_diskGauge,1);
    gl->addWidget(makeSep());
    gl->addWidget(m_swapGauge,1);
    root->addWidget(gauCard);

    // ─── Bottom Row: Network | GPU | Battery ────────────────────────
    auto *botRow = new QHBoxLayout;
    botRow->setSpacing(16);

    // Network
    auto *netCard = new QFrame;
    netCard->setObjectName("dashCard");
    auto *nl = new QVBoxLayout(netCard);
    nl->setContentsMargins(18,14,18,14); nl->setSpacing(8);
    auto *nt = new QLabel(tr("Network"));
    nt->setStyleSheet("font-size:14px;font-weight:bold;color:#89dceb;");
    nl->addWidget(nt);
    auto *ns = new QFrame; ns->setFrameShape(QFrame::HLine);
    ns->setStyleSheet("background:#313244;border:none;"); ns->setFixedHeight(1);
    nl->addWidget(ns);

    auto addNetRow = [&](const QString &k, QLabel **out){
        auto *h = new QHBoxLayout;
        auto *kl = new QLabel(k); kl->setObjectName("infoKey");
        *out = new QLabel("—"); (*out)->setObjectName("infoValue");
        (*out)->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(kl); h->addWidget(*out,1); nl->addLayout(h);
    };
    addNetRow(tr("Interface:"), &m_netIface);
    addNetRow(tr("IPv4:"),      &m_netIp);
    addNetRow(tr("↓ Download:"),&m_netRx);
    addNetRow(tr("↑ Upload:"),  &m_netTx);
    addNetRow(tr("⊟ Disk read:"),  &m_diskRead);
    addNetRow(tr("⊟ Disk write:"), &m_diskWrite);
    nl->addStretch();
    botRow->addWidget(netCard,1);

    // GPU
    m_gpuCard = new QFrame;
    m_gpuCard->setObjectName("dashCard");
    auto *gpuL = new QVBoxLayout(m_gpuCard);
    gpuL->setContentsMargins(18,14,18,14); gpuL->setSpacing(8);
    auto *gt2 = new QLabel(tr("GPU"));
    gt2->setStyleSheet("font-size:14px;font-weight:bold;color:#cba6f7;");
    gpuL->addWidget(gt2);
    auto *gs = new QFrame; gs->setFrameShape(QFrame::HLine);
    gs->setStyleSheet("background:#313244;border:none;"); gs->setFixedHeight(1);
    gpuL->addWidget(gs);

    auto addGpuRow = [&](const QString &k, QLabel **out){
        auto *h = new QHBoxLayout;
        auto *kl = new QLabel(k); kl->setObjectName("infoKey");
        *out = new QLabel("—"); (*out)->setObjectName("infoValue");
        h->addWidget(kl); h->addWidget(*out,1); gpuL->addLayout(h);
    };
    addGpuRow(tr("Model:"),    &m_gpuName);
    addGpuRow(tr("Usage:"),    &m_gpuUse);
    addGpuRow(tr("VRAM:"),     &m_gpuMem);
    addGpuRow(tr("Temp:"),     &m_gpuTmp);
    gpuL->addStretch();
    m_gpuCard->hide();
    botRow->addWidget(m_gpuCard,1);

    // Battery
    m_batCard = new QFrame;
    m_batCard->setObjectName("dashCard");
    auto *batL = new QVBoxLayout(m_batCard);
    batL->setContentsMargins(18,14,18,14); batL->setSpacing(8);
    auto *bt = new QLabel(tr("Battery"));
    bt->setStyleSheet("font-size:14px;font-weight:bold;color:#a6e3a1;");
    batL->addWidget(bt);
    auto *bs2 = new QFrame; bs2->setFrameShape(QFrame::HLine);
    bs2->setStyleSheet("background:#313244;border:none;"); bs2->setFixedHeight(1);
    batL->addWidget(bs2);

    m_batGauge = new CircularGauge;
    m_batGauge->setLabel(tr("Charge"));
    m_batGauge->setAccentColor({0xa6,0xe3,0xa1});
    m_batGauge->setMinimumSize(110,130);
    batL->addWidget(m_batGauge,0,Qt::AlignHCenter);

    m_batStatus = new QLabel("—");
    m_batStatus->setObjectName("infoValue");
    m_batStatus->setAlignment(Qt::AlignCenter);
    batL->addWidget(m_batStatus);
    m_batTime = new QLabel;
    m_batTime->setObjectName("infoKey");
    m_batTime->setAlignment(Qt::AlignCenter);
    batL->addWidget(m_batTime);
    batL->addStretch();
    m_batCard->hide();
    botRow->addWidget(m_batCard,1);

    root->addLayout(botRow);
    root->addStretch();
}

void DashboardPage::refreshSystemInfo()
{
    auto info = InfoManager::instance()->systemInfo();
    m_hostname->setText(info.hostname);
    m_os->setText(info.osName + " " + info.osVersion);
    m_kernel->setText(info.kernelVersion);
    m_arch->setText(info.architecture);
    m_desktop->setText(info.desktopEnv.isEmpty() ? "—" : info.desktopEnv);
    m_display->setText(info.displayServer);
    m_initSys->setText(ServiceTool::initSystemName());
    m_cpuModel->setText(info.cpuModel);
    m_totalRam->setText(FormatUtil::formatBytes(info.totalRamMB * 1024LL * 1024));
    m_uptimeBase = info.uptimeSeconds;
}

void DashboardPage::tickUptime()
{
    ++m_uptimeBase;
    // Re-sync from the kernel every 60s so we don't drift across suspend/resume.
    if (++m_uptimeTickCount >= 60) {
        m_uptimeTickCount = 0;
        qint64 fresh = SystemInfo::uptime();
        if (fresh > 0) m_uptimeBase = fresh;
    }
    qint64 s = m_uptimeBase;
    int d = s/86400; s%=86400;
    int h = s/3600;  s%=3600;
    int m = s/60;    s%=60;
    QString t;
    if (d>0) t = QString("%1d %2h %3m").arg(d).arg(h).arg(m);
    else if (h>0) t = QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    else t = QString("%1m %2s").arg(m).arg(s);
    if (m_uptime) m_uptime->setText(t);
}

void DashboardPage::refresh()
{
    refreshCpu(); refreshMemory(); refreshDisk();
    refreshNetwork(); refreshGpu(); refreshBattery();
}

void DashboardPage::refreshCpu()
{
    auto cpu = InfoManager::instance()->cpuUsage();
    m_cpuGauge->setValueAnimated(cpu.total);
    // Show CPU temperature under the gauge when sensors are available.
    if (auto t = TemperatureInfo::cpuTemperature(); t.has_value())
        m_cpuGauge->setSubText(FormatUtil::formatTemperature(*t));
    if (m_cpuFreq) m_cpuFreq->setText(FormatUtil::formatFrequency(cpu.freqMHz));
}

void DashboardPage::refreshMemory()
{
    auto mem = InfoManager::instance()->memory();
    m_ramGauge->setValueAnimated(mem.ramPercent());
    m_ramGauge->setSubText(FormatUtil::formatBytes(mem.usedRam) + " / " +
                            FormatUtil::formatBytes(mem.totalRam));
    if (mem.totalSwap > 0) {
        m_swapGauge->setValueAnimated(mem.swapPercent());
        m_swapGauge->setSubText(FormatUtil::formatBytes(mem.usedSwap) + " / " +
                                 FormatUtil::formatBytes(mem.totalSwap));
        m_swapGauge->show();
    } else { m_swapGauge->hide(); }
}

void DashboardPage::refreshDisk()
{
    auto parts = InfoManager::instance()->partitions();
    qint64 used=0, total=0;
    for (const auto &p : parts)
        if (p.mountPoint=="/") { used+=p.used; total+=p.total; }
    if (total>0) {
        m_diskGauge->setValueAnimated(used*100.0/total);
        m_diskGauge->setSubText(FormatUtil::formatBytes(used)+" / "+FormatUtil::formatBytes(total));
    }
}

void DashboardPage::refreshNetwork()
{
    auto ifaces = InfoManager::instance()->networks();
    bool linkUp = false;
    for (const auto &i : ifaces) {
        if (!i.isUp || i.ipv4.isEmpty()) continue;
        m_netIface->setText(i.name);
        m_netIp->setText(i.ipv4);
        m_netRx->setText(FormatUtil::formatSpeed(i.rxSpeed));
        m_netTx->setText(FormatUtil::formatSpeed(i.txSpeed));
        linkUp = true;
        break;
    }
    if (!linkUp) {
        m_netIface->setText(tr("No connection"));
        m_netIp->clear(); m_netRx->clear(); m_netTx->clear();
    }

    // Disk I/O is shown alongside network speeds — sum across physical disks.
    auto io = InfoManager::instance()->diskIo();
    qint64 rd = 0, wr = 0;
    for (const auto &d : io) { rd += d.readSpeed; wr += d.writeSpeed; }
    if (m_diskRead)  m_diskRead->setText(FormatUtil::formatSpeed(rd));
    if (m_diskWrite) m_diskWrite->setText(FormatUtil::formatSpeed(wr));
}

void DashboardPage::refreshGpu()
{
    auto gpus = InfoManager::instance()->gpus();
    if (gpus.isEmpty()) { m_gpuCard->hide(); return; }
    m_gpuCard->show();
    const auto &g = gpus.first();
    m_gpuName->setText(g.name.isEmpty() ? g.vendorName() : g.name);
    m_gpuUse->setText(g.usagePercent.has_value() ? FormatUtil::formatPercent(*g.usagePercent) : "—");
    if (g.memUsedMB.has_value() && g.memTotalMB.has_value())
        m_gpuMem->setText(QString("%1 / %2 MB").arg(*g.memUsedMB).arg(*g.memTotalMB));
    m_gpuTmp->setText(g.temperatureCelsius.has_value() ?
        FormatUtil::formatTemperature(*g.temperatureCelsius) : "—");
}

void DashboardPage::refreshBattery()
{
    auto bat = InfoManager::instance()->battery();
    if (!bat.present) { m_batCard->hide(); return; }
    m_batCard->show();
    m_batGauge->setValueAnimated(bat.percent);
    m_batStatus->setText(bat.statusString());
    if (bat.timeRemainingMin.has_value() && *bat.timeRemainingMin>0)
        m_batTime->setText(FormatUtil::formatDuration(*bat.timeRemainingMin));
    else m_batTime->clear();
}
