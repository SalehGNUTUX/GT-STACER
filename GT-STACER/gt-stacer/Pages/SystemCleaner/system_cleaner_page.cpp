#include "system_cleaner_page.h"
#include "../../Dialogs/app_cache_dialog.h"
#include "../../Widgets/cleaner_card.h"
#include "../../Widgets/cleaner_icons.h"
#include "../../Widgets/loading_overlay.h"
#include "../../../gt-stacer-core/Utils/file_util.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include <QCheckBox>
#include <QDir>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

SystemCleanerPage::SystemCleanerPage(QWidget *parent) : QWidget(parent)
{
    const QString home = QDir::homePath();

    // Note on strategies:
    //   - "UserFiles"       — path is under $HOME, the running user owns it, no pkexec needed.
    //   - "RootRotatedLogs" — only deletes rotated archives (*.gz / *.[0-9] / *.old / *.old.*).
    //                         Leaves live log files alone so logging keeps working.
    //   - "RootAllInDir"    — deletes every regular file inside a root-owned dir (kept dir).
    //   - "AptClean"        — calls `apt-get clean` so APT itself manages the cache directory.
    //   - "OldKernelsApt"   — `apt-get autoremove --purge -y` (Debian/Ubuntu family).
    m_categories = {
        {tr("Trash"),         tr("Files in the user trash bin"),
         CleanerIcons::trash(),       home + "/.local/share/Trash/files", true, CleanStrategy::UserFiles},
        {tr("App Cache"),     tr("Application cache files in ~/.cache"),
         CleanerIcons::appCache(),    home + "/.cache",                   true, CleanStrategy::UserFiles},
        {tr("Thumbnails"),    tr("Image thumbnail cache"),
         CleanerIcons::thumbnails(),  home + "/.cache/thumbnails",        true, CleanStrategy::UserFiles},
        {tr("System Logs"),   tr("Rotated log archives in /var/log"),
         CleanerIcons::appLogs(),     "/var/log",                         false, CleanStrategy::RootRotatedLogs},
        {tr("Crash Reports"), tr("System crash dumps in /var/crash"),
         CleanerIcons::crashReports(), "/var/crash",                       true, CleanStrategy::RootAllInDir},
        {tr("APT Cache"),     tr("Downloaded .deb packages waiting to be installed"),
         CleanerIcons::aptCache(),    "/var/cache/apt/archives",          false, CleanStrategy::AptClean},
        {tr("Old Kernels"),   tr("Remove orphaned kernel packages (autoremove)"),
         CleanerIcons::oldKernels(),  "",                                 false, CleanStrategy::OldKernelsApt},
    };

    buildUi();
    m_overlay = new LoadingOverlay(this);
    refreshSelectionUi();
}

SystemCleanerPage::~SystemCleanerPage() = default;

