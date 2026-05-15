#pragma once
#include <QWidget>
#include <QVector>

class CleanerCard;
class LoadingOverlay;
class QLabel;
class QPushButton;
class QCheckBox;

class SystemCleanerPage : public QWidget {
    Q_OBJECT
public:
    explicit SystemCleanerPage(QWidget *parent = nullptr);
    ~SystemCleanerPage() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void scan();
    void clean();
    void toggleSelectAll(bool checked);
    void onCardToggled();
    void openAppCacheDetails();

private:
    // Each category points at a filesystem path and remembers whether root
    // privileges are required to clean it.
    enum class CleanStrategy {
        UserFiles,        // QFile::remove inside path (user owns it)
        RootRotatedLogs,  // pkexec find … -name *.gz/*.1/*.old -delete
        RootAllInDir,     // pkexec find … -mindepth 1 -delete
        AptClean,         // pkexec apt-get clean
        OldKernelsApt,    // pkexec apt-get autoremove --purge
    };

    struct Category {
        QString name;
        QString description;
        QString iconSvg;
        QString path;        // empty for pseudo-categories like Old Kernels
        bool    recursive = true;
        CleanStrategy strategy = CleanStrategy::UserFiles;
        CleanerCard *card = nullptr;
        qint64 size = 0;     // last-scanned size in bytes
    };

    void   buildUi();
    void   refreshSelectionUi();           // update buttons + selectAll state
    qint64 scanCategory(const Category &c) const;
    bool   cleanCategory(const Category &c) const;

    QVector<Category> m_categories;
    QPushButton    *m_scanButton    = nullptr;
    QPushButton    *m_cleanButton   = nullptr;
    QPushButton    *m_detailsButton = nullptr;
    QCheckBox      *m_selectAll     = nullptr;
    QLabel         *m_statusLabel   = nullptr;
    LoadingOverlay *m_overlay       = nullptr;
    bool            m_scanCompleted = false;
    bool            m_appCacheScanned = false; // enables the Details button
};
