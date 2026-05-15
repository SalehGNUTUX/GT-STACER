#pragma once
#include <QWidget>
#include <QColor>
#include <QString>
#include <QVector>

// Lightweight QPainter-based line chart used in place of QChartView.
//
// Why custom: Qt6Charts links libQt6Charts which transitively pulls in 8+ QML
// libraries (~30 MB shared memory). For a 4-series, 30-point chart that we
// only ever update in place, that overhead is hard to justify.
//
// Capabilities:
//   - Multiple line series, one color each.
//   - Auto-scaling Y axis (peak * 1.15, snapped to round numbers) OR fixed range.
//   - Optional Y label formatter (e.g. "%1 KB/s").
//   - Optional legend underneath the plot.
//   - Optional horizontal grid lines (4 by default).
class LineChart : public QWidget {
    Q_OBJECT
public:
    explicit LineChart(QWidget *parent = nullptr);

    int  addSeries(const QString &name, const QColor &color);
    void appendPoint(int seriesIdx, double value);

    // Fixed Y range (disables auto-scale until setAutoScaleY() is called).
    void setYRange(double min, double max);
    // Recompute the Y range from current data each repaint.
    void setAutoScaleY(double minRange = 32.0); // ensures axis never collapses to a thin line

    void setMaxPoints(int n);                 // history length per series
    void setShowLegend(bool show);
    void setYLabelSuffix(const QString &s);   // appended to axis tick labels
    void setBackgroundColor(const QColor &c);
    void setGridColor(const QColor &c);
    void setAxisLabelColor(const QColor &c);

protected:
    void paintEvent(QPaintEvent *e) override;
    QSize sizeHint() const override { return {240, 140}; }

private:
    struct Series {
        QString name;
        QColor  color;
        QVector<double> data; // ring of size <= m_maxPoints
    };

    double effectiveYMax() const;

    QVector<Series> m_series;
    int     m_maxPoints     = 30;
    bool    m_autoScaleY    = true;
    double  m_autoScaleMin  = 32.0;
    double  m_yMin          = 0.0;
    double  m_yMax          = 100.0;
    bool    m_showLegend    = false;
    QString m_ySuffix;

    QColor m_bgColor    = QColor(24, 24, 37, 0);   // transparent over card
    QColor m_gridColor  = QColor(49, 50, 68, 180);
    QColor m_labelColor = QColor(166, 173, 200);
};
