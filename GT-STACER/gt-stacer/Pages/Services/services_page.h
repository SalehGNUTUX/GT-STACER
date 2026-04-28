#pragma once
#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

namespace Ui { class ServicesPage; }

class ServicesPage : public QWidget {
    Q_OBJECT
public:
    explicit ServicesPage(QWidget *parent = nullptr);
    ~ServicesPage() override;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void refresh();
    void startService();
    void stopService();
    void enableService();
    void disableService();
    void onSearchChanged(const QString &text);

private:
    QString selectedService();
    Ui::ServicesPage     *ui;
    QStandardItemModel   *m_model;
    QSortFilterProxyModel *m_proxy;
};
