#pragma once
#include <QWidget>
#include <QStandardItemModel>

namespace Ui { class StartupAppsPage; }

class StartupAppsPage : public QWidget {
    Q_OBJECT
public:
    explicit StartupAppsPage(QWidget *parent = nullptr);
    ~StartupAppsPage() override;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void refresh();
    void toggleSelected();
    void removeSelected();

private:
    Ui::StartupAppsPage  *ui;
    QStandardItemModel   *m_model;
};
