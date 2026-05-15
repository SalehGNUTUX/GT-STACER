#pragma once
#include <QWidget>
#include <QTimer>

namespace Ui { class ResourcesPage; }
class LineChart;

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
    void updateGpuSection();
    void updateTempSection();

    Ui::ResourcesPage *ui;
    QTimer            *m_timer = nullptr;

    // Series indices on our custom LineChart widgets.
    int m_cpuSeries  = -1;
    int m_memSeries  = -1;
    int m_netRxSeries = -1;
    int m_netTxSeries = -1;

    qint64 m_tick = 0;
    static constexpr int MAX_POINTS = 30;
};
