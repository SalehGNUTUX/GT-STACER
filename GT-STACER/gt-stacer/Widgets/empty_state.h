#pragma once
#include <QWidget>

class QLabel;
class QPushButton;

// Reusable "nothing-to-show-here" placeholder used by Uninstaller, APT
// Sources, Processes (filtered), etc. instead of leaving an empty table.
// Lays out: optional SVG glyph, title, subtitle, and a single action button
// (also optional).
class EmptyState : public QWidget {
    Q_OBJECT
public:
    explicit EmptyState(QWidget *parent = nullptr);

    void setIconSvg(const QString &svg, int sizePx = 56);
    void setTitle(const QString &t);
    void setSubtitle(const QString &s);

    // Show or hide the action button. Connect via actionClicked().
    void setActionLabel(const QString &label);
    void clearAction();

signals:
    void actionClicked();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QString  m_iconSvg;
    int      m_iconSize = 56;

    QLabel      *m_titleLbl  = nullptr;
    QLabel      *m_subLbl    = nullptr;
    QPushButton *m_actionBtn = nullptr;
};
