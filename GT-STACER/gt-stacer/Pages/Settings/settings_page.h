#pragma once
#include <QWidget>

namespace Ui { class SettingsPage; }

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

signals:
    void themeChanged(const QString &theme);
    void languageChanged(const QString &lang);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void applySettings();

private:
    void loadSettings();
    Ui::SettingsPage *ui;
};
