#include "uninstaller_page.h"
#include "ui_uninstaller_page.h"
#include "../../Widgets/empty_state.h"
#include "../../Widgets/skeleton_rows.h"
#include "../../Widgets/cleaner_icons.h"   // reuse aptCache() glyph
#include "../../Widgets/loading_overlay.h"
#include "../../../gt-stacer-core/Tools/package_tool.h"
#include <QFutureWatcher>
#include <QResizeEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

UninstallerPage::UninstallerPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::UninstallerPage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({tr("Name"), tr("Version"), tr("Size"), tr("Manager")});

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(0);

    ui->packageTable->setModel(m_proxy);
    ui->packageTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->packageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Multi-select: Ctrl/Shift-click + Ctrl+A picks several rows. Combined with
    // a batched remove() this lets a user clean up an entire toolchain in one
    // shot. The status label and skeleton overlay keep the user informed about
    // which package is being processed.
    ui->packageTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->packageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->packageTable->setSortingEnabled(true);
    ui->packageTable->setAlternatingRowColors(true);
    ui->packageTable->setMouseTracking(true);
    // Row hover/selection styling comes from the global theme QSS (QTableView::item).

    // Wrap the .ui's bare QTableView in a QStackedWidget so we can swap it
    // for an EmptyState (initial / no-matches) or a SkeletonRows (loading)
    // without losing the existing model/proxy wiring above.
    QWidget *tableParent = ui->packageTable->parentWidget();
    QLayout *tableLayout = tableParent ? tableParent->layout() : nullptr;
    m_stack = new QStackedWidget(this);

    // Page 0 — Initial empty state with a "Load Packages" call-to-action.
    m_initial = new EmptyState;
    m_initial->setIconSvg(CleanerIcons::aptCache(), 64);
    m_initial->setTitle(tr("Browse installed packages"));
    m_initial->setSubtitle(tr("Click \"Load Packages\" to scan installed software across every detected package manager."));
    m_initial->setActionLabel(tr("Load Packages"));
    connect(m_initial, &EmptyState::actionClicked, this, &UninstallerPage::loadPackages);
    m_stack->addWidget(m_initial);

    // Page 1 — Loading skeleton (animated shimmer)
    auto *skelHost = new QWidget;
    auto *skelLay = new QVBoxLayout(skelHost);
    skelLay->setContentsMargins(0, 4, 0, 0);
    m_skeleton = new SkeletonRows(10);
    skelLay->addWidget(m_skeleton);
    skelLay->addStretch();
    m_stack->addWidget(skelHost);

    // Page 2 — Real results (the package table)
    m_tableHost = new QWidget;
    auto *thLay = new QVBoxLayout(m_tableHost);
    thLay->setContentsMargins(0, 0, 0, 0);
    thLay->addWidget(ui->packageTable);     // re-parents the table
    m_stack->addWidget(m_tableHost);

    // Page 3 — Filter has zeroed out the view
    m_noMatches = new EmptyState;
    m_noMatches->setIconSvg(CleanerIcons::aptCache(), 56);
    m_noMatches->setTitle(tr("No packages match your filter"));
    m_noMatches->setSubtitle(tr("Try a different search term, or pick \"All\" from the manager dropdown."));
    m_stack->addWidget(m_noMatches);

    if (tableLayout) tableLayout->addWidget(m_stack);
    setDisplayState(Initial);

    ui->managerCombo->addItem(tr("All"), -1);
    auto available = PackageTool::availableManagers();
    for (PkgMgr mgr : available) {
        ui->managerCombo->addItem(PackageTool::managerName(mgr),
                                   static_cast<int>(mgr));
    }
    // Externally / manually installed apps are found by a filesystem scan rather
    // than a detected manager, so add their filter entry explicitly.
    ui->managerCombo->addItem(PackageTool::managerName(PkgMgr::Manual),
                               static_cast<int>(PkgMgr::Manual));

    connect(ui->loadButton,   &QPushButton::clicked, this, &UninstallerPage::loadPackages);
    connect(ui->removeButton, &QPushButton::clicked, this, &UninstallerPage::uninstallSelected);
    connect(ui->searchEdit,   &QLineEdit::textChanged, this, &UninstallerPage::onSearchChanged);
    connect(ui->managerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UninstallerPage::onManagerFilterChanged);

    // Uninstall is meaningless without a selection — disable until one exists,
    // and re-label the button when several rows are selected so the user knows
    // a batch is queued up.
    ui->removeButton->setEnabled(false);
    connect(ui->packageTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        const int n = ui->packageTable->selectionModel()->selectedRows().size();
        ui->removeButton->setEnabled(n > 0);
        ui->removeButton->setText(n > 1 ? tr("Uninstall %1 packages").arg(n)
                                        : tr("Uninstall"));
    });

    // Full-page spinner overlay for the (potentially slow) uninstall — a status
    // label alone left users unsure whether anything was happening. The overlay
    // dims the page, blocks interaction, and shows live "Removing X (i of n)…".
    m_overlay = new LoadingOverlay(this);
    m_overlay->resize(size());

    // First-visit auto-load — saves the user a click and immediately fills the
    // skeleton with the shimmer placeholder while the scan runs. The Load
    // button stays usable as a "Reload" trigger afterwards.
    QTimer::singleShot(0, this, &UninstallerPage::loadPackages);
    ui->loadButton->setText(tr("Reload"));
}

