#pragma once
#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

namespace Ui { class UninstallerPage; }
class EmptyState;
class SkeletonRows;
class QStackedWidget;

class UninstallerPage : public QWidget {
    Q_OBJECT
public:
    explicit UninstallerPage(QWidget *parent = nullptr);
    ~UninstallerPage() override;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void loadPackages();
    void uninstallSelected();
    void onSearchChanged(const QString &text);
    void onManagerFilterChanged(int index);

private:
    // The package table sits inside a QStackedWidget so we can swap it out for
    // a skeleton loader (while scanning) or an EmptyState (before first scan /
    // when no packages match the current filter).
    enum DisplayState { Initial = 0, Loading = 1, Results = 2, NoMatches = 3 };
    void setDisplayState(DisplayState s);

    Ui::UninstallerPage   *ui;
    QStandardItemModel    *m_model;
    QSortFilterProxyModel *m_proxy;

    QStackedWidget *m_stack     = nullptr;
    EmptyState     *m_initial   = nullptr;
    SkeletonRows   *m_skeleton  = nullptr;
    EmptyState     *m_noMatches = nullptr;
    QWidget        *m_tableHost = nullptr;
};
