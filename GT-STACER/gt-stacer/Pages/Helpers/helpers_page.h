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
    void flushDns();
    void applySwappiness();

private:
    void loadBootParams();
    void loadLocale();
    void loadSwappiness();
    void retranslateTabs();

    Ui::HelpersPage *ui;
    int m_diskImageTab = -1;
    int m_removableTab = -1;
    int m_bootTab = -1;
};