UninstallerPage::~UninstallerPage() { delete ui; }

void UninstallerPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_overlay) m_overlay->resize(size());
}

void UninstallerPage::setDisplayState(DisplayState s)
{
    if (!m_stack) return;
    if (s == Loading) m_skeleton->start();
    else              m_skeleton->stop();
    m_stack->setCurrentIndex(static_cast<int>(s));
}

void UninstallerPage::loadPackages()
{
    ui->loadButton->setEnabled(false);
    ui->statusLabel->setText(tr("Loading packages..."));
    m_model->setRowCount(0);
    setDisplayState(Loading);

    // Run the scan off the UI thread — allPackages() shells out to dpkg-query
    // / rpm -qa / pacman -Q, which can each take several seconds on a busy
    // system. Without QtConcurrent the skeleton would never even render.
    auto *watcher = new QFutureWatcher<QVector<PackageInfo>>(this);
    connect(watcher, &QFutureWatcher<QVector<PackageInfo>>::finished,
            this, [this, watcher]() {
        watcher->deleteLater();
        const auto pkgs = watcher->result();
        for (const auto &p : pkgs) {
            QList<QStandardItem*> row;
            row << new QStandardItem(p.name)
                << new QStandardItem(p.version)
                << new QStandardItem(p.size)
                << new QStandardItem(PackageTool::managerName(p.manager));
            row[0]->setData(p.name,                      Qt::UserRole);
            row[0]->setData(static_cast<int>(p.manager), Qt::UserRole + 1);
            m_model->appendRow(row);
        }
        ui->statusLabel->setText(tr("%1 packages").arg(pkgs.size()));
        ui->loadButton->setEnabled(true);
        setDisplayState(pkgs.isEmpty() ? Initial : Results);
    });
    watcher->setFuture(QtConcurrent::run([]() {
        return PackageTool::allPackages();
    }));
}

