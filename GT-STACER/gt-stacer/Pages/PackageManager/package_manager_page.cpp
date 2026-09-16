#include "package_manager_page.h"
#include "../../Widgets/loading_overlay.h"
#include "../../Widgets/table_util.h"
#include "../../../gt-stacer-core/Tools/package_tool.h"
#include "../../../gt-stacer-core/Tools/store_addon_tool.h"
#include "../../../gt-stacer-core/Tools/appimage_tool.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include <QFileDialog>
#include <QTabWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QResizeEvent>
#include <QHash>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <functional>

namespace {
// Managers we can install FROM: the distro's primary system manager plus the
// universal stores when present. (Language managers are intentionally out of
// scope for this first version.)
QVector<PkgMgr> installerManagerList()
{
    QVector<PkgMgr> out;
    const PkgMgr primary = PackageTool::primaryManager();
    if (primary != PkgMgr::Unknown && primary != PkgMgr::Manual) out << primary;
    if (PackageTool::has(PkgMgr::Flatpak)) out << PkgMgr::Flatpak;
    if (PackageTool::has(PkgMgr::Snap))    out << PkgMgr::Snap;
    return out;
}

// The core tool returns English category labels (it carries no UI/translation).
// Map them to translated strings here so the Arabic UI reads natively.
QString trCategory(const QString &c)
{
    static const QHash<QString, QString> m = {
        {"Plasma Theme",       PackageManagerPage::tr("Plasma Theme")},
        {"Plasmoid",           PackageManagerPage::tr("Plasmoid")},
        {"Global Theme",       PackageManagerPage::tr("Global Theme")},
        {"Wallpaper Plugin",   PackageManagerPage::tr("Wallpaper Plugin")},
        {"Window Decoration",  PackageManagerPage::tr("Window Decoration")},
        {"Colour Scheme",      PackageManagerPage::tr("Colour Scheme")},
        {"Wallpaper",          PackageManagerPage::tr("Wallpaper")},
        {"Icons",              PackageManagerPage::tr("Icons")},
        {"Desktop Theme",      PackageManagerPage::tr("Desktop Theme")},
        {"Font",               PackageManagerPage::tr("Font")},
        {"Konsole Profile",    PackageManagerPage::tr("Konsole Profile")},
        {"KWin Effect",        PackageManagerPage::tr("KWin Effect")},
        {"KWin Script",        PackageManagerPage::tr("KWin Script")},
        {"KWin Switcher",      PackageManagerPage::tr("KWin Switcher")},
        {"SDDM Theme",         PackageManagerPage::tr("SDDM Theme")},
    };
    return m.value(c, c);
}

QTableView *makeTable(QStandardItemModel *model, QAbstractItemView::SelectionMode mode,
                      int primaryCol = 0)
{
    auto *t = new QTableView;
    t->setModel(model);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(mode);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setAlternatingRowColors(true);
    t->setSortingEnabled(true);
    t->verticalHeader()->setVisible(false);
    // `primaryCol` fills the width but every column stays freely resizable with a
    // real, grabbable drag handle (see table_util.h).
    setupResizableTable(t, primaryCol);
    return t;
}
} // namespace

PackageManagerPage::PackageManagerPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 12);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Package & Software Manager"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto *intro = new QLabel(tr(
        "Install, upgrade and remove software from your system package manager, "
        "Flatpak and Snap — and manage desktop store add-ons. Changes that touch "
        "the system ask for your password."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildInstalledTab(), tr("Installed"));
    m_tabs->addTab(buildSearchTab(),    tr("Search && Install"));
    m_tabs->addTab(buildUpgradesTab(),  tr("Upgrades"));
    m_tabs->addTab(buildStoreTab(),     tr("Store add-ons"));
    m_tabs->addTab(buildAppImagesTab(), tr("AppImages"));
    root->addWidget(m_tabs, 1);

    m_overlay = new LoadingOverlay(this);
    m_overlay->resize(size());
    m_overlay->stop();

    // First-visit loads for the cheap, read-only tabs.
    loadInstalled();
    loadStore();
    loadAppImages();
}

