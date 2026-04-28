#pragma once
#include <QDialog>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>

class WelcomeDialog : public QDialog {
    Q_OBJECT
public:
    explicit WelcomeDialog(QWidget *parent = nullptr);
    static bool shouldShow();    // true if first run
    static void markShown();

private slots:
    void nextPage();
    void prevPage();
    void updateButtons();

private:
    QWidget *makePage(const QString &icon, const QString &title,
                      const QString &desc, const QColor &accent);

    QStackedWidget *m_stack = nullptr;
    QPushButton    *m_next  = nullptr;
    QPushButton    *m_prev  = nullptr;
    QPushButton    *m_skip  = nullptr;
    QLabel         *m_dots  = nullptr;
};
