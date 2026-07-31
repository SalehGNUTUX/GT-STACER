#include "connections_page.h"
#include "../../Managers/setting_manager.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

ConnectionsPage::ConnectionsPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Network Connections"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto *intro = new QLabel(tr(
        "Every active TCP/UDP socket and the process that owns it. Processes for "
        "sockets you don't own are hidden unless you enable privileged view."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    // Toolbar: filter + count + auto-refresh + refresh.
    auto *bar = new QHBoxLayout;
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Filter by address, port, process or state…"));
    m_filter->setClearButtonEnabled(true);
    m_count = new QLabel;
    bar->addWidget(m_filter, 1);
    bar->addSpacing(12);
    bar->addWidget(m_count);
    bar->addStretch();
    m_autoRefresh = new QCheckBox(tr("Auto-refresh"));
    m_refreshSecs = new QSpinBox; m_refreshSecs->setRange(2, 120); m_refreshSecs->setSuffix(tr(" s"));
    m_refreshBtn  = new QPushButton(tr("Refresh"));
    bar->addWidget(m_autoRefresh);
    bar->addWidget(m_refreshSecs);
    bar->addSpacing(10);
    bar->addWidget(m_refreshBtn);
    root->addLayout(bar);

    // Table.
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Proto"), tr("State"), tr("Local Address"), tr("Peer Address"), tr("Process")});
    auto *hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(2, QHeaderView::Stretch);
    hh->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // Privileged view.
    auto *bottom = new QHBoxLayout;
    m_privileged = new QCheckBox(tr("Show processes of all users (needs authorization)"));
    bottom->addWidget(m_privileged);
    bottom->addStretch();
    root->addLayout(bottom);

    // Restore persisted preferences.
    m_loading = true;
    m_autoRefresh->setChecked(SettingManager::instance()->connAutoRefresh());
    m_refreshSecs->setValue(SettingManager::instance()->connRefreshSeconds());
    m_loading = false;

    connect(m_refreshBtn,  &QPushButton::clicked, this, &ConnectionsPage::refresh);
    connect(m_filter,      &QLineEdit::textChanged, this, &ConnectionsPage::applyFilter);
    connect(m_autoRefresh, &QCheckBox::toggled, this, &ConnectionsPage::onAutoToggled);
    connect(m_privileged,  &QCheckBox::toggled, this, &ConnectionsPage::onPrivilegedToggled);
    connect(m_refreshSecs, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        if (!m_loading) SettingManager::instance()->setConnRefreshSeconds(v);
        if (m_timer->isActive()) m_timer->start(v * 1000);
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ConnectionsPage::refresh);

    refresh();
}

void ConnectionsPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
}

void ConnectionsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
    applyTimerState();
}

void ConnectionsPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();   // don't poll ss while the user is on another page
}

void ConnectionsPage::applyTimerState()
{
    if (isVisible() && m_autoRefresh->isChecked())
        m_timer->start(m_refreshSecs->value() * 1000);
    else
        m_timer->stop();
}

void ConnectionsPage::onAutoToggled(bool on)
{
    if (!m_loading) SettingManager::instance()->setConnAutoRefresh(on);
    applyTimerState();
}

void ConnectionsPage::onPrivilegedToggled(bool)
{
    // Privileged refresh runs pkexec, which prompts every time — incompatible
    // with silent auto-refresh, so turn that off to avoid repeated prompts.
    if (m_privileged->isChecked() && m_autoRefresh->isChecked())
        m_autoRefresh->setChecked(false);
    refresh();
}

void ConnectionsPage::refresh()
{
    m_all = ConnectionInfo::list(m_privileged->isChecked());
    rebuildTable();
}

void ConnectionsPage::applyFilter()
{
    rebuildTable();
}

void ConnectionsPage::rebuildTable()
{
    const QString needle = m_filter->text().trimmed();
    const bool sortWas = m_table->isSortingEnabled();
    m_table->setSortingEnabled(false);
    m_table->setUpdatesEnabled(false);

    int row = 0;
    m_table->setRowCount(m_all.size());
    for (const auto &c : m_all) {
        const QString proc = c.pid > 0
            ? QStringLiteral("%1 (%2)").arg(c.process.isEmpty() ? tr("unknown") : c.process).arg(c.pid)
            : QStringLiteral("—");

        if (!needle.isEmpty()) {
            const QString hay = c.proto + ' ' + c.state + ' ' + c.localAddr + ' ' +
                                c.peerAddr + ' ' + proc;
            if (!hay.contains(needle, Qt::CaseInsensitive)) continue;
        }

        m_table->setItem(row, 0, new QTableWidgetItem(c.proto));
        m_table->setItem(row, 1, new QTableWidgetItem(c.state));
        m_table->setItem(row, 2, new QTableWidgetItem(c.localAddr));
        m_table->setItem(row, 3, new QTableWidgetItem(c.peerAddr));
        m_table->setItem(row, 4, new QTableWidgetItem(proc));
        ++row;
    }
    m_table->setRowCount(row);   // trim filtered-out tail

    m_table->setUpdatesEnabled(true);
    m_table->setSortingEnabled(sortWas);
    m_count->setText(tr("%1 connections").arg(row));
}
