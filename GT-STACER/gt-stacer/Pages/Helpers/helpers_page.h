#pragma once
#include <QWidget>

namespace Ui { class HelpersPage; }

class HelpersPage : public QWidget {
    Q_OBJECT
public:
    explicit HelpersPage(QWidget *parent = nullptr);
    ~HelpersPage() override;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void loadHosts();
    void saveHosts();

private:
    Ui::HelpersPage *ui;
};
