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

private slots:
    void refresh();
    void killSelected();
    void onSearchChanged(const QString &text);

private:
    Ui::ProcessesPage   *ui;
    QTimer              *m_timer;
    QStandardItemModel  *m_model;
    QSortFilterProxyModel *m_proxy;
};
