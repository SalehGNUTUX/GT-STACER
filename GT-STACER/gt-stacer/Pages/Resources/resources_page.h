#pragma once
#include <QWidget>
#include <QTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QVector>

namespace Ui { class ResourcesPage; }

class ResourcesPage : public QWidget {
    Q_OBJECT
public:
    explicit ResourcesPage(QWidget *parent = nullptr);
    ~ResourcesPage() override;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void refresh();

private:
    void setupCharts();
    void updateCpuChart(double usage);
    void updateMemChart(double usage);
    void updateGpuSection();
    void updateTempSection();

    Ui::ResourcesPage    *ui;
    QTimer               *m_timer;
    QLineSeries          *m_cpuSeries;
    QLineSeries          *m_memSeries;
    QLineSeries          *m_netRxSeries;
    QLineSeries          *m_netTxSeries;
    qint64               m_tick = 0;
    static constexpr int MAX_POINTS = 60;
};
