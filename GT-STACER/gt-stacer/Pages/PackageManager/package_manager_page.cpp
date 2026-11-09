#include "package_manager_page.h"
#include "../../Widgets/loading_overlay.h"
#include "../../Widgets/table_util.h"
#include "../../Dialogs/command_log_dialog.h"
#include "../../Dialogs/local_package_dialog.h"
#include "../../../gt-stacer-core/Tools/local_package_tool.h"
#include "../../../gt-stacer-core/Tools/package_tool.h"
#include "../../../gt-stacer-core/Tools/store_addon_tool.h"
#include "../../../gt-stacer-core/Tools/appimage_tool.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Tools/notification_tool.h"
#include "../../Managers/setting_manager.h"
#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFrame>
#include <QMimeData>
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
#include <memory>
#include <QIcon>
#include <QLocale>

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

// Make column 0 of every row a checkbox, so the user can tick specific rows for
// an action without holding Ctrl. Call after (re)populating the model.
void makeCheckable(QStandardItemModel *model)
{
    for (int r = 0; r < model->rowCount(); ++r)
        if (auto *it = model->item(r, 0)) {
            it->setCheckable(true);
            it->setCheckState(Qt::Unchecked);
        }
}

// Tick or clear every row's checkbox.
void setAllChecked(QStandardItemModel *model, bool on)
{
    for (int r = 0; r < model->rowCount(); ++r)
        if (auto *it = model->item(r, 0))
            it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
}

// Collect (name, manager) targets for an action: ticked rows first; if nothing
// is ticked, fall back to the current row selection (so a single click + button
// still works). `src` is the QStandardItemModel; `view`/`viewModel` describe how
// the rows are presented (a proxy, when sorting/filtering is on).
QVector<QPair<QString, PkgMgr>> collectTargets(QStandardItemModel *src, QTableView *view,
                                               QAbstractItemModel *viewModel)
{
    QVector<QPair<QString, PkgMgr>> out;
    for (int r = 0; r < src->rowCount(); ++r) {
        auto *it = src->item(r, 0);
        if (it && it->checkState() == Qt::Checked) {
            const QString name = it->data(Qt::UserRole).toString();
            if (!name.isEmpty())
                out.append({name, PkgMgr(it->data(Qt::UserRole + 1).toInt())});
        }
    }
    if (!out.isEmpty()) return out;
    // Nothing ticked → use the selection (mapped through the presented model).
    for (const auto &idx : view->selectionModel()->selectedRows()) {
        const QString name = viewModel->data(viewModel->index(idx.row(), 0), Qt::UserRole).toString();
        const int mgr = viewModel->data(viewModel->index(idx.row(), 0), Qt::UserRole + 1).toInt();
        if (!name.isEmpty()) out.append({name, PkgMgr(mgr)});
    }
    return out;
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
        "Flatpak and Snap, install package files you downloaded, and manage desktop "
        "store add-ons. Changes that touch the system ask for your password."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildInstalledTab(), tr("Installed"));
    m_tabs->addTab(buildSearchTab(),    tr("Search && Install"));
    m_tabs->addTab(buildLocalTab(),     tr("Local packages"));
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

// The waiting card of the TAB that holds `inside` — a list loading in one tab
// never blocks the others (the first-visit loads run together at start).
LoadingOverlay *PackageManagerPage::tabOverlay(QWidget *inside) const
{
    QWidget *w = inside;
    while (w && w->parentWidget() && w->parentWidget()->parentWidget() != m_tabs)
        w = w->parentWidget();
    return LoadingOverlay::on(w ? w : const_cast<PackageManagerPage *>(this));
}

void PackageManagerPage::runOff(const QString &msg, const std::function<void()> &work,
                                const std::function<void()> &after)
{
    m_overlay->start(msg);
    auto *w = new QFutureWatcher<void>(this);
    connect(w, &QFutureWatcher<void>::finished, this, [this, w, after, msg] {
        w->deleteLater();
        m_overlay->stop();
        if (after) after();
        // Notify on completion so a user who navigated away still learns it's done.
        NotificationTool::notify(tr("GT-STACER — task finished"), msg,
                                 NotificationTool::Urgency::Normal, "gt-stacer");
    });
    w->setFuture(QtConcurrent::run(work));
}

void PackageManagerPage::runCommands(const QString &title,
                                     const QVector<QPair<QString, QStringList>> &steps,
                                     const std::function<void()> &after)
{
    auto *dlg = new CommandLogDialog(title, this);
    for (const auto &s : steps) dlg->addStep(s.first, s.second);
    connect(dlg, &CommandLogDialog::completed, this, [this, after, title, dlg](bool ok){
        if (after) after();
        // Signal completion even if the user navigated to another page/section:
        // a desktop notification plus a persistent in-app status line.
        NotificationTool::notify(
            ok ? tr("%1 — finished").arg(title) : tr("%1 — finished with errors").arg(title),
            ok ? tr("The operation completed successfully.")
               : (dlg->failureReason().isEmpty()
                      ? tr("The operation finished with errors — open the log for details.")
                      : dlg->failureReason()),
            ok ? NotificationTool::Urgency::Normal : NotificationTool::Urgency::Critical,
            "gt-stacer");
    });
    connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
    dlg->show();
    dlg->run();
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
    for (PkgMgr m : PackageTool::listingManagers())   // one per package DB
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
    auto *selAll = new QCheckBox(tr("Select all"));
    m_instStatus = new QLabel;
    m_instStatus->setObjectName("infoValue");
    m_instRemove = new QPushButton(tr("Uninstall"));
    m_instRemove->setObjectName("dangerButton");
    arow->addWidget(selAll);
    arow->addWidget(m_instStatus, 1);
    arow->addWidget(m_instRemove);
    v->addLayout(arow);

    connect(selAll, &QCheckBox::toggled, this, [this](bool on){ setAllChecked(m_instModel, on); });
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
    return tab;
}