void SystemCleanerPage::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    auto *title = new QLabel(tr("System Cleaner"));
    title->setStyleSheet("font-size:22px;font-weight:bold;color:#cdd6f4;");
    root->addWidget(title);

    auto *hint = new QLabel(tr("Pick the categories to scan, then choose which results to clean."));
    hint->setStyleSheet("color:#a6adc8;font-size:12px;");
    hint->setWordWrap(true);
    root->addWidget(hint);

    // ── Grid of cards inside a scroll area ──────────────────────────────
    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto *gridHost = new QWidget;
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 6, 0, 6);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);

    constexpr int kColumns = 5;
    for (int i = 0; i < m_categories.size(); ++i) {
        auto &cat = m_categories[i];
        cat.card = new CleanerCard(cat.iconSvg, cat.name, cat.description);
        connect(cat.card, &CleanerCard::toggled, this, &SystemCleanerPage::onCardToggled);
        // App Cache supports a per-application drill-down — double-click to open.
        if (cat.strategy == CleanStrategy::UserFiles
            && cat.path == QDir::homePath() + "/.cache") {
            cat.card->setHasDetails(true);
            cat.card->setToolTip(cat.description + "\n\n" +
                tr("ⓘ Double-click (or use the Details button below) "
                   "to manage per-application caches."));
            connect(cat.card, &CleanerCard::doubleClicked, this,
                    &SystemCleanerPage::openAppCacheDetails);
        }
        grid->addWidget(cat.card, i / kColumns, i % kColumns);
    }
    // Fill the trailing columns so cards keep a uniform width.
    for (int c = 0; c < kColumns; ++c) grid->setColumnStretch(c, 1);
    grid->setRowStretch(grid->rowCount(), 1);

    scroll->setWidget(gridHost);
    root->addWidget(scroll, 1);

    // ── Controls row ───────────────────────────────────────────────────
    auto *controls = new QHBoxLayout;
    controls->setSpacing(12);

    m_selectAll = new QCheckBox(tr("Select All"));
    connect(m_selectAll, &QCheckBox::toggled, this, &SystemCleanerPage::toggleSelectAll);
    controls->addWidget(m_selectAll);

    controls->addStretch();

    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("color:#a6adc8;");
    controls->addWidget(m_statusLabel);

    controls->addSpacing(12);

    m_scanButton = new QPushButton(tr("Scan"));
    m_scanButton->setObjectName("primaryButton");
    m_scanButton->setMinimumWidth(110);
    connect(m_scanButton, &QPushButton::clicked, this, &SystemCleanerPage::scan);
    controls->addWidget(m_scanButton);

    // App-cache drill-down. Enabled only after the App Cache card has been
    // included in a scan — otherwise the drill-down would show stale data.
    m_detailsButton = new QPushButton(tr("App Cache Details…"));
    m_detailsButton->setToolTip(tr("Manage which application caches to clean individually"));
    m_detailsButton->setEnabled(false);
    m_detailsButton->setMinimumWidth(150);
    connect(m_detailsButton, &QPushButton::clicked, this,
            &SystemCleanerPage::openAppCacheDetails);
    controls->addWidget(m_detailsButton);

    m_cleanButton = new QPushButton(tr("Clean"));
    m_cleanButton->setObjectName("dangerButton");
    m_cleanButton->setMinimumWidth(110);
    m_cleanButton->setEnabled(false);
    connect(m_cleanButton, &QPushButton::clicked, this, &SystemCleanerPage::clean);
    controls->addWidget(m_cleanButton);

    root->addLayout(controls);
}

void SystemCleanerPage::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_overlay) m_overlay->resize(size());
}

void SystemCleanerPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
}

void SystemCleanerPage::onCardToggled()
{
    refreshSelectionUi();
}

void SystemCleanerPage::toggleSelectAll(bool checked)
{
    // Block per-card signals so we don't get N refreshSelectionUi() calls.
    for (auto &c : m_categories) {
        if (!c.card) continue;
        QSignalBlocker b(c.card);
        c.card->setChecked(checked);
    }
    refreshSelectionUi();
}

void SystemCleanerPage::refreshSelectionUi()
{
    int total = 0, checked = 0;
    for (const auto &c : m_categories) {
        if (!c.card) continue;
        ++total;
        if (c.card->isChecked()) ++checked;
    }

    // Sync Select-All tri-state without re-firing toggled().
    {
        QSignalBlocker b(m_selectAll);
        m_selectAll->setChecked(checked == total && total > 0);
    }

    // Clean is only sensible after a scan has labeled the cards with sizes.
    m_cleanButton->setEnabled(m_scanCompleted && checked > 0);
    m_detailsButton->setEnabled(m_appCacheScanned);
}

void SystemCleanerPage::openAppCacheDetails()
{
    AppCacheDialog dlg(this);
    dlg.exec();
}

// ── Scan ────────────────────────────────────────────────────────────────
qint64 SystemCleanerPage::scanCategory(const Category &c) const
{
    // OldKernelsApt has no path — we don't compute a size, just a placeholder.
    if (c.strategy == CleanStrategy::OldKernelsApt) return 0;
    if (c.path.isEmpty()) return 0;
    return FileUtil::dirSize(c.path);
}

