#pragma once
#include <QDialog>
#include <QVector>

class QLineEdit;
class QComboBox;
class QTreeView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QLabel;
class QPushButton;
class QCheckBox;

// Drill-down dialog opened when the user activates the "App Cache" card.
// Lists every subdirectory of ~/.cache as one row (one app/component each)
// with size and last-modified time. Lets the user filter and select which
// caches to wipe, instead of nuking ~/.cache wholesale.
class AppCacheDialog : public QDialog {
    Q_OBJECT
public:
    explicit AppCacheDialog(QWidget *parent = nullptr);

private slots:
    void scan();
    void cleanSelected();
    void onFilterChanged();
    void onSortChanged(int index);
    void onItemChanged(class QStandardItem *item);
    void toggleSelectAll(bool checked);

private:
    struct CacheEntry {
        QString path;
        QString appName;
        qint64  bytes = 0;
        qint64  lastModifiedSecs = 0; // Unix seconds; 0 = unknown
    };

    void refreshSummary();
    void populateModel(const QVector<CacheEntry> &entries);

    QLineEdit             *m_search       = nullptr;
    QComboBox             *m_sortCombo    = nullptr;
    QTreeView             *m_table        = nullptr;
    QStandardItemModel    *m_model        = nullptr;
    QSortFilterProxyModel *m_proxy        = nullptr;
    QCheckBox             *m_selectAll    = nullptr;
    QLabel                *m_summary      = nullptr;
    QPushButton           *m_cleanButton  = nullptr;
    QPushButton           *m_rescanButton = nullptr;

    QVector<CacheEntry>    m_entries; // last scan results
};
