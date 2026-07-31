#include "firewall_page.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

FirewallPage::FirewallPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Firewall"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto *intro = new QLabel(tr(
        "Control the host firewall: turn it on or off and manage which ports are "
        "allowed. Listing and changing rules asks for authorization."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    // Status + controls.
    auto *statusRow = new QHBoxLayout;
    m_status = new QLabel;
    m_status->setObjectName("infoValue");
    m_toggleBtn  = new QPushButton;
    m_refreshBtn = new QPushButton(tr("Load rules"));
    statusRow->addWidget(m_status);
    statusRow->addStretch();
    statusRow->addWidget(m_toggleBtn);
    statusRow->addWidget(m_refreshBtn);
    root->addLayout(statusRow);

    // Rules table.
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("#"), tr("To"), tr("Action"), tr("From")});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // Add-rule row.
    auto *addRow = new QHBoxLayout;
    addRow->addWidget(new QLabel(tr("Port")));
    m_port = new QSpinBox; m_port->setRange(1, 65535); m_port->setValue(8080);
    addRow->addWidget(m_port);
    m_proto = new QComboBox; m_proto->addItems({"tcp", "udp"});
    addRow->addWidget(m_proto);
    m_action = new QComboBox;
    m_action->addItem(tr("Allow"), "allow");
    m_action->addItem(tr("Deny"),  "deny");
    addRow->addWidget(m_action);
    m_addBtn = new QPushButton(tr("Add rule"));
    m_addBtn->setObjectName("primaryButton");
    addRow->addWidget(m_addBtn);
    addRow->addSpacing(16);
    m_delBtn = new QPushButton(tr("Delete selected"));
    m_delBtn->setObjectName("dangerButton");
    addRow->addWidget(m_delBtn);
    addRow->addStretch();
    root->addLayout(addRow);

    connect(m_toggleBtn,  &QPushButton::clicked, this, &FirewallPage::toggleEnabled);
    connect(m_refreshBtn, &QPushButton::clicked, this, &FirewallPage::refreshRules);
    connect(m_addBtn,     &QPushButton::clicked, this, &FirewallPage::addRule);
    connect(m_delBtn,     &QPushButton::clicked, this, &FirewallPage::deleteSelected);

    if (FirewallTool::backend() == FirewallTool::None) {
        m_status->setText(tr("No supported firewall found. Install ufw or firewalld."));
        m_toggleBtn->setEnabled(false);
        m_refreshBtn->setEnabled(false);
        m_addBtn->setEnabled(false);
        m_delBtn->setEnabled(false);
        return;
    }
    updateStatus();
}

void FirewallPage::updateStatus()
{
    if (FirewallTool::backend() == FirewallTool::None) return;
    const bool on = FirewallTool::isEnabled();
    m_status->setText(tr("Firewall: %1 — %2")
        .arg(FirewallTool::backendName(), on ? tr("Enabled") : tr("Disabled")));
    m_toggleBtn->setText(on ? tr("Disable") : tr("Enable"));
    // Adding/deleting only makes sense once the firewall is on.
    m_addBtn->setEnabled(on);
    m_delBtn->setEnabled(on);
}

void FirewallPage::populateTable()
{
    m_table->setRowCount(m_rules.size());
    for (int r = 0; r < m_rules.size(); ++r) {
        const FwRule &rule = m_rules.at(r);
        m_table->setItem(r, 0, new QTableWidgetItem(
            rule.number > 0 ? QString::number(rule.number) : QStringLiteral("—")));
        m_table->setItem(r, 1, new QTableWidgetItem(rule.to));
        m_table->setItem(r, 2, new QTableWidgetItem(rule.action));
        m_table->setItem(r, 3, new QTableWidgetItem(rule.from));
    }
}

void FirewallPage::toggleEnabled()
{
    const bool wantOn = !FirewallTool::isEnabled();
    m_rules = FirewallTool::setEnabled(wantOn);   // one prompt: toggle + re-list
    populateTable();
    updateStatus();
}

void FirewallPage::refreshRules()
{
    m_rules = FirewallTool::rules();
    populateTable();
    updateStatus();
}

void FirewallPage::addRule()
{
    const bool allow = m_action->currentData().toString() == "allow";
    m_rules = FirewallTool::addRule(m_port->value(), m_proto->currentText(), allow);
    populateTable();
    updateStatus();
}

void FirewallPage::deleteSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_rules.size()) {
        QMessageBox::information(this, tr("Firewall"), tr("Select a rule to delete first."));
        return;
    }
    const FwRule rule = m_rules.at(row);
    if (QMessageBox::question(this, tr("Delete rule"),
            tr("Delete rule: %1 %2 (%3)?").arg(rule.to, rule.action, rule.from))
        != QMessageBox::Yes)
        return;
    m_rules = FirewallTool::deleteRule(rule);   // one prompt: delete + re-list
    populateTable();
    updateStatus();
}

void FirewallPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
}

void FirewallPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateStatus();   // cheap; rules stay until the user asks to load them
}
