#pragma once
#include <QWidget>
#include <QVector>
#include "../../../gt-stacer-core/Tools/firewall_tool.h"

class QTableWidget;
class QLabel;
class QPushButton;
class QSpinBox;
class QComboBox;

// Firewall management (26.08). Shows the host firewall state (ufw/firewalld),
// lists its rules, and lets the user enable/disable it and add or remove
// port rules. Reading the enabled state is cheap; listing and mutating rules
// each raise one polkit prompt, so rules load on demand rather than on a timer.
class FirewallPage : public QWidget {
    Q_OBJECT
public:
    explicit FirewallPage(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void refreshRules();     // pkexec: (re)load the rule list
    void toggleEnabled();
    void addRule();
    void deleteSelected();

private:
    void updateStatus();     // cheap enabled/disabled readout
    void populateTable();    // fill the table from m_rules

    QLabel       *m_status    = nullptr;
    QLabel       *m_badge     = nullptr;
    QPushButton  *m_toggleBtn = nullptr;
    QPushButton  *m_refreshBtn= nullptr;
    QTableWidget *m_table     = nullptr;
    QSpinBox     *m_port      = nullptr;
    QComboBox    *m_proto     = nullptr;
    QComboBox    *m_action    = nullptr;
    QPushButton  *m_addBtn    = nullptr;
    QPushButton  *m_delBtn    = nullptr;

    QVector<FwRule> m_rules;
};
