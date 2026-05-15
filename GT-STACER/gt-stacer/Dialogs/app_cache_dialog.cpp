#include "app_cache_dialog.h"
#include "../../gt-stacer-core/Utils/file_util.h"
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

// Proxy that sorts column-by-column using sort-role data we attached on the
// name item. QStandardItem::sortRole() acts on every column individually.
class CacheSortProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
        const int col = left.column();
        if (col == COL_SIZE) {
            return left.data(ROLE_SIZE_BYTES).toLongLong()
                 < right.data(ROLE_SIZE_BYTES).toLongLong();
        }
        if (col == COL_MODIFIED) {
            return left.data(ROLE_MTIME).toLongLong()
                 < right.data(ROLE_MTIME).toLongLong();
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }
};
}

AppCacheDialog::AppCacheDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Application caches — ~/.cache"));
    setMinimumSize(700, 460);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *hint = new QLabel(
        tr("Each row is a subdirectory of <code>~/.cache</code> — usually one application."
           " Tick the caches you want to remove."));
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#a6adc8;");
    root->addWidget(hint);

    // ── Filter row: search + sort + select-all ─────────────────────────
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

    // ── Table ──────────────────────────────────────────────────────────
    m_model = new QStandardItemModel(0, 3, this);
    m_model->setHorizontalHeaderLabels({tr("Application"), tr("Size"), tr("Last modified")});

    auto *proxy = new CacheSortProxy(this);
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

    // ── Summary + actions ──────────────────────────────────────────────
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
            this, &AppCacheDialog::onSortChanged);
    connect(m_selectAll,  &QCheckBox::toggled,    this, &AppCacheDialog::toggleSelectAll);
    connect(m_model,      &QStandardItemModel::itemChanged,
            this, &AppCacheDialog::onItemChanged);
    connect(m_rescanButton, &QPushButton::clicked, this, &AppCacheDialog::scan);
    connect(m_cleanButton,  &QPushButton::clicked, this, &AppCacheDialog::cleanSelected);
    connect(closeBtn,       &QPushButton::clicked, this, &QDialog::accept);

    scan();
}

void AppCacheDialog::onSortChanged(int index)
{
    switch (index) {
    case 0: m_table->sortByColumn(COL_SIZE,     Qt::DescendingOrder); break; // Largest first
    case 1: m_table->sortByColumn(COL_MODIFIED, Qt::AscendingOrder);  break; // Oldest first
    case 2: m_table->sortByColumn(COL_MODIFIED, Qt::DescendingOrder); break; // Newest first
    case 3: m_table->sortByColumn(COL_NAME,     Qt::AscendingOrder);  break; // Name A→Z
    }
}

// ── Scan ────────────────────────────────────────────────────────────────
void AppCacheDialog::scan()
{
    m_rescanButton->setEnabled(false);
    m_cleanButton->setEnabled(false);
    m_summary->setText(tr("Scanning…"));
    m_model->setRowCount(0);

    auto *watcher = new QFutureWatcher<QVector<CacheEntry>>(this);
    connect(watcher, &QFutureWatcher<QVector<CacheEntry>>::finished, this,
            [this, watcher]() {
        watcher->deleteLater();
        m_entries = watcher->result();
        populateModel(m_entries);
        refreshSummary();
        m_rescanButton->setEnabled(true);
    });

    watcher->setFuture(QtConcurrent::run([]() -> QVector<CacheEntry> {
        QVector<CacheEntry> out;
        QDir cache(QDir::homePath() + "/.cache");
        if (!cache.exists()) return out;

        for (const auto &entry : cache.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
            CacheEntry c;
            c.path    = entry.absoluteFilePath();
            c.appName = entry.fileName();
            if (entry.isDir())
                c.bytes = FileUtil::dirSize(c.path);
            else
                c.bytes = entry.size();
            c.lastModifiedSecs = entry.lastModified().toSecsSinceEpoch();
            out.append(c);
        }
        return out;
    }));
}

void AppCacheDialog::populateModel(const QVector<CacheEntry> &entries)
{
    m_model->setRowCount(0);
    for (const auto &c : entries) {
        auto *name = new QStandardItem(c.appName);
        name->setCheckable(true);
        name->setData(c.bytes, ROLE_SIZE_BYTES);
        name->setData(c.lastModifiedSecs, ROLE_MTIME);
        name->setData(c.path, ROLE_PATH);
        name->setToolTip(c.path);

        auto *size = new QStandardItem(FormatUtil::formatBytes(c.bytes));
        size->setData(c.bytes, ROLE_SIZE_BYTES);
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QString mtimeStr = c.lastModifiedSecs > 0
            ? QDateTime::fromSecsSinceEpoch(c.lastModifiedSecs).toString("yyyy-MM-dd hh:mm")
            : tr("—");
        auto *mtime = new QStandardItem(mtimeStr);
        mtime->setData(c.lastModifiedSecs, ROLE_MTIME);

        m_model->appendRow({name, size, mtime});
    }
}

void AppCacheDialog::onFilterChanged()
{
    m_proxy->setFilterFixedString(m_search->text());
}

void AppCacheDialog::toggleSelectAll(bool checked)
{
    // Apply to currently-visible rows (so the filter constrains the action).
    const auto state = checked ? Qt::Checked : Qt::Unchecked;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        QModelIndex proxyIdx = m_proxy->index(row, COL_NAME);
        QModelIndex srcIdx   = m_proxy->mapToSource(proxyIdx);
        m_model->itemFromIndex(srcIdx)->setCheckState(state);
    }
}

void AppCacheDialog::onItemChanged(QStandardItem *)
{
    refreshSummary();
}

void AppCacheDialog::refreshSummary()
{
    qint64 selectedBytes = 0;
    int    selectedCount = 0;
    qint64 totalBytes    = 0;
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
        tr("%1 caches · %2 total · %3 selected (%4)")
            .arg(m_model->rowCount())
            .arg(FormatUtil::formatBytes(totalBytes))
            .arg(selectedCount)
            .arg(FormatUtil::formatBytes(selectedBytes)));
    m_cleanButton->setEnabled(selectedCount > 0);
}

// ── Clean selected ─────────────────────────────────────────────────────
void AppCacheDialog::cleanSelected()
{
    QVector<QString> targets;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        auto *item = m_model->item(row, COL_NAME);
        if (item->checkState() == Qt::Checked)
            targets.append(item->data(ROLE_PATH).toString());
    }
    if (targets.isEmpty()) return;

    int yes = QMessageBox::question(this, tr("Confirm clean"),
        tr("Remove %1 cache entries from ~/.cache?\n"
           "Apps will rebuild their caches on next launch.").arg(targets.size()));
    if (yes != QMessageBox::Yes) return;

    m_cleanButton->setEnabled(false);
    m_rescanButton->setEnabled(false);

    auto *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        scan(); // refresh sizes
    });
    watcher->setFuture(QtConcurrent::run([targets]() -> int {
        int ok = 0;
        for (const auto &path : targets) {
            QFileInfo fi(path);
            // Guard rail: refuse anything that isn't actually inside ~/.cache.
            QString cacheDir = QDir::homePath() + "/.cache";
            if (!fi.absoluteFilePath().startsWith(cacheDir + "/")) continue;
            if (fi.isDir()) {
                if (QDir(path).removeRecursively()) ++ok;
            } else {
                if (QFile::remove(path)) ++ok;
            }
        }
        return ok;
    }));
}
