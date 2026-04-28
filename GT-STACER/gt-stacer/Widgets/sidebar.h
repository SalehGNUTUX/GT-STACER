#pragma once
#include <QWidget>
#include <QVector>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QLabel>

struct SidebarItem {
    QString iconSvg;   // SVG data as string
    QString label;
    QString tooltip;
};

class SidebarButton : public QWidget {
    Q_OBJECT
public:
    explicit SidebarButton(const SidebarItem &item, int index, QWidget *parent = nullptr);

    void setActive(bool active);
    void setCollapsed(bool collapsed);
    bool isActive() const { return m_active; }

signals:
    void clicked(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *)      override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    QSize sizeHint() const override;

private:
    SidebarItem m_item;
    int         m_index;
    bool        m_active    = false;
    bool        m_collapsed = false;
    bool        m_hovered   = false;
    bool        m_pressed   = false;
    QPixmap     m_icon;

    QPixmap renderIcon(int size, const QColor &color) const;
};

class Sidebar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth)
public:
    explicit Sidebar(QWidget *parent = nullptr);

    void addItem(const SidebarItem &item);
    void clearItems();          // مسح جميع أزرار التنقل (للترجمة)
    void setActiveIndex(int index);
    int  activeIndex() const { return m_activeIndex; }
    int  sidebarWidth() const { return m_width; }
    void setSidebarWidth(int w);

signals:
    void pageRequested(int index);

private slots:
    void onButtonClicked(int index);
    void toggleCollapse();

private:
    void updateCollapsedState();

    QVector<SidebarButton*> m_buttons;
    QPushButton            *m_toggleBtn           = nullptr;
    QLabel                 *m_logoLabel            = nullptr;
    QLabel                 *m_appName              = nullptr;
    QLabel                 *m_version              = nullptr;
    QWidget                *m_expandedHeaderWidget = nullptr;
    QWidget                *m_collapsedHeaderWidget= nullptr;
    QLabel                 *m_footerLabel          = nullptr;

    int  m_activeIndex  = 0;
    int  m_width        = 220;
    bool m_collapsed    = false;

    QPropertyAnimation *m_anim;

    static constexpr int EXPANDED_W  = 220;
    static constexpr int COLLAPSED_W = 64;
};
