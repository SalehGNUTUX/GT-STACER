#pragma once
#include <QWidget>
#include <QTimer>
#include <QLabel>

/* طبقة تحميل شفافة — تعرض Spinner فوق أي قطعة */
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget *parent);

    void start(const QString &message = "");
    void stop();

protected:
    void paintEvent(QPaintEvent *event)     override;
    void resizeEvent(QResizeEvent *event)   override;

private slots:
    void tick();

private:
    QTimer  *m_timer;
    QLabel  *m_label;
    int      m_angle = 0;
};

/* ─── Helper: سبينر صغير لأعمدة الجداول ─── */
class SpinnerButton : public QWidget {
    Q_OBJECT
public:
    explicit SpinnerButton(QWidget *parent = nullptr);
    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent *) override;
    QSize sizeHint() const override { return {24, 24}; }

private slots:
    void tick();

private:
    QTimer *m_timer;
    int     m_angle = 0;
    bool    m_running = false;
};
