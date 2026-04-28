#pragma once
#include <QWidget>
#include <QStandardItemModel>

namespace Ui { class AptSourcePage; }

class AptSourcePage : public QWidget {
    Q_OBJECT
public:
    explicit AptSourcePage(QWidget *parent = nullptr);
    ~AptSourcePage() override;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void refresh();
    void addSource();
    void removeSource();
    void toggleSource();

private:
    Ui::AptSourcePage  *ui;
    QStandardItemModel *m_model;
};