void SystemCleanerPage::scan()
{
    QVector<int> selected;
    for (int i = 0; i < m_categories.size(); ++i)
        if (m_categories[i].card && m_categories[i].card->isChecked())
            selected.append(i);

    if (selected.isEmpty()) {
        m_statusLabel->setText(tr("Pick at least one category to scan."));
        return;
    }

    m_scanButton->setEnabled(false);
    m_cleanButton->setEnabled(false);
    m_statusLabel->setText(tr("Scanning…"));
    m_overlay->start(tr("Scanning selected categories…"));

    // Clear any previous sizes from cards not in this scan.
    for (auto &c : m_categories) if (c.card) c.card->setSizeText("");

    // Detect whether App Cache was part of this scan so we can enable Details.
    m_appCacheScanned = false;
    for (int i : selected) {
        if (m_categories[i].strategy == CleanStrategy::UserFiles
            && m_categories[i].path == QDir::homePath() + "/.cache") {
            m_appCacheScanned = true; break;
        }
    }

    auto *watcher = new QFutureWatcher<QVector<QPair<int, qint64>>>(this);
    connect(watcher, &QFutureWatcher<QVector<QPair<int, qint64>>>::finished, this,
            [this, watcher]() {
        watcher->deleteLater();
        m_overlay->stop();
        const auto results = watcher->result();

        qint64 totalBytes = 0;
        for (const auto &r : results) {
            int idx = r.first;
            qint64 sz = r.second;
            m_categories[idx].size = sz;
            if (m_categories[idx].card) {
                if (m_categories[idx].strategy == CleanStrategy::OldKernelsApt)
                    m_categories[idx].card->setSizeText(tr("autoremove"));
                else
                    m_categories[idx].card->setSizeText(FormatUtil::formatBytes(sz));
            }
            totalBytes += sz;
        }

        m_scanCompleted = true;
        m_statusLabel->setText(
            tr("Found %1 to clean. Select what to remove, then press Clean.")
                .arg(FormatUtil::formatBytes(totalBytes)));
        m_scanButton->setEnabled(true);
        refreshSelectionUi();
    });

    // Snapshot the selection by value so the worker doesn't touch UI state.
    QVector<Category> snapshot;
    snapshot.reserve(selected.size());
    QVector<int> indices = selected;
    for (int i : indices) snapshot.append(m_categories[i]);

    watcher->setFuture(QtConcurrent::run([this, snapshot, indices]() {
        QVector<QPair<int, qint64>> out;
        out.reserve(indices.size());
        for (int k = 0; k < indices.size(); ++k)
            out.append({indices[k], scanCategory(snapshot[k])});
        return out;
    }));
}

// ── Clean ───────────────────────────────────────────────────────────────
bool SystemCleanerPage::cleanCategory(const Category &c) const
{
    switch (c.strategy) {
    case CleanStrategy::UserFiles: {
        // User-owned: walk the directory and remove each file.
        if (c.path.isEmpty()) return true;
        auto files = FileUtil::dirFiles(c.path, c.recursive);
        for (const auto &f : files) FileUtil::removeFile(f);
        return true;
    }
    case CleanStrategy::RootRotatedLogs: {
        // Only rotated log archives — never delete live log files.
        return CommandUtil::execProgram("pkexec",
            {"find", c.path, "-type", "f", "(",
             "-name", "*.gz", "-o",
             "-name", "*.xz", "-o",
             "-name", "*.bz2", "-o",
             "-name", "*.old", "-o",
             "-name", "*.old.*", "-o",
             "-regex", ".*\\.[0-9]+\\(\\.gz\\)?$",
             ")", "-delete"}, 120000) == 0;
    }
    case CleanStrategy::RootAllInDir: {
        // Remove every regular file inside the directory (keep the dir itself).
        return CommandUtil::execProgram("pkexec",
            {"find", c.path, "-mindepth", "1", "-type", "f", "-delete"},
            120000) == 0;
    }
    case CleanStrategy::AptClean:
        return CommandUtil::execProgram("pkexec",
            {"apt-get", "clean"}, 120000) == 0;
    case CleanStrategy::OldKernelsApt:
        return CommandUtil::execProgram("pkexec",
            {"apt-get", "autoremove", "--purge", "-y"}, 300000) == 0;
    }
    return false;
}

