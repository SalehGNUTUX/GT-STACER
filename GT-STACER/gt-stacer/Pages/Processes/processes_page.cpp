#include "processes_page.h"
#include "ui_processes_page.h"
#include "../../Managers/info_manager.h"
#include "../../Managers/setting_manager.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Info/process_info.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QRegularExpression>

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
    m_model->setRowCount(0);
    for (const auto &p : procs) {
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(p.pid))
            << new QStandardItem(p.name)
            << new QStandardItem(p.user)
            << new QStandardItem(FormatUtil::formatPercent(p.cpuPercent))
            << new QStandardItem(FormatUtil::formatBytes(p.memoryKB * 1024));
        m_model->appendRow(row);
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