void UninstallerPage::uninstallSelected()
{
    auto rows = ui->packageTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;

    // Collect (name, manager) pairs from every selected row. Sort the indices
    // so the confirmation list reads top-to-bottom in screen order.
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() < b.row();
    });
    QVector<QPair<QString, PkgMgr>> targets;
    targets.reserve(rows.size());
    for (const auto &idx : rows) {
        QString name = m_proxy->data(m_proxy->index(idx.row(), 0), Qt::UserRole).toString();
        int     mgri = m_proxy->data(m_proxy->index(idx.row(), 0), Qt::UserRole + 1).toInt();
        if (name.isEmpty()) continue;
        targets.append({name, static_cast<PkgMgr>(mgri)});
    }
    if (targets.isEmpty()) return;

    // Confirmation dialog — single-package form for a single row, list form
    // for many. Either way mention the password-prompt expectation explicitly.
    QString body;
    if (targets.size() == 1) {
        body = tr("Uninstall <b>%1</b> using <b>%2</b>?\n\n"
                  "You will be asked for your password.")
                   .arg(targets.first().first,
                        PackageTool::managerName(targets.first().second));
    } else {
        QStringList rows;
        for (const auto &t : targets)
            rows << QString("• <b>%1</b> (%2)").arg(t.first, PackageTool::managerName(t.second));
        body = tr("Uninstall the following <b>%1 packages</b>?<br><br>%2<br><br>"
                  "You will be asked for your password — once per package "
                  "(or once total within the polkit cache window).")
                   .arg(targets.size()).arg(rows.join("<br>"));
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Uninstall"));
    box.setIcon(QMessageBox::Warning);
    box.setTextFormat(Qt::RichText);
    box.setText(body);
    auto *go = box.addButton(tr("Uninstall"), QMessageBox::AcceptRole);
    go->setObjectName("dangerButton");
    box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != go) return;

    ui->loadButton->setEnabled(false);
    ui->removeButton->setEnabled(false);
    m_overlay->start(targets.size() == 1
        ? tr("Uninstalling %1…").arg(targets.first().first)
        : tr("Uninstalling %1 packages…").arg(targets.size()));

    // Each removal runs sequentially on the worker thread. We report progress
    // back to the UI thread via QMetaObject::invokeMethod so the status label
    // stays current without sprinkling QFutureWatcher<bool> per package.
    auto *watcher = new QFutureWatcher<QVector<QPair<QString, bool>>>(this);
    connect(watcher, &QFutureWatcher<QVector<QPair<QString, bool>>>::finished,
            this, [this, watcher]() {
        watcher->deleteLater();
        m_overlay->stop();
        const auto results = watcher->result();
        int ok = 0;
        QStringList failed;
        for (const auto &r : results) {
            if (r.second) ++ok;
            else          failed << r.first;
        }
        ui->loadButton->setEnabled(true);
        if (failed.isEmpty()) {
            ui->statusLabel->setText(tr("Removed %1 package(s).").arg(ok));
        } else {
            QMessageBox::warning(this, tr("Some removals failed"),
                tr("Removed %1 of %2 packages. Failed:\n\n%3\n\n"
                   "Possible causes: authorization denied, package is a "
                   "system dependency, or the manager rejected the operation.")
                    .arg(ok).arg(results.size()).arg(failed.join(", ")));
        }
        loadPackages();
    });
    watcher->setFuture(QtConcurrent::run([this, targets]() {
        QVector<QPair<QString, bool>> out;
        out.reserve(targets.size());
        int i = 0;
        for (const auto &t : targets) {
            ++i;
            // Update the status label from the worker — Qt::QueuedConnection
            // marshals back to the UI thread for free.
            const QString label = tr("Removing %1 (%2 of %3)…")
                                       .arg(t.first).arg(i).arg(targets.size());
            QMetaObject::invokeMethod(ui->statusLabel, "setText",
                                       Qt::QueuedConnection, Q_ARG(QString, label));
            QMetaObject::invokeMethod(this, [this, label]() {
                m_overlay->setMessage(label);
            }, Qt::QueuedConnection);
            out.append({t.first, PackageTool::remove(t.first, t.second)});
        }
        return out;
    }));
}

void UninstallerPage::onSearchChanged(const QString &t)
{
    m_proxy->setFilterKeyColumn(0);
    m_proxy->setFilterFixedString(t);
    if (m_model->rowCount() > 0)
        setDisplayState(m_proxy->rowCount() > 0 ? Results : NoMatches);
}

void UninstallerPage::onManagerFilterChanged(int idx)
{
    if (idx < 0) return;
    int mgrId = ui->managerCombo->itemData(idx).toInt();
    if (mgrId < 0) {
        m_proxy->setFilterKeyColumn(0);
        m_proxy->setFilterFixedString(ui->searchEdit->text());
    } else {
        m_proxy->setFilterKeyColumn(3);
        m_proxy->setFilterFixedString(PackageTool::managerName(static_cast<PkgMgr>(mgrId)));
    }
    if (m_model->rowCount() > 0)
        setDisplayState(m_proxy->rowCount() > 0 ? Results : NoMatches);
}

void UninstallerPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
