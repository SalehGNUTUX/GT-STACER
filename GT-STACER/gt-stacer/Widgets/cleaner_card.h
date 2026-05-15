#pragma once
#include <QWidget>
#include <QPixmap>
#include <QString>

// A checkable icon-card used in the System Cleaner page. Visually mirrors the
// reference design (centered SVG glyph, label below, checkmark beneath that).
// Click anywhere on the card toggles the check state.
class CleanerCard : public QWidget {
    Q_OBJECT
public:
    explicit CleanerCard(const QString &iconSvg,
                          const QString &label,
                          const QString &description,
                          QWidget *parent = nullptr);

    void setChecked(bool on);
    bool isChecked() const { return m_checked; }

    void setSizeText(const QString &text); // shown under the label after scan
    QString label() const { return m_label; }

    // Visibly mark the card as supporting a drill-down dialog (double-click).
    // A small "ⓘ" badge is drawn in the top-right corner.
    void setHasDetails(bool on);
    bool hasDetails() const { return m_hasDetails; }

signals:
    void toggled(bool checked);
    void doubleClicked();    // categories that support drill-down listen for this

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    QSize sizeHint() const override { return {150, 175}; }

private:
    QPixmap renderIcon(int size, const QColor &color) const;

    QString  m_iconSvg;
    QString  m_label;
    QString  m_description;
    QString  m_sizeText;
    QPixmap  m_iconCache;
    bool     m_checked    = false;
    bool     m_hovered    = false;
    bool     m_hasDetails = false;
};