PackageManagerPage::~PackageManagerPage() = default;

void PackageManagerPage::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_overlay) m_overlay->resize(size());
}

void PackageManagerPage::changeEvent(QEvent *e) { QWidget::changeEvent(e); }

void PackageManagerPage::runOff(const QString &msg, const std::function<void()> &work,
                                const std::function<void()> &after)
{
    m_overlay->start(msg);
    auto *w = new QFutureWatcher<void>(this);
    connect(w, &QFutureWatcher<void>::finished, this, [this, w, after] {
        w->deleteLater();
        m_overlay->stop();
        if (after) after();
    });
    w->setFuture(QtConcurrent::run(work));
}

// ── Installed tab ─────────────────────────────────────────────────────────────
QWidget *PackageManagerPage::buildInstalledTab()
{
    auto *tab = new QWidget;
    auto *v = new QVBoxLayout(tab);

    auto *row = new QHBoxLayout;
    m_instSearch = new QLineEdit;
    m_instSearch->setPlaceholderText(tr("Filter installed packages…"));
    m_instMgr = new QComboBox;
    m_instMgr->addItem(tr("All"), -1);
    for (PkgMgr m : PackageTool::availableManagers())
        m_instMgr->addItem(PackageTool::managerName(m), int(m));
    m_instMgr->addItem(PackageTool::managerName(PkgMgr::Manual), int(PkgMgr::Manual));
    auto *reload = new QPushButton(tr("Reload"));
    row->addWidget(m_instSearch, 1);
    row->addWidget(m_instMgr);
    row->addWidget(reload);
    v->addLayout(row);

    m_instModel = new QStandardItemModel(0, 4, this);
    m_instModel->setHorizontalHeaderLabels({tr("Name"), tr("Version"), tr("Size"), tr("Manager")});
    m_instProxy = new QSortFilterProxyModel(this);
    m_instProxy->setSourceModel(m_instModel);
    m_instProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_instTable = makeTable(m_instModel, QAbstractItemView::ExtendedSelection);
    m_instTable->setModel(m_instProxy);
    v->addWidget(m_instTable, 1);

    auto *arow = new QHBoxLayout;
    m_instStatus = new QLabel;
    m_instStatus->setObjectName("infoValue");
    m_instRemove = new QPushButton(tr("Uninstall"));
    m_instRemove->setObjectName("dangerButton");
    m_instRemove->setEnabled(false);
    arow->addWidget(m_instStatus, 1);
    arow->addWidget(m_instRemove);
    v->addLayout(arow);

    connect(reload, &QPushButton::clicked, this, &PackageManagerPage::loadInstalled);
    connect(m_instRemove, &QPushButton::clicked, this, &PackageManagerPage::removeSelectedInstalled);
    connect(m_instSearch, &QLineEdit::textChanged, this, [this](const QString &t){
        m_instProxy->setFilterKeyColumn(0);
        m_instProxy->setFilterFixedString(t);
    });
    connect(m_instMgr, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i){
        const int id = m_instMgr->itemData(i).toInt();
        if (id < 0) { m_instProxy->setFilterKeyColumn(0); m_instProxy->setFilterFixedString(m_instSearch->text()); }
        else        { m_instProxy->setFilterKeyColumn(3); m_instProxy->setFilterFixedString(PackageTool::managerName(PkgMgr(id))); }
    });
    connect(m_instTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]{
        const int n = m_instTable->selectionModel()->selectedRows().size();
        m_instRemove->setEnabled(n > 0);
        m_instRemove->setText(n > 1 ? tr("Uninstall %1 packages").arg(n) : tr("Uninstall"));
    });
    return tab;
}

