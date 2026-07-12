#include "pkg_cache_dialog.h"
#include "../../gt-stacer-core/Tools/package_tool.h"
#include "../../gt-stacer-core/Utils/command_util.h"
#include "../../gt-stacer-core/Utils/format_util.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
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
constexpr int COL_NAME     = 0;
constexpr int COL_SIZE     = 1;
constexpr int COL_MODIFIED = 2;
constexpr int ROLE_SIZE_BYTES = Qt::UserRole + 1;
constexpr int ROLE_MTIME      = Qt::UserRole + 2;
constexpr int ROLE_PATH       = Qt::UserRole + 3;

class PkgSortProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
        const int col = left.column();
        if (col == COL_SIZE)
            return left.data(ROLE_SIZE_BYTES).toLongLong() < right.data(ROLE_SIZE_BYTES).toLongLong();
        if (col == COL_MODIFIED)
            return left.data(ROLE_MTIME).toLongLong() < right.data(ROLE_MTIME).toLongLong();
        return QSortFilterProxyModel::lessThan(left, right);
    }
};
} // namespace

PkgCacheDialog::PkgCacheDialog(QWidget *parent) : QDialog(parent)
{
    // Resolve manager + cache dir up-front so the title reflects reality.
    const PkgMgr primary = PackageTool::primaryManager();
    m_managerLabel = PackageTool::managerName(primary);
    m_cacheDir     = PackageTool::cacheDir(primary);

    setWindowTitle(m_managerLabel.isEmpty()
        ? tr("Package cache")
        : tr("%1 cache — %2").arg(m_managerLabel, m_cacheDir));
    setMinimumSize(720, 480);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *hint = new QLabel;
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#a6adc8;");
    if (m_cacheDir.isEmpty()) {
        hint->setText(tr("No on-disk package cache for the detected manager (<b>%1</b>). "
                          "Universal managers like Flatpak/Snap manage their own storage.")
                       .arg(m_managerLabel.isEmpty() ? tr("Unknown") : m_managerLabel));
    } else {
        hint->setText(tr("Files inside <code>%1</code>. Tick what to remove — only files "
                          "inside this directory are touched.").arg(m_cacheDir));
    }
    root->addWidget(hint);

    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    filterRow->addWidget(m_search, 1);

    m_sortCombo = new QComboBox;
    m_sortCombo->addItem(tr("Largest first"));
    m_sortCombo->addItem(tr("Oldest first"));
    m_sortCombo->addItem(tr("Newest first"));
    m_sortCombo->addItem(tr("Name A→Z"));
    filterRow->addWidget(m_sortCombo);

    m_selectAll = new QCheckBox(tr("Select All"));
    filterRow->addWidget(m_selectAll);
    root->addLayout(filterRow);

    m_model = new QStandardItemModel(0, 3, this);
    m_model->setHorizontalHeaderLabels({tr("File"), tr("Size"), tr("Modified")});

    auto *proxy = new PkgSortProxy(this);
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
    m_table->header()->setSectionResizeMode(COL_SIZE, QHeaderView::ResizeToContents);
    m_table->header()->setSectionResizeMode(COL_MODIFIED, QHeaderView::ResizeToContents);
    m_table->sortByColumn(COL_SIZE, Qt::DescendingOrder);
    root->addWidget(m_table, 1);

    auto *summaryRow = new QHBoxLayout;
    m_summary = new QLabel;
    m_summary->setStyleSheet("color:#a6adc8;");
    summaryRow->addWidget(m_summary, 1);

    m_rescanButton = new QPushButton(tr("Rescan"));
    summaryRow->addWidget(m_rescanButton);

    m_cleanButton = new QPushButton(tr("Clean Selected"));
    m_cleanButton->setObjectName("dangerButton");
    m_cleanButton->setEnabled(false);
    summaryRow->addWidget(m_cleanButton);

    auto *closeBtn = new QPushButton(tr("Close"));
    summaryRow->addWidget(closeBtn);
    root->addLayout(summaryRow);

    connect(m_search,     &QLineEdit::textChanged,
            this, [this](const QString &t){ m_proxy->setFilterFixedString(t); });
    connect(m_sortCombo,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PkgCacheDialog::onSortChanged);
    connect(m_selectAll,  &QCheckBox::toggled,    this, &PkgCacheDialog::toggleSelectAll);
    connect(m_model,      &QStandardItemModel::itemChanged,
            this, &PkgCacheDialog::onItemChanged);
    connect(m_rescanButton, &QPushButton::clicked, this, &PkgCacheDialog::scan);
    connect(m_cleanButton,  &QPushButton::clicked, this, &PkgCacheDialog::cleanSelected);
    connect(closeBtn,       &QPushButton::clicked, this, &QDialog::accept);

    if (!m_cacheDir.isEmpty()) scan();
    else                       refreshSummary();
}

void PkgCacheDialog::onSortChanged(int index)
{
    switch (index) {
    case 0: m_table->sortByColumn(COL_SIZE,     Qt::DescendingOrder); break;
    case 1: m_table->sortByColumn(COL_MODIFIED, Qt::AscendingOrder);  break;
    case 2: m_table->sortByColumn(COL_MODIFIED, Qt::DescendingOrder); break;
    case 3: m_table->sortByColumn(COL_NAME,     Qt::AscendingOrder);  break;
    }
}

