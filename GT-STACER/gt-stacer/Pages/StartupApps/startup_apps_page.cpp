#include "startup_apps_page.h"
#include "ui_startup_apps_page.h"
#include "../../Managers/tool_manager.h"
#include <QHeaderView>

StartupAppsPage::StartupAppsPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::StartupAppsPage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({tr("Name"), tr("Command"), tr("Status"), tr("Description")});
    ui->startupTable->setModel(m_model);
    ui->startupTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->startupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->startupTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->refreshButton, &QPushButton::clicked, this, &StartupAppsPage::refresh);
    connect(ui->toggleButton,  &QPushButton::clicked, this, &StartupAppsPage::toggleSelected);
    connect(ui->removeButton,  &QPushButton::clicked, this, &StartupAppsPage::removeSelected);

    refresh();
}

StartupAppsPage::~StartupAppsPage() { delete ui; }

void StartupAppsPage::refresh()
{
    auto entries = StartupTool::entries();
    m_model->setRowCount(0);
    for (const auto &e : entries) {
        QList<QStandardItem*> row;
        row << new QStandardItem(e.name)
            << new QStandardItem(e.exec)
            << new QStandardItem(e.enabled ? tr("Enabled") : tr("Disabled"))
            << new QStandardItem(e.comment);
        row[2]->setData(e.filePath, Qt::UserRole);
        row[2]->setData(e.enabled, Qt::UserRole + 1);
        if (!e.enabled) row[2]->setForeground(QColor("#6c7086"));
        m_model->appendRow(row);
    }
}

void StartupAppsPage::toggleSelected()
{
    auto idx = ui->startupTable->currentIndex();
    if (!idx.isValid()) return;
    QString path    = m_model->item(idx.row(), 2)->data(Qt::UserRole).toString();
    bool    enabled = m_model->item(idx.row(), 2)->data(Qt::UserRole + 1).toBool();
    if (enabled) StartupTool::disable(path); else StartupTool::enable(path);
    refresh();
}

void StartupAppsPage::removeSelected()
{
    auto idx = ui->startupTable->currentIndex();
    if (!idx.isValid()) return;
    QString path = m_model->item(idx.row(), 2)->data(Qt::UserRole).toString();
    StartupTool::remove(path);
    refresh();
}

void StartupAppsPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
