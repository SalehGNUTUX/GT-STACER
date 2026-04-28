#pragma once
#include <QWidget>
#include <QStandardItemModel>
#include <QFuture>

namespace Ui { class SystemCleanerPage; }
class LoadingOverlay;

class SystemCleanerPage : public QWidget {
    Q_OBJECT
public:
    explicit SystemCleanerPage(QWidget *parent = nullptr);
    ~SystemCleanerPage() override;

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void scan();
    void clean();

private:
    struct CleanCategory {
        QString name;
        QString path;
        bool    recursive;
        QString description;
    };

    qint64 calcCategorySize(const CleanCategory &cat);
    void   cleanCategory(const CleanCategory &cat);

    Ui::SystemCleanerPage *ui;
    QStandardItemModel    *m_model;
    QVector<CleanCategory> m_categories;
    LoadingOverlay        *m_overlay = nullptr;
};