void PkgCacheDialog::scan()
{
    if (m_cacheDir.isEmpty()) return;
    m_rescanButton->setEnabled(false);
    m_cleanButton->setEnabled(false);
    m_summary->setText(tr("Scanning…"));
    m_model->setRowCount(0);

    QString dir = m_cacheDir;
    auto *watcher = new QFutureWatcher<QVector<CacheFile>>(this);
    connect(watcher, &QFutureWatcher<QVector<CacheFile>>::finished, this,
            [this, watcher]() {
        watcher->deleteLater();
        populateModel(watcher->result());
        refreshSummary();
        m_rescanButton->setEnabled(true);
    });

    watcher->setFuture(QtConcurrent::run([dir]() -> QVector<CacheFile> {
        QVector<CacheFile> out;
        QDir d(dir);
        if (!d.exists()) return out;
        const auto entries = d.entryInfoList(QDir::Files | QDir::NoSymLinks);
        out.reserve(entries.size());
        for (const auto &fi : entries) {
            CacheFile c;
            c.path = fi.absoluteFilePath();
            c.fileName = fi.fileName();
            c.bytes = fi.size();
            c.modifiedSecs = fi.lastModified().toSecsSinceEpoch();
            out.append(c);
        }
        return out;
    }));
}

void PkgCacheDialog::populateModel(const QVector<CacheFile> &files)
{
    m_model->setRowCount(0);
    for (const auto &f : files) {
        auto *name = new QStandardItem(f.fileName);
        name->setCheckable(true);
        name->setData(f.bytes,        ROLE_SIZE_BYTES);
        name->setData(f.modifiedSecs, ROLE_MTIME);
        name->setData(f.path,         ROLE_PATH);
        name->setToolTip(f.path);

        auto *size = new QStandardItem(FormatUtil::formatBytes(f.bytes));
        size->setData(f.bytes, ROLE_SIZE_BYTES);
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QString mtimeStr = f.modifiedSecs > 0
            ? QDateTime::fromSecsSinceEpoch(f.modifiedSecs).toString("yyyy-MM-dd hh:mm")
            : tr("—");
        auto *mtime = new QStandardItem(mtimeStr);
        mtime->setData(f.modifiedSecs, ROLE_MTIME);

        m_model->appendRow({name, size, mtime});
    }
}

void PkgCacheDialog::toggleSelectAll(bool checked)
{
    const auto state = checked ? Qt::Checked : Qt::Unchecked;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        QModelIndex proxyIdx = m_proxy->index(row, COL_NAME);
        QModelIndex srcIdx   = m_proxy->mapToSource(proxyIdx);
        m_model->itemFromIndex(srcIdx)->setCheckState(state);
    }
}

void PkgCacheDialog::onItemChanged(QStandardItem *) { refreshSummary(); }

void PkgCacheDialog::refreshSummary()
{
    if (m_cacheDir.isEmpty()) {
        m_summary->setText(tr("No cache directory available."));
        m_cleanButton->setEnabled(false);
        return;
    }
    qint64 selectedBytes = 0, totalBytes = 0;
    int    selectedCount = 0;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        auto *item = m_model->item(row, COL_NAME);
        qint64 b = item->data(ROLE_SIZE_BYTES).toLongLong();
        totalBytes += b;
        if (item->checkState() == Qt::Checked) {
            selectedBytes += b;
            ++selectedCount;
        }
    }
    m_summary->setText(
        tr("%1 files · %2 total · %3 selected (%4)")
            .arg(m_model->rowCount())
            .arg(FormatUtil::formatBytes(totalBytes))
            .arg(selectedCount)
            .arg(FormatUtil::formatBytes(selectedBytes)));
    m_cleanButton->setEnabled(selectedCount > 0);
}

void PkgCacheDialog::cleanSelected()
{
    QStringList targets;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        auto *item = m_model->item(row, COL_NAME);
        if (item->checkState() == Qt::Checked)
            targets << item->data(ROLE_PATH).toString();
    }
    if (targets.isEmpty()) return;

    int yes = QMessageBox::question(this, tr("Confirm clean"),
        tr("Remove %1 file(s) from %2?\n\nYou will be asked for your password.")
            .arg(targets.size()).arg(m_cacheDir));
    if (yes != QMessageBox::Yes) return;

    m_cleanButton->setEnabled(false);
    m_rescanButton->setEnabled(false);

    // Path-confine — refuse anything not inside the configured cache dir.
    const QString cacheDir = m_cacheDir;
    QStringList safe;
    for (const auto &p : targets)
        if (p.startsWith(cacheDir + "/")) safe << p;

    auto *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        scan();
    });
    watcher->setFuture(QtConcurrent::run([safe]() -> int {
        if (safe.isEmpty()) return 0;
        // Batch into a single pkexec call so the password prompt fires once.
        QStringList args = {"rm", "-f"};
        args << safe;
        return CommandUtil::execProgram("pkexec", args, 120000);
    }));
}
