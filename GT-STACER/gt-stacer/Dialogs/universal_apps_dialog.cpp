#include "universal_apps_dialog.h"
#include "../../gt-stacer-core/Utils/format_util.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {
constexpr int COL_NAME = 0;
constexpr int COL_VER  = 1;
constexpr int COL_SIZE = 2;
constexpr int COL_ID   = 3;
constexpr int ROLE_SIZE_BYTES = Qt::UserRole + 1;
constexpr int ROLE_APP_ID     = Qt::UserRole + 2;

class AppsSortProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
protected:
    bool lessThan(const QModelIndex &l, const QModelIndex &r) const override {
        if (l.column() == COL_SIZE)
            return l.data(ROLE_SIZE_BYTES).toLongLong() < r.data(ROLE_SIZE_BYTES).toLongLong();
        return QSortFilterProxyModel::lessThan(l, r);
    }
};
} // namespace

UniversalAppsDialog::UniversalAppsDialog(PkgMgr manager, QWidget *parent)
    : QDialog(parent), m_manager(manager)
{
    m_managerLabel = PackageTool::managerName(manager);

    setWindowTitle(tr("%1 applications").arg(m_managerLabel));
    setMinimumSize(720, 460);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *hint = new QLabel(
        tr("Installed %1 apps. Tick the entries you want to remove — "
           "Flatpak/Snap reclaim space automatically once an app is uninstalled.")
            .arg(m_managerLabel));
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#a6adc8;");
    root->addWidget(hint);

    // Filter row
    auto *filterRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    filterRow->addWidget(m_search, 1);

    m_sortCombo = new QComboBox;
    m_sortCombo->addItem(tr("Largest first"));
    m_sortCombo->addItem(tr("Name A→Z"));
    filterRow->addWidget(m_sortCombo);

    m_selectAll = new QCheckBox(tr("Select All"));
    filterRow->addWidget(m_selectAll);
    root->addLayout(filterRow);

    // Table
    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({tr("Application"), tr("Version"), tr("Size"), tr("ID")});

    auto *proxy = new AppsSortProxy(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(COL_NAME);
    m_proxy = proxy;

    m_table = new QTreeView;
    m_table->setModel(m_proxy);
    m_table->setRootIsDecorated(false);
    m_table->setUniformRowHeights(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->header()->setSectionResizeMode(COL_NAME, QHeaderView::Stretch);
    m_table->header()->setSectionResizeMode(COL_VER,  QHeaderView::ResizeToContents);
    m_table->header()->setSectionResizeMode(COL_SIZE, QHeaderView::ResizeToContents);
    m_table->header()->setSectionResizeMode(COL_ID,   QHeaderView::Stretch);
    m_table->sortByColumn(COL_SIZE, Qt::DescendingOrder);
    root->addWidget(m_table, 1);

    // Summary + actions
    auto *bottom = new QHBoxLayout;
    m_summary = new QLabel;
    m_summary->setStyleSheet("color:#a6adc8;");
    bottom->addWidget(m_summary, 1);

    m_rescanButton = new QPushButton(tr("Rescan"));
    bottom->addWidget(m_rescanButton);

    m_cleanButton = new QPushButton(tr("Remove Selected"));
    m_cleanButton->setObjectName("dangerButton");
    m_cleanButton->setEnabled(false);
    bottom->addWidget(m_cleanButton);

    auto *closeBtn = new QPushButton(tr("Close"));
    bottom->addWidget(closeBtn);
    root->addLayout(bottom);

    connect(m_search,     &QLineEdit::textChanged,
            this, [this](const QString &t){ m_proxy->setFilterFixedString(t); });
    connect(m_sortCombo,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &UniversalAppsDialog::onSortChanged);
    connect(m_selectAll,  &QCheckBox::toggled,    this, &UniversalAppsDialog::toggleSelectAll);
    connect(m_model,      &QStandardItemModel::itemChanged,
            this, &UniversalAppsDialog::onItemChanged);
    connect(m_rescanButton, &QPushButton::clicked, this, &UniversalAppsDialog::rescan);
    connect(m_cleanButton,  &QPushButton::clicked, this, &UniversalAppsDialog::cleanSelected);
    connect(closeBtn,       &QPushButton::clicked, this, &QDialog::accept);

    rescan();
}

void UniversalAppsDialog::onSortChanged(int i)
{
    if (i == 0) m_table->sortByColumn(COL_SIZE, Qt::DescendingOrder);
    else        m_table->sortByColumn(COL_NAME, Qt::AscendingOrder);
}

void UniversalAppsDialog::rescan()
{
    m_rescanButton->setEnabled(false);
    m_cleanButton->setEnabled(false);
    m_summary->setText(tr("Loading…"));
    m_model->setRowCount(0);

    PkgMgr mgr = m_manager;
    auto *watcher = new QFutureWatcher<QVector<PackageTool::UniversalApp>>(this);
    connect(watcher, &QFutureWatcher<QVector<PackageTool::UniversalApp>>::finished, this,
            [this, watcher]() {
        watcher->deleteLater();
        populateModel(watcher->result());
        refreshSummary();
        m_rescanButton->setEnabled(true);
    });
    watcher->setFuture(QtConcurrent::run([mgr]() {
        return mgr == PkgMgr::Snap ? PackageTool::snapApps() : PackageTool::flatpakApps();
    }));
}

void UniversalAppsDialog::populateModel(const QVector<PackageTool::UniversalApp> &apps)
{
    m_model->setRowCount(0);
    for (const auto &a : apps) {
        auto *name = new QStandardItem(a.name);
        name->setCheckable(true);
        name->setData(a.sizeBytes, ROLE_SIZE_BYTES);
        name->setData(a.appId,     ROLE_APP_ID);
        name->setToolTip(a.appId);

        auto *ver  = new QStandardItem(a.version);
        auto *size = new QStandardItem(a.sizeBytes > 0 ? FormatUtil::formatBytes(a.sizeBytes) : QString("—"));
        size->setData(a.sizeBytes, ROLE_SIZE_BYTES);
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto *id = new QStandardItem(a.appId);
        id->setForeground(QColor("#6c7086"));

        m_model->appendRow({name, ver, size, id});
    }
}

void UniversalAppsDialog::toggleSelectAll(bool checked)
{
    const auto state = checked ? Qt::Checked : Qt::Unchecked;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        QModelIndex proxyIdx = m_proxy->index(row, COL_NAME);
        QModelIndex srcIdx   = m_proxy->mapToSource(proxyIdx);
        m_model->itemFromIndex(srcIdx)->setCheckState(state);
    }
}

void UniversalAppsDialog::onItemChanged(QStandardItem *) { refreshSummary(); }

void UniversalAppsDialog::refreshSummary()
{
    qint64 selBytes = 0, totalBytes = 0;
    int sel = 0;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        auto *item = m_model->item(row, COL_NAME);
        qint64 b = item->data(ROLE_SIZE_BYTES).toLongLong();
        totalBytes += b;
        if (item->checkState() == Qt::Checked) { selBytes += b; ++sel; }
    }
    m_summary->setText(
        tr("%1 apps · %2 total · %3 selected (%4)")
            .arg(m_model->rowCount())
            .arg(FormatUtil::formatBytes(totalBytes))
            .arg(sel)
            .arg(FormatUtil::formatBytes(selBytes)));
    m_cleanButton->setEnabled(sel > 0);
}

void UniversalAppsDialog::cleanSelected()
{
    QStringList targets;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        auto *item = m_model->item(row, COL_NAME);
        if (item->checkState() == Qt::Checked)
            targets << item->data(ROLE_APP_ID).toString();
    }
    if (targets.isEmpty()) return;

    int yes = QMessageBox::question(this, tr("Confirm removal"),
        tr("Remove %1 %2 application(s)?\n\nThis cannot be undone — you'll need "
           "to reinstall any app you remove here.")
            .arg(targets.size()).arg(m_managerLabel));
    if (yes != QMessageBox::Yes) return;

    m_cleanButton->setEnabled(false);
    m_rescanButton->setEnabled(false);

    PkgMgr mgr = m_manager;
    auto *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        rescan();
    });
    watcher->setFuture(QtConcurrent::run([mgr, targets]() -> int {
        int ok = 0;
        for (const auto &id : targets)
            if (PackageTool::remove(id, mgr)) ++ok;
        return ok;
    }));
}
