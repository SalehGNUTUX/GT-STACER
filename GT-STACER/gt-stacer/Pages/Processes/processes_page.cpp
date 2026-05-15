#include "processes_page.h"
#include "ui_processes_page.h"
#include "../../Managers/info_manager.h"
#include "../../Managers/setting_manager.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Info/process_info.h"
#include <QMessageBox>
#include <QHash>
#include <QHeaderView>

ProcessesPage::ProcessesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::ProcessesPage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 5, this);
    m_model->setHorizontalHeaderLabels({tr("PID"), tr("Name"), tr("User"), tr("CPU %"), tr("Memory")});

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(1);

    ui->processTable->setModel(m_proxy);
    ui->processTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->processTable->setSortingEnabled(true);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProcessesPage::refresh);
    connect(ui->killButton, &QPushButton::clicked, this, &ProcessesPage::killSelected);
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &ProcessesPage::onSearchChanged);

    refresh();
    m_timer->start(SettingManager::instance()->updateIntervalMs());
}

ProcessesPage::~ProcessesPage() { delete ui; }

void ProcessesPage::refresh()
{
    auto procs = InfoManager::instance()->processes();

    // Map current model rows by PID for in-place update — preserves selection,
    // scrollbar position, and sort state across refreshes.
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
            // Numeric sort hints
            row[0]->setData(p.pid,           Qt::UserRole);
            row[3]->setData(p.cpuPercent,    Qt::UserRole);
            row[4]->setData(p.memoryKB,      Qt::UserRole);
            m_model->appendRow(row);
        } else {
            int r = it.value();
            if (m_model->item(r, 1)->text() != p.name) m_model->item(r, 1)->setText(p.name);
            if (m_model->item(r, 2)->text() != p.user) m_model->item(r, 2)->setText(p.user);
            if (m_model->item(r, 3)->text() != cpuTxt) m_model->item(r, 3)->setText(cpuTxt);
            if (m_model->item(r, 4)->text() != memTxt) m_model->item(r, 4)->setText(memTxt);
            m_model->item(r, 3)->setData(p.cpuPercent, Qt::UserRole);
            m_model->item(r, 4)->setData(p.memoryKB,    Qt::UserRole);
        }
    }

    // Remove rows for processes that ended (iterate backwards to keep indices valid).
    for (int r = m_model->rowCount() - 1; r >= 0; --r) {
        int pid = m_model->item(r, 0)->text().toInt();
        if (!seenPids.contains(pid)) m_model->removeRow(r);
    }

    ui->countLabel->setText(tr("%1 processes").arg(procs.size()));
}

void ProcessesPage::killSelected()
{
    auto index = ui->processTable->currentIndex();
    if (!index.isValid()) return;

    int pid = m_proxy->data(m_proxy->index(index.row(), 0)).toInt();
    QString name = m_proxy->data(m_proxy->index(index.row(), 1)).toString();

    auto res = QMessageBox::question(this, tr("Kill Process"),
        tr("Kill process '%1' (PID %2)?").arg(name).arg(pid));
    if (res != QMessageBox::Yes) return;

    if (!ProcessInfo::kill(pid))
        QMessageBox::warning(this, tr("Error"), tr("Could not kill process."));
    else
        refresh();
}

void ProcessesPage::onSearchChanged(const QString &text)
{
    m_proxy->setFilterFixedString(text);
}

void ProcessesPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
