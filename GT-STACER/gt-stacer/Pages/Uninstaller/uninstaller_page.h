#pragma once
#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

namespace Ui { class UninstallerPage; }

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
    Ui::UninstallerPage   *ui;
    QStandardItemModel    *m_model;
    QSortFilterProxyModel *m_proxy;
};
