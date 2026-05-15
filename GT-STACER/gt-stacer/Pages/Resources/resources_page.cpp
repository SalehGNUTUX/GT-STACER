#include "resources_page.h"
#include "ui_resources_page.h"
#include "../../Managers/info_manager.h"
#include "../../Managers/setting_manager.h"
#include "../../Widgets/line_chart.h"
#include "../../Widgets/per_core_bars.h"
#include "../../../gt-stacer-core/Utils/format_util.h"

ResourcesPage::ResourcesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::ResourcesPage)
{
    ui->setupUi(this);
    setupCharts();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ResourcesPage::refresh);
    refresh();
    m_timer->start(SettingManager::instance()->updateIntervalMs());
}

ResourcesPage::~ResourcesPage() { delete ui; }

void ResourcesPage::setupCharts()
{
    // CPU and Memory: 0..100 percent, no legend, simple single series.
    ui->cpuChartView->setMaxPoints(MAX_POINTS);
    ui->cpuChartView->setYRange(0, 100);
    ui->cpuChartView->setYLabelSuffix("%");
    m_cpuSeries = ui->cpuChartView->addSeries(tr("CPU"), QColor("#89b4fa"));

    ui->memChartView->setMaxPoints(MAX_POINTS);
    ui->memChartView->setYRange(0, 100);
    ui->memChartView->setYLabelSuffix("%");
    m_memSeries = ui->memChartView->addSeries(tr("Memory"), QColor("#a6e3a1"));

    // Network: two series sharing one chart, auto-scaling Y axis in KB/s.
    ui->netChartView->setMaxPoints(MAX_POINTS);
    ui->netChartView->setAutoScaleY(32.0);
    ui->netChartView->setYLabelSuffix("KB/s");
    ui->netChartView->setShowLegend(true);
    m_netRxSeries = ui->netChartView->addSeries(tr("Download"), QColor("#89dceb"));
    m_netTxSeries = ui->netChartView->addSeries(tr("Upload"),   QColor("#f38ba8"));
}

void ResourcesPage::refresh()
{
    ++m_tick;

    // CPU
    auto cpu = InfoManager::instance()->cpuUsage();
    ui->cpuChartView->appendPoint(m_cpuSeries, cpu.total);
    ui->cpuUsageLabel->setText(FormatUtil::formatPercent(cpu.total));
    ui->cpuModelLabel->setText(cpu.model);
    ui->cpuFreqLabel->setText(FormatUtil::formatFrequency(cpu.freqMHz));
    ui->perCoreBars->setValues(cpu.perCore);

    // Memory
    auto mem = InfoManager::instance()->memory();
    ui->memChartView->appendPoint(m_memSeries, mem.ramPercent());
    ui->ramUsageLabel->setText(FormatUtil::formatBytes(mem.usedRam) + " / " + FormatUtil::formatBytes(mem.totalRam));
    ui->swapUsageLabel->setText(FormatUtil::formatBytes(mem.usedSwap) + " / " + FormatUtil::formatBytes(mem.totalSwap));

    // Network — values in KB/s on a shared auto-scaled axis.
    auto ifaces = InfoManager::instance()->networks();
    double rxKB = 0, txKB = 0;
    for (const auto &i : ifaces) {
        if (!i.isUp || i.ipv4.isEmpty()) continue;
        rxKB = i.rxSpeed / 1024.0;
        txKB = i.txSpeed / 1024.0;
        break;
    }
    ui->netChartView->appendPoint(m_netRxSeries, rxKB);
    ui->netChartView->appendPoint(m_netTxSeries, txKB);

    updateGpuSection();
    updateTempSection();
}

void ResourcesPage::updateGpuSection()
{
    auto gpus = InfoManager::instance()->gpus();
    if (gpus.isEmpty()) { ui->gpuGroupBox->hide(); return; }

    ui->gpuGroupBox->show();
    const auto &g = gpus.first();
    ui->gpuNameLabel->setText(g.name);
    if (g.usagePercent.has_value()) {
        ui->gpuUsageBar->setValue(*g.usagePercent);
        ui->gpuUsageLabel->setText(FormatUtil::formatPercent(*g.usagePercent));
    }
    if (g.memUsedMB.has_value() && g.memTotalMB.has_value())
        ui->gpuMemLabel->setText(QString("%1 MB / %2 MB").arg(*g.memUsedMB).arg(*g.memTotalMB));
    if (g.temperatureCelsius.has_value())
        ui->gpuTempLabel->setText(FormatUtil::formatTemperature(*g.temperatureCelsius));
}

void ResourcesPage::updateTempSection()
{
    auto sensors = InfoManager::instance()->sensors();
    if (sensors.isEmpty()) { ui->tempGroupBox->hide(); return; }

    ui->tempGroupBox->show();
    QString text;
    int count = 0;
    for (const auto &s : sensors) {
        text += QString("%1: %2\n").arg(s.label).arg(FormatUtil::formatTemperature(s.current));
        if (++count >= 6) break;
    }
    ui->tempLabel->setText(text.trimmed());
}

void ResourcesPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
