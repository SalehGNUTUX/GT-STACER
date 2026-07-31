#pragma once
#include <QWidget>
#include <QTimer>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

namespace Ui { class ProcessesPage; }

class ProcessesPage : public QWidget {
    Q_OBJECT
public:
    explicit ProcessesPage(QWidget *parent = nullptr);
    ~ProcessesPage() override;

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;   // resume polling only when shown
    void hideEvent(QHideEvent *event) override;    // stop polling when navigated away

private slots:
    void refresh();
    void killSelected();          // SIGTERM (graceful)
    void onSearchChanged(const QString &text);
    void onContextMenu(const QPoint &pos);

private:
    // Common entry point — confirms (with extra warning for critical PIDs)
    // and then dispatches to the requested signal/operation.
    enum class Action { Terminate, ForceKill, Suspend, Resume, ReniceLow, ReniceHigh };
    void runActionOnSelected(Action a);

    // Returns the selected (pid, name) pair, or {0, ""} if none.
    QPair<int, QString> currentSelection() const;

    Ui::ProcessesPage   *ui;
    QTimer              *m_timer;
    QStandardItemModel  *m_model;
    QSortFilterProxyModel *m_proxy;
};
