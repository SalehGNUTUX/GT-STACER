#pragma once
#include <QWidget>
#include <QVector>

// Horizontal mini bar-chart that visualizes per-core CPU utilisation.
// One thin bar per core, colored by load: green → yellow → orange → red.
//
// QPainter-based to avoid pulling QtCharts back in.
class PerCoreBars : public QWidget {
    Q_OBJECT
public:
    explicit PerCoreBars(QWidget *parent = nullptr);

    void setValues(const QVector<double> &percents); // 0..100 per core

protected:
    void paintEvent(QPaintEvent *e) override;
    QSize sizeHint() const override { return {200, 90}; }
    QSize minimumSizeHint() const override { return {100, 70}; }

private:
    QVector<double> m_values;
};
