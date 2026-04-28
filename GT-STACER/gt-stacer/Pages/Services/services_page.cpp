#include "services_page.h"
#include "ui_services_page.h"
#include "../../Managers/tool_manager.h"
#include "../../../gt-stacer-core/Tools/service_tool.h"
#include <QHeaderView>
#include <QMessageBox>

ServicesPage::ServicesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::ServicesPage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({tr("Name"), tr("Status"), tr("Enabled"), tr("Description")});

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(0);

    ui->serviceTable->setModel(m_proxy);
    ui->serviceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    ui->serviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->serviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->refreshButton, &QPushButton::clicked, this, &ServicesPage::refresh);
    connect(ui->startButton,   &QPushButton::clicked, this, &ServicesPage::startService);
    connect(ui->stopButton,    &QPushButton::clicked, this, &ServicesPage::stopService);
    connect(ui->enableButton,  &QPushButton::clicked, this, &ServicesPage::enableService);
    connect(ui->disableButton, &QPushButton::clicked, this, &ServicesPage::disableService);
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &ServicesPage::onSearchChanged);

    refresh();
}

ServicesPage::~ServicesPage() { delete ui; }

void ServicesPage::refresh()
{
    auto svcs = ServiceTool::services();
    m_model->setRowCount(0);
    for (const auto &s : svcs) {
        QList<QStandardItem*> row;
        row << new QStandardItem(s.name)
            << new QStandardItem(s.stateString())
            << new QStandardItem(s.enabled ? tr("Yes") : tr("No"))
            << new QStandardItem(s.description);
        if (s.state == ServiceState::Active)
            row[1]->setForeground(QColor("#a6e3a1"));
        else if (s.state == ServiceState::Failed)
            row[1]->setForeground(QColor("#f38ba8"));
        m_model->appendRow(row);
    }
}

QString ServicesPage::selectedService()
{
    auto idx = ui->serviceTable->currentIndex();
    if (!idx.isValid()) return {};
    return m_proxy->data(m_proxy->index(idx.row(), 0)).toString();
}

void ServicesPage::startService()   { if (auto s = selectedService(); !s.isEmpty()) { ServiceTool::start(s);   refresh(); } }
void ServicesPage::stopService()    { if (auto s = selectedService(); !s.isEmpty()) { ServiceTool::stop(s);    refresh(); } }
void ServicesPage::enableService()  { if (auto s = selectedService(); !s.isEmpty()) { ServiceTool::enable(s);  refresh(); } }
void ServicesPage::disableService() { if (auto s = selectedService(); !s.isEmpty()) { ServiceTool::disable(s); refresh(); } }
void ServicesPage::onSearchChanged(const QString &t) { m_proxy->setFilterFixedString(t); }

void ServicesPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
