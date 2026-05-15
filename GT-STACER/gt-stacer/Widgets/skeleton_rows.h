#pragma once
#include <QWidget>
#include <QTimer>

// Renders N skeleton rows with a left-to-right shimmer animation. Used as a
// placeholder while package lists or other slow scans are loading. Cheap —
// nothing more than a QPainter + 60 ms repaint timer.
//
// Usage:
//     auto *skel = new SkeletonRows(8);
//     skel->start();
//     // …after data is ready:
//     skel->stop();
//     skel->hide();
class SkeletonRows : public QWidget {
    Q_OBJECT
public:
    explicit SkeletonRows(int rowCount = 8, QWidget *parent = nullptr);

    void setRowCount(int n);
    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent *e) override;
    QSize sizeHint() const override;

private:
    int     m_rowCount = 8;
    int     m_rowHeight = 38;
    int     m_rowGap   = 6;
    QTimer *m_timer    = nullptr;
    double  m_phase    = 0.0; // 0..1, advances each tick for the shimmer
};
