#include "resources_page.h"
#include "ui_resources_page.h"
#include "../../Managers/info_manager.h"
#include "../../Managers/setting_manager.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

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
    auto makeChart = [this](const QString &title, const QColor &color) {
        auto *series = new QLineSeries;
        series->setColor(color);
        for (int i = 0; i < MAX_POINTS; ++i) series->append(i, 0);

        auto *chart = new QChart;
        chart->addSeries(series);
        chart->setTitle(title);
        chart->setBackgroundBrush(Qt::transparent);
        chart->legend()->hide();
        chart->setMargins(QMargins(0,0,0,0));

        auto *axisX = new QValueAxis; axisX->setRange(0, MAX_POINTS); axisX->setVisible(false);
        auto *axisY = new QValueAxis; axisY->setRange(0, 100);
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisX);
        series->attachAxis(axisY);

        return series;
    };

    m_cpuSeries   = makeChart(tr("CPU"),      QColor("#89b4fa"));
    m_memSeries   = makeChart(tr("Memory"),   QColor("#a6e3a1"));
    m_netRxSeries = makeChart(tr("Net RX"),   QColor("#89dceb"));
    m_netTxSeries = makeChart(tr("Net TX"),   QColor("#f38ba8"));

    ui->cpuChartView->setChart(m_cpuSeries->chart());
    ui->memChartView->setChart(m_memSeries->chart());
    ui->netChartView->setChart(m_netRxSeries->chart());
    ui->cpuChartView->setRenderHint(QPainter::Antialiasing);
    ui->memChartView->setRenderHint(QPainter::Antialiasing);
    ui->netChartView->setRenderHint(QPainter::Antialiasing);
}

static void appendPoint(QLineSeries *series, double val, int maxPoints)
{
    auto pts = series->points();
    for (int i = 0; i < pts.size() - 1; ++i)
        series->replace(i, i, pts[i + 1].y());
    series->replace(maxPoints - 1, maxPoints - 1, val);
}

void ResourcesPage::refresh()
{
    ++m_tick;

    // CPU
    auto cpu = InfoManager::instance()->cpuUsage();
    appendPoint(m_cpuSeries, cpu.total, MAX_POINTS);
    ui->cpuUsageLabel->setText(FormatUtil::formatPercent(cpu.total));
    ui->cpuModelLabel->setText(cpu.model);
    ui->cpuFreqLabel->setText(FormatUtil::formatFrequency(cpu.freqMHz));

    // Memory
    auto mem = InfoManager::instance()->memory();
    appendPoint(m_memSeries, mem.ramPercent(), MAX_POINTS);
    ui->ramUsageLabel->setText(FormatUtil::formatBytes(mem.usedRam) + " / " + FormatUtil::formatBytes(mem.totalRam));
    ui->swapUsageLabel->setText(FormatUtil::formatBytes(mem.usedSwap) + " / " + FormatUtil::formatBytes(mem.totalSwap));

    // GPU
    updateGpuSection();

    // Temperature
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
    // Show the first few sensors
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
