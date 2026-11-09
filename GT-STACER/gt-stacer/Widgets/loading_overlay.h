#pragma once
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QElapsedTimer>

/* طبقة تحميل — بطاقة في وسط القطعة: حلقات نبض، قوس دوّار، عنوان وتفصيل */
// A dimmed backdrop (fades in) with a centred card: expanding pulse rings, a
// rotating gradient arc around a breathing core, the title, an optional detail
// line (e.g. the current step) and three bouncing dots. All colours come from
// Theme:: inside paintEvent, so it follows dark/light (26.12).
//
// It is the ONE waiting sign of the app: pages, tabs and dialogs all use it,
// with a title naming the service and a detail line for the step. It follows
// its host's size by itself (event filter), blocks clicks at once, but only
// becomes visible after a short delay — a wait that ends within it never
// flashes. start() while already showing only changes the texts.
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget *parent);
    // The overlay of `host` (a tab, a dialog …) — created on first use.
    static LoadingOverlay *on(QWidget *host);

    void start(const QString &message = "", const QString &detail = QString());
    void setMessage(const QString &message);
    void setDetail(const QString &detail);     // second line — the current step
    void stop();

protected:
    void paintEvent(QPaintEvent *event)     override;
    void resizeEvent(QResizeEvent *event)   override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void tick();

private:
    QRect cardRect() const;
    void  placeLabels();
    void  applyColors();

    QTimer  *m_timer;
    QLabel  *m_label;
    QLabel  *m_detail;
    QElapsedTimer m_clock;   // animation time
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