void SystemCleanerPage::clean()
{
    QVector<Category> toClean;
    for (const auto &c : m_categories)
        if (c.card && c.card->isChecked()) toClean.append(c);
    if (toClean.isEmpty()) {
        m_statusLabel->setText(tr("Nothing selected to clean."));
        return;
    }

    // ── Confirmation: explicit list of what's about to be removed ───────
    // We force the user to read what's being deleted before any pkexec prompt
    // appears. Sensitive (root-owned) categories are tagged so the user knows
    // the auth prompt that follows is expected.
    qint64 totalBytes = 0;
    QString summary;
    bool needsRoot = false;
    summary += tr("<b>You are about to remove:</b><br><br>");
    for (const auto &c : toClean) {
        totalBytes += c.size;
        QString rootBadge;
        switch (c.strategy) {
        case CleanStrategy::RootRotatedLogs:
        case CleanStrategy::RootAllInDir:
        case CleanStrategy::AptClean:
        case CleanStrategy::OldKernelsApt:
            needsRoot = true;
            rootBadge = QStringLiteral(" <span style='color:#f38ba8;'>· requires root</span>");
            break;
        default: break;
        }
        QString sizeText = (c.strategy == CleanStrategy::OldKernelsApt)
            ? tr("(autoremove)")
            : FormatUtil::formatBytes(c.size);
        summary += QString("• <b>%1</b> — %2%3<br>").arg(c.name, sizeText, rootBadge);
    }
    summary += "<br>" + tr("Estimated total: <b>%1</b>")
        .arg(FormatUtil::formatBytes(totalBytes));
    if (needsRoot)
        summary += "<br><br>" + tr("ⓘ You will be asked for your password to "
                                    "perform the root-level cleanups.");

    QMessageBox box(this);
    box.setWindowTitle(tr("Confirm clean"));
    box.setTextFormat(Qt::RichText);
    box.setIcon(QMessageBox::Warning);
    box.setText(summary);
    auto *cleanBtn = box.addButton(tr("Clean now"), QMessageBox::AcceptRole);
    cleanBtn->setObjectName("dangerButton");
    box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != cleanBtn) return;

    m_scanButton->setEnabled(false);
    m_cleanButton->setEnabled(false);
    m_statusLabel->setText(tr("Cleaning…"));
    m_overlay->start(tr("Cleaning selected categories…"));

    auto *watcher = new QFutureWatcher<QVector<QPair<QString, bool>>>(this);
    connect(watcher, &QFutureWatcher<QVector<QPair<QString, bool>>>::finished, this,
            [this, watcher]() {
        watcher->deleteLater();
        m_overlay->stop();
        const auto results = watcher->result();
        int ok = 0, failed = 0;
        QStringList failedNames;
        for (const auto &r : results) {
            if (r.second) ++ok;
            else          { ++failed; failedNames << r.first; }
        }
        if (failed == 0)
            m_statusLabel->setText(tr("Cleaning complete — %1 categories processed.").arg(ok));
        else
            m_statusLabel->setText(
                tr("Cleaned %1, failed %2 (%3). Authorization may have been denied.")
                    .arg(ok).arg(failed).arg(failedNames.join(", ")));
        m_scanButton->setEnabled(true);
        // After cleaning, kick off a rescan of the same categories to refresh sizes.
        for (auto &c : m_categories) if (c.card) c.card->setSizeText("");
        m_scanCompleted = false;
        refreshSelectionUi();
    });

    watcher->setFuture(QtConcurrent::run([this, toClean]() {
        QVector<QPair<QString, bool>> out;
        out.reserve(toClean.size());
        for (const auto &c : toClean)
            out.append({c.name, cleanCategory(c)});
        return out;
    }));
}
