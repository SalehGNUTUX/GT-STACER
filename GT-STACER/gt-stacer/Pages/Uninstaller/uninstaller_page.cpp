#include "uninstaller_page.h"
#include "ui_uninstaller_page.h"
#include "../../Widgets/empty_state.h"
#include "../../Widgets/skeleton_rows.h"
#include "../../Widgets/cleaner_icons.h"   // reuse aptCache() glyph
#include "../../../gt-stacer-core/Tools/package_tool.h"
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

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

    // Wrap the .ui's bare QTableView in a QStackedWidget so we can swap it
    // for an EmptyState (initial / no-matches) or a SkeletonRows (loading)
    // without losing the existing model/proxy wiring above.
    QWidget *tableParent = ui->packageTable->parentWidget();
    QLayout *tableLayout = tableParent ? tableParent->layout() : nullptr;
    m_stack = new QStackedWidget(this);

    // Page 0 — Initial empty state with a "Load Packages" call-to-action.
    m_initial = new EmptyState;
    m_initial->setIconSvg(CleanerIcons::aptCache(), 64);
    m_initial->setTitle(tr("Browse installed packages"));
    m_initial->setSubtitle(tr("Click \"Load Packages\" to scan installed software across every detected package manager."));
    m_initial->setActionLabel(tr("Load Packages"));
    connect(m_initial, &EmptyState::actionClicked, this, &UninstallerPage::loadPackages);
    m_stack->addWidget(m_initial);

    // Page 1 — Loading skeleton (animated shimmer)
    auto *skelHost = new QWidget;
    auto *skelLay = new QVBoxLayout(skelHost);
    skelLay->setContentsMargins(0, 4, 0, 0);
    m_skeleton = new SkeletonRows(10);
    skelLay->addWidget(m_skeleton);
    skelLay->addStretch();
    m_stack->addWidget(skelHost);

    // Page 2 — Real results (the package table)
    m_tableHost = new QWidget;
    auto *thLay = new QVBoxLayout(m_tableHost);
    thLay->setContentsMargins(0, 0, 0, 0);
    thLay->addWidget(ui->packageTable);     // re-parents the table
    m_stack->addWidget(m_tableHost);

    // Page 3 — Filter has zeroed out the view
    m_noMatches = new EmptyState;
    m_noMatches->setIconSvg(CleanerIcons::aptCache(), 56);
    m_noMatches->setTitle(tr("No packages match your filter"));
    m_noMatches->setSubtitle(tr("Try a different search term, or pick \"All\" from the manager dropdown."));
    m_stack->addWidget(m_noMatches);

    if (tableLayout) tableLayout->addWidget(m_stack);
    setDisplayState(Initial);

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

void UninstallerPage::setDisplayState(DisplayState s)
{
    if (!m_stack) return;
    if (s == Loading) m_skeleton->start();
    else              m_skeleton->stop();
    m_stack->setCurrentIndex(static_cast<int>(s));
}

void UninstallerPage::loadPackages()
{
    ui->loadButton->setEnabled(false);
    ui->statusLabel->setText(tr("Loading packages..."));
    m_model->setRowCount(0);
    setDisplayState(Loading);

    // Run the scan off the UI thread — allPackages() shells out to dpkg-query
    // / rpm -qa / pacman -Q, which can each take several seconds on a busy
    // system. Without QtConcurrent the skeleton would never even render.
    auto *watcher = new QFutureWatcher<QVector<PackageInfo>>(this);
    connect(watcher, &QFutureWatcher<QVector<PackageInfo>>::finished,
            this, [this, watcher]() {
        watcher->deleteLater();
        const auto pkgs = watcher->result();
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
        setDisplayState(pkgs.isEmpty() ? Initial : Results);
    });
    watcher->setFuture(QtConcurrent::run([]() {
        return PackageTool::allPackages();
    }));
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
    if (m_model->rowCount() > 0)
        setDisplayState(m_proxy->rowCount() > 0 ? Results : NoMatches);
}

void UninstallerPage::onManagerFilterChanged(int idx)
{
    if (idx < 0) return;
    int mgrId = ui->managerCombo->itemData(idx).toInt();
    if (mgrId < 0) {
        m_proxy->setFilterKeyColumn(0);
        m_proxy->setFilterFixedString(ui->searchEdit->text());
    } else {
        m_proxy->setFilterKeyColumn(3);
        m_proxy->setFilterFixedString(PackageTool::managerName(static_cast<PkgMgr>(mgrId)));
    }
    if (m_model->rowCount() > 0)
        setDisplayState(m_proxy->rowCount() > 0 ? Results : NoMatches);
}

void UninstallerPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
