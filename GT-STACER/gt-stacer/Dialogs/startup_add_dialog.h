#pragma once
#include <QDialog>
#include "../../gt-stacer-core/Tools/startup_tool.h"

class QLineEdit;
class QListView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QTabWidget;
class QPushButton;

// Dialog for adding a new autostart entry. Two tabs:
//  · "From System Apps" — browse installed .desktop entries with search.
//  · "Manual Entry"     — type in Name + Exec + Comment + Icon directly.
//
// On accept, the chosen StartupEntry is available via result().
class StartupAddDialog : public QDialog {
    Q_OBJECT
public:
    explicit StartupAddDialog(QWidget *parent = nullptr);

    StartupEntry result() const { return m_result; }

private slots:
    void onAddFromSystem();
    void onAddManual();

private:
    void buildSystemTab();
    void buildManualTab();

    QTabWidget *m_tabs = nullptr;

    // System tab
    QLineEdit             *m_systemSearch = nullptr;
    QListView             *m_systemList   = nullptr;
    QStandardItemModel    *m_systemModel  = nullptr;
    QSortFilterProxyModel *m_systemProxy  = nullptr;

    // Manual tab
    QLineEdit *m_manualName    = nullptr;
    QLineEdit *m_manualExec    = nullptr;
    QLineEdit *m_manualComment = nullptr;
    QLineEdit *m_manualIcon    = nullptr;

    StartupEntry m_result;
};
