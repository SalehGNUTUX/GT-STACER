#include "uninstaller_page.h"
#include "ui_uninstaller_page.h"
#include "../../../gt-stacer-core/Tools/package_tool.h"
#include <QHeaderView>
#include <QMessageBox>

UninstallerPage::UninstallerPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::UninstallerPage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({tr("Name"), tr("Version"), tr("Size"), tr("Manager")});

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(0);

    ui->packageTable->setModel(m_proxy);
    ui->packageTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->packageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->packageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->packageTable->setSortingEnabled(true);

    // Populate manager filter from available managers
    ui->managerCombo->addItem(tr("All"), -1);
    auto available = PackageTool::availableManagers();
    for (PkgMgr mgr : available) {
        ui->managerCombo->addItem(PackageTool::managerName(mgr),
                                   static_cast<int>(mgr));
    }

    connect(ui->loadButton,   &QPushButton::clicked, this, &UninstallerPage::loadPackages);
    connect(ui->removeButton, &QPushButton::clicked, this, &UninstallerPage::uninstallSelected);
    connect(ui->searchEdit,   &QLineEdit::textChanged, this, &UninstallerPage::onSearchChanged);
    connect(ui->managerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UninstallerPage::onManagerFilterChanged);
}

UninstallerPage::~UninstallerPage() { delete ui; }

void UninstallerPage::loadPackages()
{
    ui->loadButton->setEnabled(false);
    ui->statusLabel->setText(tr("Loading packages..."));
    m_model->setRowCount(0);

    auto pkgs = PackageTool::allPackages();
    for (const auto &p : pkgs) {
        QList<QStandardItem*> row;
        row << new QStandardItem(p.name)
            << new QStandardItem(p.version)
            << new QStandardItem(p.size)
            << new QStandardItem(PackageTool::managerName(p.manager));
        row[0]->setData(p.name,                      Qt::UserRole);
        row[0]->setData(static_cast<int>(p.manager), Qt::UserRole + 1);
        m_model->appendRow(row);
    }
    ui->statusLabel->setText(tr("%1 packages").arg(pkgs.size()));
    ui->loadButton->setEnabled(true);
}

void UninstallerPage::uninstallSelected()
{
    auto idx = ui->packageTable->currentIndex();
    if (!idx.isValid()) return;

    QString name = m_proxy->data(m_proxy->index(idx.row(), 0), Qt::UserRole).toString();
    int     mgri = m_proxy->data(m_proxy->index(idx.row(), 0), Qt::UserRole + 1).toInt();
    auto    mgr  = static_cast<PkgMgr>(mgri);

    auto ans = QMessageBox::question(this, tr("Uninstall"),
        tr("Uninstall '%1'?").arg(name));
    if (ans != QMessageBox::Yes) return;

    if (!PackageTool::remove(name, mgr))
        QMessageBox::warning(this, tr("Error"), tr("Failed to uninstall '%1'.").arg(name));
    else
        loadPackages();
}

void UninstallerPage::onSearchChanged(const QString &t)
{
    m_proxy->setFilterKeyColumn(0);
    m_proxy->setFilterFixedString(t);
}

void UninstallerPage::onManagerFilterChanged(int idx)
{
    if (idx < 0) return;
    int mgrId = ui->managerCombo->itemData(idx).toInt();
    if (mgrId < 0) {
        // "All"
        m_proxy->setFilterKeyColumn(0);
        m_proxy->setFilterFixedString(ui->searchEdit->text());
    } else {
        m_proxy->setFilterKeyColumn(3);
        m_proxy->setFilterFixedString(PackageTool::managerName(static_cast<PkgMgr>(mgrId)));
    }
}

void UninstallerPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
