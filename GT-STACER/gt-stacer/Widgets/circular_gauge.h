#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QColor>

/* مقياس دائري متحرك — يُستخدم في Dashboard */
class CircularGauge : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
public:
    explicit CircularGauge(QWidget *parent = nullptr);

    double  value()             const { return m_value; }
    QString label()             const { return m_label; }
    QString unit()              const { return m_unit; }
    QString subText()           const { return m_subText; }

    void setLabel(const QString &label);
    void setUnit(const QString &unit);
    void setSubText(const QString &text);
    void setRange(double min, double max);
    void setAccentColor(const QColor &c);

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override;

public slots:
    void setValue(double v);
    void setValueAnimated(double v);

signals:
    void valueChanged(double v);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor arcColor() const;

    double m_value  = 0.0;
    double m_min    = 0.0;
    double m_max    = 100.0;
    QString m_label;
    QString m_unit  = "%";
    QString m_subText;
    QColor  m_accent{0x89, 0xb4, 0xfa};

    QPropertyAnimation *m_anim;
};
