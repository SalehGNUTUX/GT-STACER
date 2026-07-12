#pragma once
#include <QDialog>
#include "../../gt-stacer-core/Tools/package_tool.h"

class QLineEdit;
class QComboBox;
class QTreeView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QLabel;
class QPushButton;
class QCheckBox;

// Drill-down dialog used by both the Flatpak and Snap cards. Mirrors
// AppCacheDialog: list every installed app of the requested manager with
// size + version, support filter/sort, and offer per-app removal through
// `flatpak uninstall` / `pkexec snap remove`.
class UniversalAppsDialog : public QDialog {
    Q_OBJECT
public:
    explicit UniversalAppsDialog(PkgMgr manager, QWidget *parent = nullptr);

private slots:
    void rescan();
    void cleanSelected();
    void onSortChanged(int index);
    void onItemChanged(class QStandardItem *item);
    void toggleSelectAll(bool checked);

private:
    void refreshSummary();
    void populateModel(const QVector<PackageTool::UniversalApp> &apps);

    PkgMgr         m_manager;
    QString        m_managerLabel;

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
