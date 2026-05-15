#include "services_page.h"
#include "ui_services_page.h"
#include "../../Managers/tool_manager.h"
#include "../../../gt-stacer-core/Tools/service_tool.h"
#include <QHash>
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

    // In-place merge so the user's selection survives reloads.
    QHash<QString, int> rowByName;
    rowByName.reserve(m_model->rowCount());
    for (int r = 0; r < m_model->rowCount(); ++r)
        rowByName.insert(m_model->item(r, 0)->text(), r);

    QSet<QString> seen;
    seen.reserve(svcs.size());

    auto stateColor = [](ServiceState st) -> QColor {
        switch (st) {
        case ServiceState::Active: return QColor("#a6e3a1");
        case ServiceState::Failed: return QColor("#f38ba8");
        default:                   return QColor("#cdd6f4");
        }
    };

    for (const auto &s : svcs) {
        seen.insert(s.name);
        const QString stateTxt = s.stateString();
        const QString enaTxt   = s.enabled ? tr("Yes") : tr("No");
        const QColor  fg       = stateColor(s.state);

        auto it = rowByName.find(s.name);
        if (it == rowByName.end()) {
            QList<QStandardItem*> row;
            row << new QStandardItem(s.name)
                << new QStandardItem(stateTxt)
                << new QStandardItem(enaTxt)
                << new QStandardItem(s.description);
            row[1]->setForeground(fg);
            m_model->appendRow(row);
        } else {
            int r = it.value();
            if (m_model->item(r, 1)->text() != stateTxt) {
                m_model->item(r, 1)->setText(stateTxt);
                m_model->item(r, 1)->setForeground(fg);
            }
            if (m_model->item(r, 2)->text() != enaTxt)        m_model->item(r, 2)->setText(enaTxt);
            if (m_model->item(r, 3)->text() != s.description) m_model->item(r, 3)->setText(s.description);
        }
    }

    for (int r = m_model->rowCount() - 1; r >= 0; --r)
        if (!seen.contains(m_model->item(r, 0)->text())) m_model->removeRow(r);
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
