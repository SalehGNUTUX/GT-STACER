#pragma once
#include <QWidget>
#include <QVector>
#include <functional>
#include "../../../gt-stacer-core/Tools/package_tool.h"

class QTabWidget;
class QStandardItemModel;
class QSortFilterProxyModel;
class QTableView;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class LoadingOverlay;

// Package & software manager (26.11) — evolves the old Uninstaller into a full,
// professionally organized package tool with four tabs:
//   • Installed      — browse & remove installed software (all detected managers)
//   • Search & Install — search a manager's repos and install
//   • Upgrades        — list & apply available upgrades
//   • Store add-ons   — manage opendesktop.org / KNewStuff content
// Every privileged operation goes through PackageTool (execProgram, no shell;
// names validated with isSafeIdentifier). Long operations run off the UI thread
// behind a LoadingOverlay.
class PackageManagerPage : public QWidget {
    Q_OBJECT
public:
    explicit PackageManagerPage(QWidget *parent = nullptr);
    ~PackageManagerPage() override;

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // ── Installed tab ──
    QWidget *buildInstalledTab();
    void loadInstalled();
    void removeSelectedInstalled();

    // ── Search & Install tab ──
    QWidget *buildSearchTab();
    void runSearch();
    void installSelectedSearch();

    // ── Upgrades tab ──
    QWidget *buildUpgradesTab();
    void loadUpgrades();
    void upgradeSelected();
    void upgradeEverything();

    // ── Store add-ons tab ──
    QWidget *buildStoreTab();
    void loadStore();
    void removeSelectedStore();
    void installFromOcs();

    // ── AppImages tab (GearLever-compatible integration) ──
    QWidget *buildAppImagesTab();
    void loadAppImages();
    void addAppImage();
    void removeSelectedAppImage();

    // Run a blocking op set off the UI thread behind the overlay, then `after`.
    void runOff(const QString &msg, const std::function<void()> &work,
                const std::function<void()> &after);

    QTabWidget     *m_tabs    = nullptr;
    LoadingOverlay *m_overlay = nullptr;

    // Installed
    QStandardItemModel    *m_instModel = nullptr;
    QSortFilterProxyModel *m_instProxy = nullptr;
    QTableView            *m_instTable = nullptr;
    QLineEdit             *m_instSearch = nullptr;
    QComboBox             *m_instMgr    = nullptr;
    QPushButton           *m_instRemove = nullptr;
    QLabel                *m_instStatus = nullptr;

    // Search & Install
    QComboBox   *m_srchMgr    = nullptr;
    QLineEdit   *m_srchEdit   = nullptr;
    QPushButton *m_srchButton = nullptr;
    QStandardItemModel *m_srchModel = nullptr;
    QTableView  *m_srchTable  = nullptr;
    QPushButton *m_srchInstall = nullptr;
    QLabel      *m_srchStatus = nullptr;

    // Upgrades
    QComboBox   *m_upMgr    = nullptr;
    QStandardItemModel *m_upModel = nullptr;
    QTableView  *m_upTable  = nullptr;
    QPushButton *m_upCheck  = nullptr;
    QPushButton *m_upOne    = nullptr;
    QPushButton *m_upAll    = nullptr;
    QLabel      *m_upStatus = nullptr;

    // Store add-ons
    QStandardItemModel *m_stModel = nullptr;
    QSortFilterProxyModel *m_stProxy = nullptr;
    QTableView  *m_stTable  = nullptr;
    QLineEdit   *m_stSearch = nullptr;
    QPushButton *m_stRemove = nullptr;
    QLabel      *m_stStatus = nullptr;

    // AppImages
    QStandardItemModel *m_aiModel = nullptr;
    QSortFilterProxyModel *m_aiProxy = nullptr;
    QTableView  *m_aiTable  = nullptr;
    QLineEdit   *m_aiSearch = nullptr;
    QPushButton *m_aiRemove = nullptr;
    QLabel      *m_aiStatus = nullptr;
};