void PackageManagerPage::loadInstalled()
{
    m_instStatus->setText(tr("Loading…"));
    m_instModel->setRowCount(0);
    auto *w = new QFutureWatcher<QVector<PackageInfo>>(this);
    connect(w, &QFutureWatcher<QVector<PackageInfo>>::finished, this, [this, w]{
        const auto pkgs = w->result(); w->deleteLater();
        for (const auto &p : pkgs) {
            QList<QStandardItem*> r;
            r << new QStandardItem(p.name) << new QStandardItem(p.version)
              << new QStandardItem(p.size) << new QStandardItem(PackageTool::managerName(p.manager));
            r[0]->setData(p.name, Qt::UserRole);
            r[0]->setData(int(p.manager), Qt::UserRole + 1);
            m_instModel->appendRow(r);
        }
        m_instStatus->setText(tr("%1 packages").arg(pkgs.size()));
    });
    w->setFuture(QtConcurrent::run([]{ return PackageTool::allPackages(); }));
}

void PackageManagerPage::removeSelectedInstalled()
{
    auto rows = m_instTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    std::sort(rows.begin(), rows.end(), [](auto &a, auto &b){ return a.row() < b.row(); });
    QVector<QPair<QString, PkgMgr>> targets;
    for (const auto &idx : rows) {
        const QString name = m_instProxy->data(m_instProxy->index(idx.row(), 0), Qt::UserRole).toString();
        const int mgr = m_instProxy->data(m_instProxy->index(idx.row(), 0), Qt::UserRole + 1).toInt();
        if (!name.isEmpty()) targets.append({name, PkgMgr(mgr)});
    }
    if (targets.isEmpty()) return;

    QStringList names;
    for (const auto &t : targets) names << "• " + t.first;
    if (QMessageBox::warning(this, tr("Uninstall"),
            tr("Uninstall the following %1 package(s)?\n\n%2\n\nYou will be asked for your password.")
                .arg(targets.size()).arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    runOff(tr("Uninstalling %1 package(s)…").arg(targets.size()),
        [targets]{ for (const auto &t : targets) PackageTool::remove(t.first, t.second); },
        [this]{ loadInstalled(); });
}

// ── Search & Install tab ──────────────────────────────────────────────────────
QWidget *PackageManagerPage::buildSearchTab()
{
    auto *tab = new QWidget;
    auto *v = new QVBoxLayout(tab);

    auto *row = new QHBoxLayout;
    m_srchMgr = new QComboBox;
    for (PkgMgr m : installerManagerList())
        m_srchMgr->addItem(PackageTool::managerName(m), int(m));
    m_srchEdit = new QLineEdit;
    m_srchEdit->setPlaceholderText(tr("Search for a package to install…"));
    m_srchButton = new QPushButton(tr("Search"));
    m_srchButton->setObjectName("primaryButton");
    row->addWidget(m_srchMgr);
    row->addWidget(m_srchEdit, 1);
    row->addWidget(m_srchButton);
    v->addLayout(row);

    m_srchModel = new QStandardItemModel(0, 3, this);
    m_srchModel->setHorizontalHeaderLabels({tr("Package"), tr("Version"), tr("Description")});
    m_srchTable = makeTable(m_srchModel, QAbstractItemView::ExtendedSelection, 2); // Description fills
    m_srchTable->setColumnWidth(0, 240);
    m_srchTable->setColumnWidth(1, 130);
    v->addWidget(m_srchTable, 1);

    auto *arow = new QHBoxLayout;
    m_srchStatus = new QLabel;
    m_srchStatus->setObjectName("infoValue");
    m_srchStatus->setWordWrap(true);
    m_srchInstall = new QPushButton(tr("Install"));
    m_srchInstall->setObjectName("primaryButton");
    m_srchInstall->setEnabled(false);
    arow->addWidget(m_srchStatus, 1);
    arow->addWidget(m_srchInstall);
    v->addLayout(arow);

    connect(m_srchButton, &QPushButton::clicked, this, &PackageManagerPage::runSearch);
    connect(m_srchEdit, &QLineEdit::returnPressed, this, &PackageManagerPage::runSearch);
    connect(m_srchInstall, &QPushButton::clicked, this, &PackageManagerPage::installSelectedSearch);
    connect(m_srchTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]{
        const int n = m_srchTable->selectionModel()->selectedRows().size();
        m_srchInstall->setEnabled(n > 0);
        m_srchInstall->setText(n > 1 ? tr("Install %1 packages").arg(n) : tr("Install"));
    });
    if (m_srchMgr->count() == 0) {
        m_srchButton->setEnabled(false);
        m_srchStatus->setText(tr("No installable package manager was detected."));
    }
    return tab;
}

void PackageManagerPage::runSearch()
{
    const QString q = m_srchEdit->text().trimmed();
    if (q.isEmpty() || m_srchMgr->count() == 0) return;
    const PkgMgr mgr = PkgMgr(m_srchMgr->currentData().toInt());
    m_srchModel->setRowCount(0);
    m_srchStatus->setText(tr("Searching…"));
    m_srchButton->setEnabled(false);

    auto *w = new QFutureWatcher<QVector<PackageInfo>>(this);
    connect(w, &QFutureWatcher<QVector<PackageInfo>>::finished, this, [this, w]{
        const auto res = w->result(); w->deleteLater();
        for (const auto &p : res) {
            QList<QStandardItem*> r;
            r << new QStandardItem(p.name) << new QStandardItem(p.version) << new QStandardItem(p.description);
            r[0]->setData(p.name, Qt::UserRole);
            r[0]->setData(int(p.manager), Qt::UserRole + 1);
            m_srchModel->appendRow(r);
        }
        m_srchStatus->setText(res.isEmpty() ? tr("No results — try a different term.")
                                            : tr("%1 result(s).").arg(res.size()));
        m_srchButton->setEnabled(true);
    });
    w->setFuture(QtConcurrent::run([q, mgr]{ return PackageTool::search(q, mgr); }));
}

void PackageManagerPage::installSelectedSearch()
{
    auto rows = m_srchTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    QVector<QPair<QString, PkgMgr>> targets;
    for (const auto &idx : rows) {
        const QString name = m_srchModel->data(m_srchModel->index(idx.row(), 0), Qt::UserRole).toString();
        const int mgr = m_srchModel->data(m_srchModel->index(idx.row(), 0), Qt::UserRole + 1).toInt();
        if (!name.isEmpty()) targets.append({name, PkgMgr(mgr)});
    }
    if (targets.isEmpty()) return;

    QStringList names;
    for (const auto &t : targets) names << "• " + t.first;
    if (QMessageBox::question(this, tr("Install"),
            tr("Install the following %1 package(s)?\n\n%2\n\nYou may be asked for your password.")
                .arg(targets.size()).arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    runOff(tr("Installing %1 package(s)…").arg(targets.size()),
        [targets]{ for (const auto &t : targets) PackageTool::install(t.first, t.second); },
        [this]{ m_srchStatus->setText(tr("Done. Check the Installed tab.")); loadInstalled(); });
}

// ── Upgrades tab ──────────────────────────────────────────────────────────────
QWidget *PackageManagerPage::buildUpgradesTab()
{
    auto *tab = new QWidget;
    auto *v = new QVBoxLayout(tab);

    auto *row = new QHBoxLayout;
    m_upMgr = new QComboBox;
    for (PkgMgr m : installerManagerList())
        m_upMgr->addItem(PackageTool::managerName(m), int(m));
    m_upCheck = new QPushButton(tr("Check for upgrades"));
    m_upCheck->setObjectName("primaryButton");
    row->addWidget(m_upMgr);
    row->addWidget(m_upCheck);
    row->addStretch();
    v->addLayout(row);

    m_upModel = new QStandardItemModel(0, 2, this);
    m_upModel->setHorizontalHeaderLabels({tr("Package"), tr("New version")});
    m_upTable = makeTable(m_upModel, QAbstractItemView::ExtendedSelection);
    v->addWidget(m_upTable, 1);

    auto *arow = new QHBoxLayout;
    m_upStatus = new QLabel;
    m_upStatus->setObjectName("infoValue");
    m_upOne = new QPushButton(tr("Upgrade selected"));
    m_upOne->setEnabled(false);
    m_upAll = new QPushButton(tr("Upgrade all"));
    m_upAll->setObjectName("primaryButton");
    m_upAll->setEnabled(false);
    arow->addWidget(m_upStatus, 1);
    arow->addWidget(m_upOne);
    arow->addWidget(m_upAll);
    v->addLayout(arow);

    connect(m_upCheck, &QPushButton::clicked, this, &PackageManagerPage::loadUpgrades);
    connect(m_upOne,   &QPushButton::clicked, this, &PackageManagerPage::upgradeSelected);
    connect(m_upAll,   &QPushButton::clicked, this, &PackageManagerPage::upgradeEverything);
    connect(m_upTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]{
        m_upOne->setEnabled(!m_upTable->selectionModel()->selectedRows().isEmpty());
    });
    if (m_upMgr->count() == 0) m_upCheck->setEnabled(false);
    return tab;
}

void PackageManagerPage::loadUpgrades()
{
    if (m_upMgr->count() == 0) return;
    const PkgMgr mgr = PkgMgr(m_upMgr->currentData().toInt());
    m_upModel->setRowCount(0);
    m_upStatus->setText(tr("Checking…"));
    m_upCheck->setEnabled(false);
    m_upAll->setEnabled(false);

    auto *w = new QFutureWatcher<QVector<PackageInfo>>(this);
    connect(w, &QFutureWatcher<QVector<PackageInfo>>::finished, this, [this, w]{
        const auto res = w->result(); w->deleteLater();
        for (const auto &p : res) {
            QList<QStandardItem*> r;
            r << new QStandardItem(p.name) << new QStandardItem(p.version);
            r[0]->setData(p.name, Qt::UserRole);
            r[0]->setData(int(p.manager), Qt::UserRole + 1);
            m_upModel->appendRow(r);
        }
        m_upStatus->setText(res.isEmpty() ? tr("Everything is up to date.")
                                          : tr("%1 upgrade(s) available.").arg(res.size()));
        m_upCheck->setEnabled(true);
        m_upAll->setEnabled(!res.isEmpty());
    });
    w->setFuture(QtConcurrent::run([mgr]{ return PackageTool::upgradable(mgr); }));
}

void PackageManagerPage::upgradeSelected()
{
    auto rows = m_upTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    QVector<QPair<QString, PkgMgr>> targets;
    for (const auto &idx : rows) {
        const QString name = m_upModel->data(m_upModel->index(idx.row(), 0), Qt::UserRole).toString();
        const int mgr = m_upModel->data(m_upModel->index(idx.row(), 0), Qt::UserRole + 1).toInt();
        if (!name.isEmpty()) targets.append({name, PkgMgr(mgr)});
    }
    if (targets.isEmpty()) return;
    runOff(tr("Upgrading %1 package(s)…").arg(targets.size()),
        [targets]{ for (const auto &t : targets) PackageTool::upgrade(t.first, t.second); },
        [this]{ loadUpgrades(); });
}

void PackageManagerPage::upgradeEverything()
{
    if (m_upMgr->count() == 0) return;
    const PkgMgr mgr = PkgMgr(m_upMgr->currentData().toInt());
    if (QMessageBox::question(this, tr("Upgrade all"),
            tr("Upgrade every package with an available update via %1?\n\n"
               "You will be asked for your password.").arg(PackageTool::managerName(mgr)),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    runOff(tr("Upgrading all packages… this can take a while."),
        [mgr]{ PackageTool::upgradeAll(mgr); },
        [this]{ loadUpgrades(); });
}

// ── Store add-ons tab ─────────────────────────────────────────────────────────
QWidget *PackageManagerPage::buildStoreTab()
{
    auto *tab = new QWidget;
    auto *v = new QVBoxLayout(tab);

    auto *info = new QLabel(tr(
        "Desktop add-ons installed from opendesktop.org / store.kde.org — themes, "
        "icons, cursors, plasmoids, wallpapers and more. Remove them here, or "
        "install new ones from the store."));
    info->setWordWrap(true);
    info->setObjectName("introText");
    v->addWidget(info);

    auto *row = new QHBoxLayout;
    m_stSearch = new QLineEdit;
    m_stSearch->setPlaceholderText(tr("Filter add-ons…"));
    auto *reload = new QPushButton(tr("Reload"));
    auto *ocsBtn = new QPushButton(tr("Install from link…"));
    auto *storeBtn = new QPushButton(tr("Open store"));
    row->addWidget(m_stSearch, 1);
    row->addWidget(reload);
    row->addWidget(ocsBtn);
    row->addWidget(storeBtn);
    v->addLayout(row);

    m_stModel = new QStandardItemModel(0, 3, this);
    m_stModel->setHorizontalHeaderLabels({tr("Add-on"), tr("Category"), tr("Size")});
    m_stProxy = new QSortFilterProxyModel(this);
    m_stProxy->setSourceModel(m_stModel);
    m_stProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_stProxy->setFilterKeyColumn(-1);   // match across all columns
    m_stTable = makeTable(m_stModel, QAbstractItemView::ExtendedSelection);
    m_stTable->setModel(m_stProxy);
    v->addWidget(m_stTable, 1);

    auto *arow = new QHBoxLayout;
    m_stStatus = new QLabel;
    m_stStatus->setObjectName("infoValue");
    m_stRemove = new QPushButton(tr("Remove"));
    m_stRemove->setObjectName("dangerButton");
    m_stRemove->setEnabled(false);
    arow->addWidget(m_stStatus, 1);
    arow->addWidget(m_stRemove);
    v->addLayout(arow);

    connect(reload,   &QPushButton::clicked, this, &PackageManagerPage::loadStore);
    connect(m_stRemove, &QPushButton::clicked, this, &PackageManagerPage::removeSelectedStore);
    connect(ocsBtn,   &QPushButton::clicked, this, &PackageManagerPage::installFromOcs);
    connect(storeBtn, &QPushButton::clicked, this, []{
        QDesktopServices::openUrl(QUrl(StoreAddonTool::storeUrl()));
    });
    connect(m_stSearch, &QLineEdit::textChanged, this, [this](const QString &t){
        m_stProxy->setFilterFixedString(t);
    });
    connect(m_stTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]{
        const int n = m_stTable->selectionModel()->selectedRows().size();
        m_stRemove->setEnabled(n > 0);
        m_stRemove->setText(n > 1 ? tr("Remove %1 add-ons").arg(n) : tr("Remove"));
    });
    return tab;
}

void PackageManagerPage::loadStore()
{
    m_stStatus->setText(tr("Loading…"));
    m_stModel->setRowCount(0);
    auto *w = new QFutureWatcher<QVector<StoreAddon>>(this);
    connect(w, &QFutureWatcher<QVector<StoreAddon>>::finished, this, [this, w]{
        const auto items = w->result(); w->deleteLater();
        for (const auto &a : items) {
            QList<QStandardItem*> r;
            r << new QStandardItem(a.name) << new QStandardItem(trCategory(a.category))
              << new QStandardItem(FormatUtil::formatBytes(a.sizeBytes));
            r[0]->setData(a.path, Qt::UserRole);
            r[2]->setData(qlonglong(a.sizeBytes), Qt::UserRole);
            m_stModel->appendRow(r);
        }
        m_stStatus->setText(tr("%1 add-on(s).").arg(items.size()));
    });
    w->setFuture(QtConcurrent::run([]{ return StoreAddonTool::installed(); }));
}

void PackageManagerPage::removeSelectedStore()
{
    auto rows = m_stTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    QVector<StoreAddon> targets;
    QStringList names;
    for (const auto &idx : rows) {
        StoreAddon a;
        a.path = m_stProxy->data(m_stProxy->index(idx.row(), 0), Qt::UserRole).toString();
        a.name = m_stProxy->data(m_stProxy->index(idx.row(), 0)).toString();
        if (!a.path.isEmpty()) { targets.append(a); names << "• " + a.name; }
    }
    if (targets.isEmpty()) return;
    if (QMessageBox::warning(this, tr("Remove add-ons"),
            tr("Permanently delete the following %1 add-on(s) from your home folder?\n\n%2")
                .arg(targets.size()).arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    runOff(tr("Removing %1 add-on(s)…").arg(targets.size()),
        [targets]{ for (const auto &a : targets) StoreAddonTool::remove(a); },
        [this]{ loadStore(); });
}

void PackageManagerPage::installFromOcs()
{
    bool ok = false;
    const QString url = QInputDialog::getText(this, tr("Install from link"),
        tr("Paste an ocs-url:// install link from the store's \"Install\" button:"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || url.isEmpty()) return;
    if (!StoreAddonTool::installFromOcs(url)) {
        QMessageBox::warning(this, tr("Install from link"),
            StoreAddonTool::ocsHandlerAvailable()
                ? tr("That does not look like a valid ocs-url:// link.")
                : tr("No ocs-url handler is installed. Install \"ocs-url\" (or "
                     "Plasma Discover) to enable one-click store installs."));
        return;
    }
    m_stStatus->setText(tr("Handed the link to the store installer…"));
    QTimer::singleShot(4000, this, &PackageManagerPage::loadStore);
}

// ── AppImages tab ─────────────────────────────────────────────────────────────
QWidget *PackageManagerPage::buildAppImagesTab()
{
    auto *tab = new QWidget;
    auto *v = new QVBoxLayout(tab);

    // Integration note — reflects the managed folder and GearLever coexistence.
    const QString folder = AppImageTool::managedFolder();
    const bool gl = AppImageTool::gearLeverInstalled();
    auto *info = new QLabel(
        (gl ? tr("Integrate AppImages into your application menu. GT-STACER uses the "
                 "same folder and format as GearLever (%1), so apps integrated by "
                 "either tool appear here — no duplication.")
            : tr("Integrate AppImages into your application menu: GT-STACER moves or "
                 "copies the file into %1, extracts its icon, and creates a launcher."))
            .arg(folder));
    info->setWordWrap(true);
    info->setObjectName("introText");
    v->addWidget(info);

    auto *row = new QHBoxLayout;
    m_aiSearch = new QLineEdit;
    m_aiSearch->setPlaceholderText(tr("Filter AppImages…"));
    auto *reload = new QPushButton(tr("Reload"));
    auto *add = new QPushButton(tr("Add AppImage…"));
    add->setObjectName("primaryButton");
    auto *openFolder = new QPushButton(tr("Open folder"));
    row->addWidget(m_aiSearch, 1);
    row->addWidget(reload);
    row->addWidget(add);
    row->addWidget(openFolder);
    v->addLayout(row);

    m_aiModel = new QStandardItemModel(0, 4, this);
    m_aiModel->setHorizontalHeaderLabels({tr("Application"), tr("Version"), tr("Size"), tr("Source")});
    m_aiProxy = new QSortFilterProxyModel(this);
    m_aiProxy->setSourceModel(m_aiModel);
    m_aiProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_aiProxy->setFilterKeyColumn(-1);
    m_aiTable = makeTable(m_aiModel, QAbstractItemView::ExtendedSelection);
    m_aiTable->setModel(m_aiProxy);
    v->addWidget(m_aiTable, 1);

    auto *arow = new QHBoxLayout;
    m_aiStatus = new QLabel;
    m_aiStatus->setObjectName("infoValue");
    m_aiRemove = new QPushButton(tr("Remove"));
    m_aiRemove->setObjectName("dangerButton");
    m_aiRemove->setEnabled(false);
    arow->addWidget(m_aiStatus, 1);
    arow->addWidget(m_aiRemove);
    v->addLayout(arow);

    connect(reload, &QPushButton::clicked, this, &PackageManagerPage::loadAppImages);
    connect(add,    &QPushButton::clicked, this, &PackageManagerPage::addAppImage);
    connect(m_aiRemove, &QPushButton::clicked, this, &PackageManagerPage::removeSelectedAppImage);
    connect(openFolder, &QPushButton::clicked, this, [folder]{
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    });
    connect(m_aiSearch, &QLineEdit::textChanged, this, [this](const QString &t){
        m_aiProxy->setFilterFixedString(t);
    });
    connect(m_aiTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]{
        const int n = m_aiTable->selectionModel()->selectedRows().size();
        m_aiRemove->setEnabled(n > 0);
        m_aiRemove->setText(n > 1 ? tr("Remove %1 AppImages").arg(n) : tr("Remove"));
    });
    return tab;
}

void PackageManagerPage::loadAppImages()
{
    if (!m_aiStatus) return;
    m_aiStatus->setText(tr("Loading…"));
    m_aiModel->setRowCount(0);
    auto *w = new QFutureWatcher<QVector<AppImageEntry>>(this);
    connect(w, &QFutureWatcher<QVector<AppImageEntry>>::finished, this, [this, w]{
        const auto items = w->result(); w->deleteLater();
        for (const auto &e : items) {
            QList<QStandardItem*> r;
            r << new QStandardItem(e.name)
              << new QStandardItem(e.version)
              << new QStandardItem(FormatUtil::formatBytes(e.sizeBytes))
              << new QStandardItem(e.byGearLever ? tr("GearLever folder") : tr("Integrated"));
            r[0]->setData(e.appImagePath, Qt::UserRole);
            r[0]->setData(e.desktopPath,  Qt::UserRole + 1);
            r[0]->setData(e.iconPath,     Qt::UserRole + 2);
            m_aiModel->appendRow(r);
        }
        m_aiStatus->setText(tr("%1 AppImage(s).").arg(items.size()));
    });
    w->setFuture(QtConcurrent::run([]{ return AppImageTool::installed(); }));
}

void PackageManagerPage::addAppImage()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose an AppImage to integrate"),
        QDir::homePath(), tr("AppImages (*.AppImage *.appimage);;All files (*)"));
    if (file.isEmpty()) return;
    runOff(tr("Integrating %1…").arg(QFileInfo(file).fileName()),
        [file]{ AppImageTool::integrate(file); },
        [this]{
            m_aiStatus->setText(tr("Integrated — it should now appear in your app menu."));
            loadAppImages();
        });
}

void PackageManagerPage::removeSelectedAppImage()
{
    auto rows = m_aiTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    QVector<AppImageEntry> targets;
    QStringList names;
    for (const auto &idx : rows) {
        AppImageEntry e;
        e.appImagePath = m_aiProxy->data(m_aiProxy->index(idx.row(), 0), Qt::UserRole).toString();
        e.desktopPath  = m_aiProxy->data(m_aiProxy->index(idx.row(), 0), Qt::UserRole + 1).toString();
        e.iconPath     = m_aiProxy->data(m_aiProxy->index(idx.row(), 0), Qt::UserRole + 2).toString();
        e.name         = m_aiProxy->data(m_aiProxy->index(idx.row(), 0)).toString();
        if (!e.appImagePath.isEmpty()) { targets.append(e); names << "• " + e.name; }
    }
    if (targets.isEmpty()) return;
    if (QMessageBox::warning(this, tr("Remove AppImages"),
            tr("Remove the following %1 AppImage(s) — the file, its launcher and its icon?\n\n%2")
                .arg(targets.size()).arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    runOff(tr("Removing %1 AppImage(s)…").arg(targets.size()),
        [targets]{ for (const auto &e : targets) AppImageTool::remove(e); },
        [this]{ loadAppImages(); });
}
