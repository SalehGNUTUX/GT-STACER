#include "processes_page.h"
#include "ui_processes_page.h"
#include "../../Managers/info_manager.h"
#include "../../Managers/setting_manager.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Info/process_info.h"
#include <QAction>
#include <QHash>
#include <QHeaderView>
#include <QHideEvent>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QShowEvent>

namespace {
// Custom proxy: searches across PID + name + cmdline + user simultaneously,
// case-insensitive. Cmdline is stashed on the Name item as Qt::UserRole+10
// (chosen above the existing UserRole + UserRole+1 uses for numeric hints).
constexpr int CMDLINE_ROLE = Qt::UserRole + 10;

class ProcSearchProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setQuery(const QString &q) { m_query = q.trimmed(); invalidateFilter(); }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        if (m_query.isEmpty()) return true;
        const QAbstractItemModel *m = sourceModel();
        if (!m) return true;
        auto val = [&](int col, int role = Qt::DisplayRole) -> QString {
            return m->index(row, col, parent).data(role).toString();
        };
        const QString pid  = val(0);
        const QString name = val(1);
        const QString cmd  = m->index(row, 1, parent).data(CMDLINE_ROLE).toString();
        const QString user = val(2);
        return pid.contains(m_query, Qt::CaseInsensitive)
            || name.contains(m_query, Qt::CaseInsensitive)
            || cmd.contains(m_query, Qt::CaseInsensitive)
            || user.contains(m_query, Qt::CaseInsensitive);
    }
private:
    QString m_query;
};
} // namespace

ProcessesPage::ProcessesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::ProcessesPage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 5, this);
    m_model->setHorizontalHeaderLabels({tr("PID"), tr("Name"), tr("User"), tr("CPU %"), tr("Memory")});

    auto *proxy = new ProcSearchProxy(this);
    proxy->setSourceModel(m_model);
    m_proxy = proxy;

    ui->processTable->setModel(m_proxy);
    ui->processTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->processTable->setSortingEnabled(true);

    // Inform users they can search by PID / cmdline too.
    ui->searchEdit->setPlaceholderText(tr("Search by name, PID, command, or user…"));

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProcessesPage::refresh);
    connect(ui->killButton, &QPushButton::clicked, this, &ProcessesPage::killSelected);
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &ProcessesPage::onSearchChanged);

    // Right-click context menu surfaces signal/nice operations without
    // cluttering the toolbar.
    ui->processTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->processTable, &QWidget::customContextMenuRequested,
            this, &ProcessesPage::onContextMenu);

    refresh();
    m_timer->start(SettingManager::instance()->updateIntervalMs());
}

ProcessesPage::~ProcessesPage() { delete ui; }

void ProcessesPage::refresh()
{
    auto procs = InfoManager::instance()->processes();

    // In-place merge — preserves selection, scrollbar position, sort state.
    QHash<int, int> rowByPid;
    rowByPid.reserve(m_model->rowCount());
    for (int r = 0; r < m_model->rowCount(); ++r)
        rowByPid.insert(m_model->item(r, 0)->text().toInt(), r);

    QSet<int> seenPids;
    seenPids.reserve(procs.size());

    for (const auto &p : procs) {
        seenPids.insert(p.pid);
        const QString cpuTxt = FormatUtil::formatPercent(p.cpuPercent);
        const QString memTxt = FormatUtil::formatBytes(p.memoryKB * 1024);

        auto it = rowByPid.find(p.pid);
        if (it == rowByPid.end()) {
            QList<QStandardItem*> row;
            row << new QStandardItem(QString::number(p.pid))
                << new QStandardItem(p.name)
                << new QStandardItem(p.user)
                << new QStandardItem(cpuTxt)
                << new QStandardItem(memTxt);
            row[0]->setData(p.pid,        Qt::UserRole);
            row[1]->setData(p.command,    CMDLINE_ROLE);  // searchable cmdline
            row[1]->setToolTip(p.command);
            row[3]->setData(p.cpuPercent, Qt::UserRole);
            row[4]->setData(p.memoryKB,   Qt::UserRole);
            m_model->appendRow(row);
        } else {
            int r = it.value();
            if (m_model->item(r, 1)->text() != p.name) m_model->item(r, 1)->setText(p.name);
            if (m_model->item(r, 2)->text() != p.user) m_model->item(r, 2)->setText(p.user);
            if (m_model->item(r, 3)->text() != cpuTxt) m_model->item(r, 3)->setText(cpuTxt);
            if (m_model->item(r, 4)->text() != memTxt) m_model->item(r, 4)->setText(memTxt);
            m_model->item(r, 1)->setData(p.command, CMDLINE_ROLE);
            m_model->item(r, 1)->setToolTip(p.command);
            m_model->item(r, 3)->setData(p.cpuPercent, Qt::UserRole);
            m_model->item(r, 4)->setData(p.memoryKB,    Qt::UserRole);
        }
    }

    for (int r = m_model->rowCount() - 1; r >= 0; --r) {
        int pid = m_model->item(r, 0)->text().toInt();
        if (!seenPids.contains(pid)) m_model->removeRow(r);
    }

    ui->countLabel->setText(tr("%1 processes").arg(procs.size()));
}

