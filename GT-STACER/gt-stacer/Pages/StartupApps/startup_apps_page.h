#pragma once
#include <QWidget>

class QVBoxLayout;
class QLineEdit;
class QLabel;

class StartupAppsPage : public QWidget {
    Q_OBJECT
public:
    explicit StartupAppsPage(QWidget *parent = nullptr);
    ~StartupAppsPage() override;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void refresh();
    void onSearchChanged(const QString &text);
    void openAddDialog();

private:
    void clearRows();
    void appendRow(const class StartupEntry &entry);

    QLineEdit   *m_search   = nullptr;
    QVBoxLayout *m_rowsBox  = nullptr;
    QLabel      *m_emptyHint = nullptr;
    QLabel      *m_countLabel = nullptr;
};
