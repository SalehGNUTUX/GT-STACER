#pragma once
#include <QWidget>
#include <QVector>
#include "../../../gt-stacer-core/Info/connection_info.h"

class QTableWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QTimer;

// Live network connections — a friendly front-end for `ss -tunap`. Lists active
// TCP/UDP sockets with their owning process, a text filter, manual refresh and
// an optional auto-refresh. A privileged toggle reveals processes owned by other
// users. Part of the 26.08 "network & power tools" set.
class ConnectionsPage : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionsPage(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void refresh();          // re-run ss and rebuild
    void applyFilter();      // filter the already-fetched list in place
    void onAutoToggled(bool on);
    void onPrivilegedToggled(bool on);

private:
    void rebuildTable();
    void applyTimerState();  // run the auto-refresh timer only while visible

    QTableWidget *m_table       = nullptr;
    QLineEdit    *m_filter      = nullptr;
    QLabel       *m_count       = nullptr;
    QPushButton  *m_refreshBtn  = nullptr;
    QCheckBox    *m_autoRefresh = nullptr;
    QSpinBox     *m_refreshSecs = nullptr;
    QCheckBox    *m_privileged  = nullptr;
    QTimer       *m_timer       = nullptr;

    QVector<Connection> m_all;   // last fetched, unfiltered
    bool m_loading = false;
};