void PackageManagerPage::loadInstalled()
{
    m_instStatus->setText(tr("Loading…"));
    m_instModel->setRowCount(0);
    tabOverlay(m_instTable)->start(tr("Loading installed packages"),
                                   tr("Reading the list from every package manager…"));
    auto *w = new QFutureWatcher<QVector<PackageInfo>>(this);
    connect(w, &QFutureWatcher<QVector<PackageInfo>>::finished, this, [this, w]{
        const auto pkgs = w->result(); w->deleteLater();
        tabOverlay(m_instTable)->stop();
        for (const auto &p : pkgs) {
            QList<QStandardItem*> r;
            r << new QStandardItem(p.name) << new QStandardItem(p.version)
              << new QStandardItem(p.size) << new QStandardItem(PackageTool::managerName(p.manager));
            r[0]->setData(p.name, Qt::UserRole);
            r[0]->setData(int(p.manager), Qt::UserRole + 1);
            m_instModel->appendRow(r);
        }
        makeCheckable(m_instModel);
        m_instStatus->setText(tr("%1 packages").arg(pkgs.size()));
    });
    w->setFuture(QtConcurrent::run([]{ return PackageTool::allPackages(); }));
}

void PackageManagerPage::removeSelectedInstalled()
{
    const auto targets = collectTargets(m_instModel, m_instTable, m_instProxy);
    if (targets.isEmpty()) {
        QMessageBox::information(this, tr("Uninstall"),
            tr("Tick the packages to uninstall (or select a row) first."));
        return;
    }
    QStringList names;
    for (const auto &t : targets) names << "• " + t.first;
    if (QMessageBox::warning(this, tr("Uninstall"),
            tr("Uninstall the following %1 package(s)?\n\n%2\n\nYou will be asked for your password.")
                .arg(targets.size()).arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Manual apps have no single command — remove them off-thread; the rest run
    // in the live log window.
    QVector<QPair<QString, QStringList>> steps;
    QVector<QPair<QString, PkgMgr>> manual;
    for (const auto &t : targets) {
        const QStringList cmd = PackageTool::removeCommand(t.first, t.second);
        if (cmd.isEmpty()) manual.append(t);
        else steps.append({tr("Uninstall %1").arg(t.first), cmd});
    }
    if (!manual.isEmpty())
        runOff(tr("Removing %1 external app(s)…").arg(manual.size()),
            [manual]{ for (const auto &t : manual) PackageTool::remove(t.first, t.second); },
            [this]{ loadInstalled(); });
    if (!steps.isEmpty())
        runCommands(tr("Uninstalling %1 package(s)").arg(steps.size()), steps,
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
    auto *selAll = new QCheckBox(tr("Select all"));
    m_srchStatus = new QLabel;
    m_srchStatus->setObjectName("infoValue");
    m_srchStatus->setWordWrap(true);
    m_srchInstall = new QPushButton(tr("Install"));
    m_srchInstall->setObjectName("primaryButton");
    arow->addWidget(selAll);
    arow->addWidget(m_srchStatus, 1);
    arow->addWidget(m_srchInstall);
    v->addLayout(arow);

    connect(selAll, &QCheckBox::toggled, this, [this](bool on){ setAllChecked(m_srchModel, on); });
    connect(m_srchButton, &QPushButton::clicked, this, &PackageManagerPage::runSearch);
    connect(m_srchEdit, &QLineEdit::returnPressed, this, &PackageManagerPage::runSearch);
    connect(m_srchInstall, &QPushButton::clicked, this, &PackageManagerPage::installSelectedSearch);
    if (m_srchMgr->count() == 0) {
        m_srchButton->setEnabled(false);
        m_srchStatus->setText(tr("No installable package manager was detected."));
    }
    return tab;
}

void PackageManagerPage::runSearch()
{
    if (m_srchMgr->count() == 0) return;
    const QString q = m_srchEdit->text().trimmed();
    if (q.isEmpty()) {                       // give feedback instead of doing nothing
        m_srchStatus->setText(tr("Type a package name in the box, then Search."));
        m_srchEdit->setFocus();
        return;
    }
    const PkgMgr mgr = PkgMgr(m_srchMgr->currentData().toInt());
    m_srchModel->setRowCount(0);
    m_srchStatus->setText(tr("Searching…"));
    m_srchButton->setEnabled(false);
    tabOverlay(m_srchTable)->start(tr("Searching for “%1”").arg(q),
                                   tr("Asking %1…").arg(PackageTool::managerName(mgr)));

    auto *w = new QFutureWatcher<QVector<PackageInfo>>(this);
    connect(w, &QFutureWatcher<QVector<PackageInfo>>::finished, this, [this, w]{
        const auto res = w->result(); w->deleteLater();
        tabOverlay(m_srchTable)->stop();
        for (const auto &p : res) {
            QList<QStandardItem*> r;
            r << new QStandardItem(p.name) << new QStandardItem(p.version) << new QStandardItem(p.description);
            r[0]->setData(p.name, Qt::UserRole);
            r[0]->setData(int(p.manager), Qt::UserRole + 1);
            m_srchModel->appendRow(r);
        }
        makeCheckable(m_srchModel);
        m_srchStatus->setText(res.isEmpty() ? tr("No results — try a different term.")
                                            : tr("%1 result(s).").arg(res.size()));
        m_srchButton->setEnabled(true);
    });
    w->setFuture(QtConcurrent::run([q, mgr]{ return PackageTool::search(q, mgr); }));
}

void PackageManagerPage::installSelectedSearch()
{
    const auto targets = collectTargets(m_srchModel, m_srchTable, m_srchModel);
    if (targets.isEmpty()) {
        QMessageBox::information(this, tr("Install"),
            tr("Tick the packages to install (or select a row) first."));
        return;
    }
    QStringList names;
    QVector<QPair<QString, QStringList>> steps;
    for (const auto &t : targets) {
        names << "• " + t.first;
        steps.append({tr("Install %1").arg(t.first), PackageTool::installCommand(t.first, t.second)});
    }
    if (QMessageBox::question(this, tr("Install"),
            tr("Install the following %1 package(s)?\n\n%2\n\nYou may be asked for your password.")
                .arg(targets.size()).arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    runCommands(tr("Installing %1 package(s)").arg(steps.size()), steps,
        [this]{ m_srchStatus->setText(tr("Done. Check the Installed tab.")); loadInstalled(); });
}

// ── Local packages tab ────────────────────────────────────────────────────────
namespace {
QString kindTr(LocalPkgKind k)
{
    switch (k) {
    case LocalPkgKind::Deb:           return PackageManagerPage::tr("Debian packages (.deb)");
    case LocalPkgKind::Rpm:           return PackageManagerPage::tr("RPM packages (.rpm)");
    case LocalPkgKind::Pacman:        return PackageManagerPage::tr("Arch packages (.pkg.tar.zst)");
    case LocalPkgKind::Apk:           return PackageManagerPage::tr("Alpine packages (.apk)");
    case LocalPkgKind::Eopkg:         return PackageManagerPage::tr("Solus packages (.eopkg)");
    case LocalPkgKind::FlatpakBundle: return PackageManagerPage::tr("Flatpak bundles (.flatpak)");
    case LocalPkgKind::FlatpakRef:    return PackageManagerPage::tr("Flatpak references (.flatpakref)");
    case LocalPkgKind::Snap:          return PackageManagerPage::tr("Snap packages (.snap)");
    case LocalPkgKind::AppImage:      return PackageManagerPage::tr("AppImages");
    default:                          return {};
    }
}
} // namespace

QWidget *PackageManagerPage::buildLocalTab()
{
    auto *tab = new QWidget;
    auto *v = new QVBoxLayout(tab);

    // What this system is and what it can take — detected, not configured.
    QStringList kinds;
    for (LocalPkgKind k : LocalPackageTool::supportedKinds()) kinds << kindTr(k);
    const PkgMgr primary = PackageTool::primaryManager();
    auto *info = new QLabel(
        tr("Detected system: <b>%1</b> · package manager: <b>%2</b><br>"
           "Package files this system can install: %3.")
            .arg(LocalPackageTool::distroName().toHtmlEscaped(),
                 primary == PkgMgr::Unknown ? tr("none detected") : PackageTool::managerName(primary),
                 kinds.join(QStringLiteral(" · "))));
    info->setWordWrap(true);
    info->setObjectName("introText");
    v->addWidget(info);

    // A drop zone with a big "+" — click it, or drop a file on the zone.
    m_lpDrop = new QFrame;
    m_lpDrop->setObjectName("localDrop");
    m_lpDrop->setAcceptDrops(true);
    m_lpDrop->installEventFilter(this);
    auto *dz = new QVBoxLayout(m_lpDrop);
    dz->setContentsMargins(24, 32, 24, 32);
    dz->setSpacing(12);
    auto *add = new QPushButton(QStringLiteral("+"));
    add->setObjectName("localAdd");
    add->setFixedSize(72, 72);
    add->setCursor(Qt::PointingHandCursor);
    add->setToolTip(tr("Choose a package file to install"));
    auto *title = new QLabel(tr("Choose a package file to install"));
    title->setObjectName("pkgName");
    title->setAlignment(Qt::AlignCenter);
    auto *sub = new QLabel(tr("Click + to browse, or drop a file here. You'll see the package details and "
                              "its dependencies before anything is installed."));
    sub->setObjectName("introText");
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    dz->addStretch(1);
    dz->addWidget(add, 0, Qt::AlignHCenter);
    dz->addWidget(title);
    dz->addWidget(sub);
    dz->addStretch(1);
    v->addWidget(m_lpDrop, 1);

    m_lpStatus = new QLabel;
    m_lpStatus->setObjectName("infoValue");
    m_lpStatus->setWordWrap(true);
    v->addWidget(m_lpStatus);

    connect(add, &QPushButton::clicked, this, &PackageManagerPage::chooseLocalPackage);
    return tab;
}

bool PackageManagerPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_lpDrop) {
        if (event->type() == QEvent::DragEnter) {
            auto *e = static_cast<QDragEnterEvent *>(event);
            const auto urls = e->mimeData()->urls();
            if (urls.size() == 1 && urls.first().isLocalFile()) { e->acceptProposedAction(); return true; }
        } else if (event->type() == QEvent::Drop) {
            auto *e = static_cast<QDropEvent *>(event);
            const auto urls = e->mimeData()->urls();
            if (!urls.isEmpty() && urls.first().isLocalFile()) {
                e->acceptProposedAction();
                const QString path = urls.first().toLocalFile();
                QTimer::singleShot(0, this, [this, path]{ openLocalPackage(path); });
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PackageManagerPage::chooseLocalPackage()
{
    // Filters: everything installable here first, then one per kind, then the
    // rest (so the user can still pick e.g. an .rpm and be told why it won't go).
    QStringList filters, allHere;
    for (LocalPkgKind k : LocalPackageTool::supportedKinds()) {
        const QStringList pats = LocalPackageTool::patternsFor(k);
        allHere += pats;
        filters << QString("%1 (%2)").arg(kindTr(k), pats.join(' '));
    }
    filters.prepend(tr("Packages for this system (%1)").arg(allHere.join(' ')));
    filters << tr("All files (*)");
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose a package file to install"),
        QDir::homePath() + "/Downloads", filters.join(";;"));
    if (!file.isEmpty()) openLocalPackage(file);
}

void PackageManagerPage::openLocalPackage(const QString &path)
{
    m_overlay->start(tr("Reading the package"),
                     tr("%1 — checking its dependencies…").arg(QFileInfo(path).fileName()));
    auto *w = new QFutureWatcher<LocalPackageInfo>(this);
    connect(w, &QFutureWatcher<LocalPackageInfo>::finished, this, [this, w]{
        const LocalPackageInfo info = w->result();
        w->deleteLater();
        m_overlay->stop();

        LocalPackageDialog dlg(info, this);
        if (dlg.exec() != QDialog::Accepted) {
            m_lpStatus->setText(tr("Cancelled — nothing was installed."));
            return;
        }
        const QString label = info.name + (info.version.isEmpty() ? QString() : ' ' + info.version);

        if (info.kind == LocalPkgKind::AppImage) {
            integrateAppImage(info.path, [this, label](bool ok){
                m_lpStatus->setText(ok ? tr("%1 was integrated — see the AppImages tab and your app menu.").arg(label)
                                       : tr("%1 could not be integrated.").arg(label));
            });
            return;
        }

        const QStringList optional = dlg.selectedOptional();
        const auto argvs = LocalPackageTool::installSteps(info, optional);
        if (argvs.isEmpty()) {
            m_lpStatus->setText(tr("No install command is available for this package on this system."));
            return;
        }
        QVector<QPair<QString, QStringList>> steps;
        for (int i = 0; i < argvs.size(); ++i)
            steps.append({i == 0 ? tr("Install %1").arg(label) : tr("Install the chosen optional dependencies"),
                          argvs[i]});
        m_lpStatus->setText(optional.isEmpty()
            ? tr("Installing %1…").arg(label)
            : tr("Installing %1 with %2 optional dependenc(ies)…").arg(label).arg(optional.size()));
        runCommands(tr("Installing %1").arg(label), steps, [this, label]{
            m_lpStatus->setText(tr("Finished: %1 — check the log for the result.").arg(label));
            loadInstalled();
        });
    });
    w->setFuture(QtConcurrent::run([path]{ return LocalPackageTool::inspect(path); }));
}

// ── Upgrades tab ──────────────────────────────────────────────────────────────
// Every source at once ("All sources") or one; the list shows where each update
// comes from, and per-source boxes let the user leave a whole source out before
// anything runs. One command per source (one password per system source).
namespace {
constexpr int kUpName = Qt::UserRole, kUpMgr = Qt::UserRole + 1, kUpId = Qt::UserRole + 2;
}

QWidget *PackageManagerPage::buildUpgradesTab()
{
    auto *tab = new QWidget;
    auto *v = new QVBoxLayout(tab);

    auto *row = new QHBoxLayout;
    m_upMgr = new QComboBox;
    m_upMgr->addItem(tr("All sources"), -1);
    for (PkgMgr m : installerManagerList())
        m_upMgr->addItem(PackageTool::managerName(m), int(m));
    m_upCheck = new QPushButton(tr("Check for upgrades"));
    m_upCheck->setObjectName("primaryButton");
    m_upCheck->setToolTip(tr("Refreshes the package lists first (no password where the system allows it), "
                             "then lists every available update and where it comes from."));
    row->addWidget(m_upMgr);
    row->addWidget(m_upCheck);
    row->addStretch();
    v->addLayout(row);

    // Per-source boxes: untick a source to leave it out of the upgrade.
    m_upSources = new QWidget;
    auto *srcLay = new QHBoxLayout(m_upSources);
    srcLay->setContentsMargins(0, 0, 0, 0);
    m_upSources->setVisible(false);
    v->addWidget(m_upSources);

    m_upModel = new QStandardItemModel(0, 4, this);
    m_upModel->setHorizontalHeaderLabels({tr("Package"), tr("Installed"), tr("New version"), tr("Source")});
    m_upTable = makeTable(m_upModel, QAbstractItemView::ExtendedSelection);
    v->addWidget(m_upTable, 1);

    auto *arow = new QHBoxLayout;
    auto *selAll = new QCheckBox(tr("Select all"));
    m_upStatus = new QLabel;
    m_upStatus->setObjectName("infoValue");
    m_upStatus->setWordWrap(true);
    m_upAll = new QPushButton(tr("Upgrade…"));
    m_upAll->setObjectName("primaryButton");
    m_upAll->setToolTip(tr("Upgrade the ticked updates. A summary per source is shown first."));
    m_upAll->setEnabled(false);
    arow->addWidget(selAll);
    arow->addWidget(m_upStatus, 1);
    arow->addWidget(m_upAll);
    v->addLayout(arow);

    connect(selAll, &QCheckBox::toggled, this, [this](bool on){
        m_upSyncing = true; setAllChecked(m_upModel, on); m_upSyncing = false;
        syncUpgradeSourceBoxes();
    });
    connect(m_upModel, &QStandardItemModel::itemChanged, this, [this](QStandardItem *it){
        if (!m_upSyncing && it && it->column() == 0) syncUpgradeSourceBoxes();
    });
    connect(m_upCheck, &QPushButton::clicked, this, [this]{ loadUpgrades(true); });
    connect(m_upAll,   &QPushButton::clicked, this, &PackageManagerPage::upgradeTicked);
    if (m_upMgr->count() == 1) m_upCheck->setEnabled(false);   // only "All sources", nothing behind it
    return tab;
}

void PackageManagerPage::loadUpgrades(bool refresh)
{
    if (m_upMgr->count() <= 1) return;
    const int scope = m_upMgr->currentData().toInt();
    QVector<PkgMgr> mgrs;
    if (scope < 0) mgrs = installerManagerList();
    else mgrs << PkgMgr(scope);

    m_upModel->setRowCount(0);
    m_upSources->setVisible(false);
    m_upStatus->setText(refresh ? tr("Refreshing the package lists and checking… this can take a minute.")
                                : tr("Checking…"));
    m_upCheck->setEnabled(false);
    m_upMgr->setEnabled(false);
    m_upAll->setEnabled(false);
    // A clear sign in the middle of the window while it searches — with the
    // step it is on (refreshing, then each source in turn).
    tabOverlay(m_upTable)->start(tr("Looking for updates"),
                     refresh ? tr("Refreshing the package lists…") : tr("Checking…"));

    struct Found { QVector<PackageInfo> pkgs; QStringList unsupported; };
    auto *w = new QFutureWatcher<Found>(this);
    connect(w, &QFutureWatcher<Found>::finished, this, [this, w]{
        const Found f = w->result(); w->deleteLater();
        tabOverlay(m_upTable)->stop();
        m_upSyncing = true;
        for (const auto &p : f.pkgs) {
            QList<QStandardItem*> r;
            r << new QStandardItem(p.name)
              << new QStandardItem(p.currentVersion.isEmpty() ? QStringLiteral("—") : p.currentVersion)
              << new QStandardItem(p.version)
              << new QStandardItem(PackageTool::managerName(p.manager));
            r[0]->setData(p.name, kUpName);
            r[0]->setData(int(p.manager), kUpMgr);
            r[0]->setData(p.id.isEmpty() ? p.name : p.id, kUpId);
            if (!p.id.isEmpty()) r[0]->setToolTip(p.id);
            m_upModel->appendRow(r);
        }
        makeCheckable(m_upModel);
        setAllChecked(m_upModel, true);           // everything ticked: untick what to skip
        m_upSyncing = false;
        rebuildUpgradeSources();

        QString msg = f.pkgs.isEmpty() ? tr("Everything is up to date.")
                                       : tr("%n update(s) available.", nullptr, int(f.pkgs.size()));
        if (!f.unsupported.isEmpty())
            msg += " " + tr("Updates can't be listed here for: %1 — use its own tool.").arg(f.unsupported.join(", "));
        m_upStatus->setText(msg);
        m_upCheck->setEnabled(true);
        m_upMgr->setEnabled(true);
        m_upAll->setEnabled(!f.pkgs.isEmpty());
    });
    QPointer<PackageManagerPage> self(this);
    w->setFuture(QtConcurrent::run([mgrs, refresh, self]{
        Found f;
        for (int k = 0; k < mgrs.size(); ++k) {
            const PkgMgr m = mgrs[k];
            if (!PackageTool::canListUpgrades(m)) { f.unsupported << PackageTool::managerName(m); continue; }
            // Tell the overlay which source is being asked (GUI thread).
            const QString name = PackageTool::managerName(m);
            const int n = k + 1, total = int(mgrs.size());
            QMetaObject::invokeMethod(qApp, [self, name, n, total, refresh, m]{
                if (!self) return;
                QString step = (refresh && (m == PkgMgr::APT || m == PkgMgr::Pacman))
                    ? PackageManagerPage::tr("Refreshing %1's package lists, then checking…").arg(name)
                    : PackageManagerPage::tr("Checking %1…").arg(name);
                if (total > 1) step += "  " + PackageManagerPage::tr("(%1 of %2)").arg(n).arg(total);
                self->tabOverlay(self->m_upTable)->setDetail(step);
            }, Qt::QueuedConnection);
            f.pkgs += PackageTool::upgradable(m, refresh);
        }
        return f;
    }));
}

void PackageManagerPage::rebuildUpgradeSources()
{
    auto *lay = static_cast<QHBoxLayout *>(m_upSources->layout());
    while (QLayoutItem *it = lay->takeAt(0)) { delete it->widget(); delete it; }
    m_upSourceBoxes.clear();

    QMap<int, int> counts;   // keeps the manager order stable (enum order)
    QVector<int> order;
    for (int r = 0; r < m_upModel->rowCount(); ++r) {
        const int mgr = m_upModel->item(r, 0)->data(kUpMgr).toInt();
        if (!counts.contains(mgr)) order << mgr;
        ++counts[mgr];
    }
    if (order.isEmpty()) { m_upSources->setVisible(false); return; }

    lay->addWidget(new QLabel(tr("Sources:")));
    for (int mgr : order) {
        auto *box = new QCheckBox(QStringLiteral("%1 (%2)").arg(PackageTool::managerName(PkgMgr(mgr))).arg(counts[mgr]));
        box->setChecked(true);
        box->setToolTip(tr("Untick to leave every update from this source out."));
        connect(box, &QCheckBox::clicked, this, [this, mgr](bool on){
            m_upSyncing = true;
            for (int r = 0; r < m_upModel->rowCount(); ++r)
                if (m_upModel->item(r, 0)->data(kUpMgr).toInt() == mgr)
                    m_upModel->item(r, 0)->setCheckState(on ? Qt::Checked : Qt::Unchecked);
            m_upSyncing = false;
            syncUpgradeSourceBoxes();
        });
        lay->addWidget(box);
        m_upSourceBoxes.insert(mgr, box);
    }
    lay->addStretch();
    m_upSources->setVisible(true);
}

void PackageManagerPage::syncUpgradeSourceBoxes()
{
    QMap<int, QPair<int, int>> ticked;   // mgr → (ticked, total)
    for (int r = 0; r < m_upModel->rowCount(); ++r) {
        QStandardItem *it = m_upModel->item(r, 0);
        auto &t = ticked[it->data(kUpMgr).toInt()];
        ++t.second;
        if (it->checkState() == Qt::Checked) ++t.first;
    }
    int anyTicked = 0;
    for (auto it = m_upSourceBoxes.cbegin(); it != m_upSourceBoxes.cend(); ++it) {
        const auto t = ticked.value(it.key());
        anyTicked += t.first;
        const QSignalBlocker block(it.value());
        it.value()->setCheckState(t.first == 0 ? Qt::Unchecked
                                : t.first == t.second ? Qt::Checked : Qt::PartiallyChecked);
    }
    m_upAll->setEnabled(anyTicked > 0);
}

void PackageManagerPage::upgradeTicked()
{
    // Ticked updates, grouped by source, in the order the sources appear.
    QVector<int> order;
    QMap<int, QStringList> ids;
    QMap<int, int> total;
    for (int r = 0; r < m_upModel->rowCount(); ++r) {
        QStandardItem *it = m_upModel->item(r, 0);
        const int mgr = it->data(kUpMgr).toInt();
        if (!total.contains(mgr)) order << mgr;
        ++total[mgr];
        if (it->checkState() == Qt::Checked) ids[mgr] << it->data(kUpId).toString();
    }
    QVector<QPair<QString, QStringList>> steps;
    QStringList summary, withPassword;
    for (int mgr : order) {
        const QStringList chosen = ids.value(mgr);
        if (chosen.isEmpty()) continue;
        const PkgMgr m = PkgMgr(mgr);
        const QString name = PackageTool::managerName(m);
        const bool everything = chosen.size() == total.value(mgr);
        if (m == PkgMgr::Pacman && !everything) {
            QMessageBox::warning(this, tr("Upgrade"),
                tr("Arch Linux (pacman) doesn't support upgrading only some packages — it can leave the "
                   "system with mismatched libraries. Tick all of its updates, or untick the whole source."));
            return;
        }
        const QStringList cmd = PackageTool::upgradeBatchCommand(m, chosen, everything);
        if (cmd.isEmpty()) {
            QMessageBox::warning(this, tr("Upgrade"),
                tr("The updates from %1 can't be upgraded from here — use its own tool.").arg(name));
            return;
        }
        summary << (everything ? tr("• %1: all %n update(s)", nullptr, int(chosen.size())).arg(name)
                               : tr("• %1: %2 of %3 updates").arg(name).arg(chosen.size()).arg(total.value(mgr)));
        if (cmd.first() == "pkexec") withPassword << name;
        steps.append({everything ? tr("Upgrade everything from %1").arg(name)
                                 : tr("Upgrade %n package(s) from %1", nullptr, int(chosen.size())).arg(name),
                      cmd});
    }
    if (steps.isEmpty()) {
        QMessageBox::information(this, tr("Upgrade"), tr("Tick the updates to install first."));
        return;
    }
    QString text = tr("These updates will be installed:\n\n%1").arg(summary.join('\n'));
    if (!withPassword.isEmpty())
        text += "\n\n" + tr("Your password will be asked once for each of: %1.").arg(withPassword.join(", "));
    text += "\n\n" + tr("Changed configuration files you edited yourself are kept. If one source fails, the "
                        "ones after it are not started.");
    if (QMessageBox::question(this, tr("Upgrade"), text, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    runCommands(tr("Upgrading"), steps, [this]{ loadUpgrades(false); });
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
    auto *selAll = new QCheckBox(tr("Select all"));
    m_stStatus = new QLabel;
    m_stStatus->setObjectName("infoValue");
    m_stRemove = new QPushButton(tr("Remove"));
    m_stRemove->setObjectName("dangerButton");
    arow->addWidget(selAll);
    arow->addWidget(m_stStatus, 1);
    arow->addWidget(m_stRemove);
    v->addLayout(arow);

    connect(selAll, &QCheckBox::toggled, this, [this](bool on){ setAllChecked(m_stModel, on); });
    connect(reload,   &QPushButton::clicked, this, &PackageManagerPage::loadStore);
    connect(m_stRemove, &QPushButton::clicked, this, &PackageManagerPage::removeSelectedStore);
    connect(ocsBtn,   &QPushButton::clicked, this, &PackageManagerPage::installFromOcs);
    connect(storeBtn, &QPushButton::clicked, this, []{
        QDesktopServices::openUrl(QUrl(StoreAddonTool::storeUrl()));
    });
    connect(m_stSearch, &QLineEdit::textChanged, this, [this](const QString &t){
        m_stProxy->setFilterFixedString(t);
    });
    return tab;
}

void PackageManagerPage::loadStore()
{
    m_stStatus->setText(tr("Loading…"));
    m_stModel->setRowCount(0);
    tabOverlay(m_stTable)->start(tr("Loading store add-ons"),
                                 tr("Looking for installed themes, icons and widgets…"));
    auto *w = new QFutureWatcher<QVector<StoreAddon>>(this);
    connect(w, &QFutureWatcher<QVector<StoreAddon>>::finished, this, [this, w]{
        const auto items = w->result(); w->deleteLater();
        tabOverlay(m_stTable)->stop();
        for (const auto &a : items) {
            QList<QStandardItem*> r;
            r << new QStandardItem(a.name) << new QStandardItem(trCategory(a.category))
              << new QStandardItem(FormatUtil::formatBytes(a.sizeBytes));
            r[0]->setData(a.path, Qt::UserRole);
            r[2]->setData(qlonglong(a.sizeBytes), Qt::UserRole);
            m_stModel->appendRow(r);
        }
        makeCheckable(m_stModel);
        m_stStatus->setText(tr("%1 add-on(s).").arg(items.size()));
    });
    w->setFuture(QtConcurrent::run([]{ return StoreAddonTool::installed(); }));
}

void PackageManagerPage::removeSelectedStore()
{
    QVector<StoreAddon> targets;
    QStringList names;
    // Ticked rows first, else the current selection.
    for (int r = 0; r < m_stModel->rowCount(); ++r) {
        auto *it = m_stModel->item(r, 0);
        if (it && it->checkState() == Qt::Checked) {
            StoreAddon a; a.path = it->data(Qt::UserRole).toString(); a.name = it->text();
            if (!a.path.isEmpty()) { targets.append(a); names << "• " + a.name; }
        }
    }
    if (targets.isEmpty())
        for (const auto &idx : m_stTable->selectionModel()->selectedRows()) {
            StoreAddon a;
            a.path = m_stProxy->data(m_stProxy->index(idx.row(), 0), Qt::UserRole).toString();
            a.name = m_stProxy->data(m_stProxy->index(idx.row(), 0)).toString();
            if (!a.path.isEmpty()) { targets.append(a); names << "• " + a.name; }
        }
    if (targets.isEmpty()) {
        QMessageBox::information(this, tr("Remove add-ons"),
            tr("Tick the add-ons to remove (or select a row) first."));
        return;
    }
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
    auto *selAll = new QCheckBox(tr("Select all"));
    m_aiStatus = new QLabel;
    m_aiStatus->setObjectName("infoValue");
    m_aiRemove = new QPushButton(tr("Remove"));
    m_aiRemove->setObjectName("dangerButton");
    arow->addWidget(selAll);
    arow->addWidget(m_aiStatus, 1);
    arow->addWidget(m_aiRemove);
    v->addLayout(arow);

    connect(selAll, &QCheckBox::toggled, this, [this](bool on){ setAllChecked(m_aiModel, on); });
    connect(reload, &QPushButton::clicked, this, &PackageManagerPage::loadAppImages);
    connect(add,    &QPushButton::clicked, this, &PackageManagerPage::addAppImage);
    connect(m_aiRemove, &QPushButton::clicked, this, &PackageManagerPage::removeSelectedAppImage);
    connect(openFolder, &QPushButton::clicked, this, [folder]{
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    });
    connect(m_aiSearch, &QLineEdit::textChanged, this, [this](const QString &t){
        m_aiProxy->setFilterFixedString(t);
    });
    return tab;
}

void PackageManagerPage::loadAppImages()
{
    if (!m_aiStatus) return;
    m_aiStatus->setText(tr("Loading…"));
    m_aiModel->setRowCount(0);
    tabOverlay(m_aiTable)->start(tr("Loading AppImages"),
                                 tr("Looking for integrated AppImages…"));
    auto *w = new QFutureWatcher<QVector<AppImageEntry>>(this);
    connect(w, &QFutureWatcher<QVector<AppImageEntry>>::finished, this, [this, w]{
        const auto items = w->result(); w->deleteLater();
        tabOverlay(m_aiTable)->stop();
        for (const auto &e : items) {
            QList<QStandardItem*> r;
            r << new QStandardItem(e.name)
              << new QStandardItem(e.version)
              << new QStandardItem(FormatUtil::formatBytes(e.sizeBytes))
              << new QStandardItem(e.byGearLever ? tr("GearLever folder") : tr("Integrated"));
            r[0]->setData(e.appImagePath, Qt::UserRole);
            r[0]->setData(e.desktopPath,  Qt::UserRole + 1);
            r[0]->setData(e.iconPath,     Qt::UserRole + 2);
            // Show the app's own icon (an extracted file, or an icon-theme name).
            const QIcon icon = e.iconPath.startsWith('/') ? QIcon(e.iconPath)
                                                          : QIcon::fromTheme(e.iconPath);
            if (!icon.isNull()) r[0]->setIcon(icon);
            r[0]->setToolTip(e.comment.isEmpty() ? e.appImagePath
                                                 : e.comment + "\n" + e.appImagePath);
            m_aiModel->appendRow(r);
        }
        makeCheckable(m_aiModel);
        m_aiStatus->setText(tr("%1 AppImage(s).").arg(items.size()));
    });
    // Localized names ("Name[ar]=…") follow the UI language.
    const QString locale = SettingManager::instance()->effectiveLanguage() == "ar"
                         ? QStringLiteral("ar_MA") : QLocale().name();
    w->setFuture(QtConcurrent::run([locale]{ return AppImageTool::installed(locale); }));
}

void PackageManagerPage::addAppImage()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose an AppImage to integrate"),
        QDir::homePath(), tr("AppImages (*.AppImage *.appimage);;All files (*)"));
    if (file.isEmpty()) return;
    integrateAppImage(file, nullptr);
}

void PackageManagerPage::integrateAppImage(const QString &file, const std::function<void(bool)> &done)
{
    // Shared by the AppImages tab and the Local packages tab. The result is
    // reported for real — 26.11 announced success even when integration failed.
    auto ok  = std::make_shared<bool>(false);
    auto err = std::make_shared<QString>();
    runOff(tr("Integrating %1…").arg(QFileInfo(file).fileName()),
        [file, ok, err]{ *ok = AppImageTool::integrate(file, err.get()); },
        [this, ok, err, file, done]{
            if (*ok) {
                m_aiStatus->setText(tr("Integrated — it should now appear in your app menu."));
            } else {
                m_aiStatus->setText(tr("Integration failed."));
                QMessageBox::warning(this, tr("AppImage"),
                    tr("Could not integrate %1:\n\n%2").arg(QFileInfo(file).fileName(), *err));
            }
            loadAppImages();
            if (done) done(*ok);
        });
}

void PackageManagerPage::removeSelectedAppImage()
{
    QVector<AppImageEntry> targets;
    QStringList names;
    auto grab = [&](QStandardItem *it) {
        AppImageEntry e;
        e.appImagePath = it->data(Qt::UserRole).toString();
        e.desktopPath  = it->data(Qt::UserRole + 1).toString();
        e.iconPath     = it->data(Qt::UserRole + 2).toString();
        e.name         = it->text();
        if (!e.appImagePath.isEmpty()) { targets.append(e); names << "• " + e.name; }
    };
    for (int r = 0; r < m_aiModel->rowCount(); ++r)
        if (auto *it = m_aiModel->item(r, 0); it && it->checkState() == Qt::Checked) grab(it);
    if (targets.isEmpty())
        for (const auto &idx : m_aiTable->selectionModel()->selectedRows())
            if (auto *it = m_aiModel->itemFromIndex(
                    m_aiProxy->mapToSource(m_aiProxy->index(idx.row(), 0)))) grab(it);
    if (targets.isEmpty()) {
        QMessageBox::information(this, tr("Remove AppImages"),
            tr("Tick the AppImages to remove (or select a row) first."));
        return;
    }
    if (QMessageBox::warning(this, tr("Remove AppImages"),
            tr("Remove the following %1 AppImage(s) — the file, its launcher and its icon?\n\n%2")
                .arg(targets.size()).arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    runOff(tr("Removing %1 AppImage(s)…").arg(targets.size()),
        [targets]{ for (const auto &e : targets) AppImageTool::remove(e); },
        [this]{ loadAppImages(); });
}