QPair<int, QString> ProcessesPage::currentSelection() const
{
    auto idx = ui->processTable->currentIndex();
    if (!idx.isValid()) return {0, QString()};
    int pid      = m_proxy->data(m_proxy->index(idx.row(), 0)).toInt();
    QString name = m_proxy->data(m_proxy->index(idx.row(), 1)).toString();
    return {pid, name};
}

void ProcessesPage::killSelected()
{
    runActionOnSelected(Action::Terminate);
}

void ProcessesPage::runActionOnSelected(Action a)
{
    const auto [pid, name] = currentSelection();
    if (pid <= 0) return;

    const bool critical = ProcessInfo::isCriticalProcess(pid, name);

    QString verb;
    switch (a) {
    case Action::Terminate:  verb = tr("terminate");          break;
    case Action::ForceKill:  verb = tr("force-kill");         break;
    case Action::Suspend:    verb = tr("suspend");            break;
    case Action::Resume:     verb = tr("resume");             break;
    case Action::ReniceLow:  verb = tr("lower priority of");  break;
    case Action::ReniceHigh: verb = tr("raise priority of");  break;
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Confirm action"));
    box.setIcon(critical ? QMessageBox::Critical : QMessageBox::Warning);
    QString body = tr("About to <b>%1</b> process <b>'%2'</b> (PID %3).").arg(verb, name).arg(pid);
    if (critical) {
        body += "<br><br>" + tr(
            "⚠ <span style='color:#f38ba8;'><b>This is a critical system process.</b></span> "
            "Ending it can hang your session, log you out, or require a reboot. "
            "Continue only if you understand the impact.");
    }
    box.setTextFormat(Qt::RichText);
    box.setText(body);
    auto *go = box.addButton(critical ? tr("Yes, I understand the risk") : tr("Yes"),
                              QMessageBox::AcceptRole);
    box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != go) return;

    bool ok = false;
    switch (a) {
    case Action::Terminate: ok = ProcessInfo::kill(pid);      break;
    case Action::ForceKill: ok = ProcessInfo::forceKill(pid); break;
    case Action::Suspend:   ok = ProcessInfo::suspend(pid);   break;
    case Action::Resume:    ok = ProcessInfo::resume(pid);    break;
    case Action::ReniceLow: {
        bool oki = false;
        int n = QInputDialog::getInt(this, tr("Lower priority"),
            tr("Niceness for '%1' (higher = lower priority, 1..19):").arg(name),
            10, 1, 19, 1, &oki);
        if (!oki) return;
        ok = ProcessInfo::setPriority(pid, n);
        break;
    }
    case Action::ReniceHigh: {
        bool oki = false;
        int n = QInputDialog::getInt(this, tr("Raise priority"),
            tr("Niceness for '%1' (lower = higher priority, -20..-1).\n"
               "Requires root privileges.").arg(name),
            -5, -20, -1, 1, &oki);
        if (!oki) return;
        ok = ProcessInfo::setPriority(pid, n);
        break;
    }
    }

    if (!ok)
        QMessageBox::warning(this, tr("Error"),
            tr("Operation failed for PID %1. You may not own this process.").arg(pid));
    refresh();
}

void ProcessesPage::onContextMenu(const QPoint &pos)
{
    auto idx = ui->processTable->indexAt(pos);
    if (!idx.isValid()) return;

    QMenu menu(this);
    auto *terminate = menu.addAction(tr("Terminate (SIGTERM)"));
    auto *forceKill = menu.addAction(tr("Force kill (SIGKILL)"));
    menu.addSeparator();
    auto *suspend   = menu.addAction(tr("Suspend (SIGSTOP)"));
    auto *resume    = menu.addAction(tr("Resume (SIGCONT)"));
    menu.addSeparator();
    auto *lower     = menu.addAction(tr("Lower priority…"));
    auto *raise     = menu.addAction(tr("Raise priority…"));

    QAction *picked = menu.exec(ui->processTable->viewport()->mapToGlobal(pos));
    if      (picked == terminate) runActionOnSelected(Action::Terminate);
    else if (picked == forceKill) runActionOnSelected(Action::ForceKill);
    else if (picked == suspend)   runActionOnSelected(Action::Suspend);
    else if (picked == resume)    runActionOnSelected(Action::Resume);
    else if (picked == lower)     runActionOnSelected(Action::ReniceLow);
    else if (picked == raise)     runActionOnSelected(Action::ReniceHigh);
}

void ProcessesPage::onSearchChanged(const QString &text)
{
    static_cast<ProcSearchProxy*>(m_proxy)->setQuery(text);
}

void ProcessesPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}

void ProcessesPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();   // fresh data immediately, then poll while visible
    m_timer->start(SettingManager::instance()->updateIntervalMs());
}

void ProcessesPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();   // don't scan /proc while the user is on another page
}
