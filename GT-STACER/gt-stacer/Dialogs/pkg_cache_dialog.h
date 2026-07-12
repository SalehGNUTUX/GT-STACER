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

// Drill-down for the System Cleaner's package-cache card. Lists every file in
// the active package manager's cache directory (the path returned by
// PackageTool::cacheDir(primaryManager()) so it works for APT, DNF, Pacman,
// Zypper, XBPS, APK, Eopkg, …).
//
// Provides search, sort (largest/oldest/newest/name), per-row selection, and
// a "Clean Selected" action that batches deletions through pkexec.
class PkgCacheDialog : public QDialog {
    Q_OBJECT
public:
    explicit PkgCacheDialog(QWidget *parent = nullptr);

private slots:
    void scan();
    void cleanSelected();
    void onSortChanged(int index);
    void onItemChanged(class QStandardItem *item);
    void toggleSelectAll(bool checked);

private:
    struct CacheFile {
        QString  path;
        QString  fileName;
        qint64   bytes = 0;
        qint64   modifiedSecs = 0;
    };

    void refreshSummary();
    void populateModel(const QVector<CacheFile> &files);

    QString      m_cacheDir;
    QString      m_managerLabel;

    QLineEdit             *m_search       = nullptr;
    QComboBox             *m_sortCombo    = nullptr;
    QTreeView             *m_table        = nullptr;
    QStandardItemModel    *m_model        = nullptr;
    QSortFilterProxyModel *m_proxy        = nullptr;
    QCheckBox             *m_selectAll    = nullptr;
    QLabel                *m_summary      = nullptr;
    QPushButton           *m_cleanButton  = nullptr;
    QPushButton           *m_rescanButton = nullptr;
};
