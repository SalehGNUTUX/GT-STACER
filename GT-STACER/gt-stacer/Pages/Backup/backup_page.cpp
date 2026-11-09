#include "backup_page.h"
#include "../../Widgets/table_util.h"
#include "../../Widgets/bidi_text.h"
#include "../../../gt-stacer-core/Tools/backup_tool.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include "../../Managers/theme.h"
#include "../../Widgets/loading_overlay.h"
#include "../../Widgets/status_pill.h"
#include "../../Widgets/failure_text.h"
#include "../../../gt-stacer-core/Tools/notification_tool.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <algorithm>
#include <functional>

BackupPage::BackupPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Backup & Snapshots"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto *intro = new QLabel(tr(
        "Create system restore points (snapshots) and mirror your home directory "
        "to another disk. Snapshots and restores need authorization; a home backup "
        "does not — they are your own files."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    buildSnapshotSection();
    if (m_snapBox) root->addWidget(m_snapBox);
    buildBackupSection();
    root->addWidget(m_backupBox);
    root->addStretch();

    m_overlay = new LoadingOverlay(this);
    m_overlay->resize(size());
    m_overlay->stop();
}

BackupPage::~BackupPage()
{
    // Closing the pipe makes the guard stop rsync (it would also happen when
    // the process exits, even if killed — see CommandUtil::guardForStop).
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->closeWriteChannel();
        m_proc->waitForFinished(3000);
    }
}

// ── snapshots ────────────────────────────────────────────────────────────────
void BackupPage::buildSnapshotSection()
{
    m_snapBox = new QGroupBox(tr("System snapshots"), this);
    m_snapBox->setObjectName("reliefGroup");
    auto *v = new QVBoxLayout(m_snapBox);

    // At-a-glance engine state pill, left-aligned above the details.
    auto *badgeRow = new QHBoxLayout;
    m_snapBadge = new QLabel(m_snapBox);
    badgeRow->addWidget(m_snapBadge);
    badgeRow->addStretch();
    v->addLayout(badgeRow);

    // Engine selector — only when the filesystem supports more than one. The
    // ideal engine for the detected filesystem is placed first and tagged.
    const QVector<SnapshotTool::Backend> engines = SnapshotTool::availableBackends();
    const SnapshotTool::Backend ideal = SnapshotTool::idealBackend();
    if (ideal != SnapshotTool::None) SnapshotTool::setBackend(ideal);
    if (engines.size() > 1) {
        auto *engRow = new QHBoxLayout;
        engRow->addWidget(new QLabel(tr("Engine"), m_snapBox));
        m_engineCombo = new QComboBox(m_snapBox);
        auto addEngine = [&](SnapshotTool::Backend b, bool recommended) {
            QString label = SnapshotTool::nameOf(b);
            const QString md = SnapshotTool::mode();
            if (b == SnapshotTool::Timeshift && !md.isEmpty()) label += QString(" (%1)").arg(md);
            if (recommended) label += "   ★ " + tr("recommended for %1").arg(SnapshotTool::rootFsType());
            m_engineCombo->addItem(label, int(b));
        };
        addEngine(ideal, true);                              // ideal first, tagged
        for (auto b : engines) if (b != ideal) addEngine(b, false);
        connect(m_engineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &BackupPage::onBackendChanged);
        engRow->addWidget(m_engineCombo, 1);
        v->addLayout(engRow);
    }

    m_snapStatus = new QLabel(m_snapBox);
    m_snapStatus->setObjectName("infoValue");
    m_snapStatus->setWordWrap(true);
    v->addWidget(m_snapStatus);
    m_snapIssue = new QLabel(m_snapBox);
    m_snapIssue->setObjectName("pkgWarning");
    m_snapIssue->setWordWrap(true);
    m_snapIssue->setVisible(false);
    v->addWidget(m_snapIssue);

    m_snapTable = new QTableWidget(0, 3, m_snapBox);
    m_snapTable->setHorizontalHeaderLabels({tr("Date"), tr("Tags"), tr("Description")});
    setupResizableTable(m_snapTable, 2);   // primary column fills; all resizable
    m_snapTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_snapTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_snapTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_snapTable->verticalHeader()->setVisible(false);
    m_snapTable->setAlternatingRowColors(true);
    v->addWidget(m_snapTable);

    auto *row = new QHBoxLayout;
    m_snapComment = new QLineEdit(m_snapBox);
    m_snapComment->setPlaceholderText(tr("Comment for a new snapshot (optional)"));
    m_snapCreate  = new QPushButton(tr("Create snapshot"), m_snapBox);
    m_snapCreate->setObjectName("primaryButton");
    m_snapLoad    = new QPushButton(tr("Load"), m_snapBox);
    m_snapRestore = new QPushButton(tr("Restore selected"), m_snapBox);
    m_snapDelete  = new QPushButton(tr("Delete selected"), m_snapBox);
    m_snapDelete->setObjectName("dangerButton");
    row->addWidget(m_snapComment, 1);
    row->addWidget(m_snapCreate);
    row->addWidget(m_snapLoad);
    row->addSpacing(12);
    row->addWidget(m_snapRestore);
    row->addWidget(m_snapDelete);
    v->addLayout(row);

    connect(m_snapLoad,    &QPushButton::clicked, this, &BackupPage::loadSnapshots);
    connect(m_snapCreate,  &QPushButton::clicked, this, &BackupPage::createSnapshot);
    connect(m_snapDelete,  &QPushButton::clicked, this, &BackupPage::deleteSnapshot);
    connect(m_snapRestore, &QPushButton::clicked, this, &BackupPage::restoreSnapshot);

    updateSnapStatus();
    watchSnapshotConfig();
}

// Settings changed in Timeshift (or Snapper) while GT-STACER is open used to
// stay invisible until a restart (26.12). Watch their folders — Timeshift
// saves by REPLACING timeshift.json, which a file-only watch would lose — and
// refresh the status shortly after. Both are world-readable: no password.
void BackupPage::watchSnapshotConfig()
{
    m_snapWatch = new QFileSystemWatcher(this);
    m_snapDebounce = new QTimer(this);
    m_snapDebounce->setSingleShot(true);
    m_snapDebounce->setInterval(600);            // one refresh per save, not per write
    connect(m_snapDebounce, &QTimer::timeout, this, &BackupPage::onSnapshotConfigChanged);
    auto arm = [this]{
        for (const QString &p : {QStringLiteral("/etc/timeshift"), QStringLiteral("/etc/timeshift/timeshift.json"),
                                 QStringLiteral("/etc/snapper"), QStringLiteral("/etc/snapper/configs")})
            if (QFileInfo::exists(p) && !m_snapWatch->files().contains(p) && !m_snapWatch->directories().contains(p))
                m_snapWatch->addPath(p);
    };
    arm();
    connect(m_snapWatch, &QFileSystemWatcher::fileChanged, this, [this, arm]{ arm(); m_snapDebounce->start(); });
    connect(m_snapWatch, &QFileSystemWatcher::directoryChanged, this, [this, arm]{ arm(); m_snapDebounce->start(); });
}

void BackupPage::onSnapshotConfigChanged()
{
    updateSnapStatus();
    // A list loaded before the change may belong to the old snapshot location.
    if (!m_snaps.isEmpty()) {
        m_snaps.clear();
        m_snapTable->setRowCount(0);
        m_snapStatus->setText(m_snapStatus->text() + "\n"
            + tr("The snapshot settings changed — click “Load” to list the snapshots of the new location."));
    }
}

void BackupPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateSnapStatus();                          // never show a stale status
}

void BackupPage::updateSnapStatus()
{
    const bool ok = SnapshotTool::available();
    const QString fs = SnapshotTool::rootFsType();
    if (m_snapBadge)
        setStatusPill(m_snapBadge,
                      ok ? SnapshotTool::backendName() : tr("Not configured"),
                      ok ? Theme::green() : Theme::subtext());
    QString issue;
    if (ok) {
        const QString md = SnapshotTool::mode();
        QString status = (SnapshotTool::backend() == SnapshotTool::Zfs
                ? tr("Engine: %1%2 · filesystem: %3 · restores apply immediately")
                : tr("Engine: %1%2 · filesystem: %3 · restores need a reboot"))
            .arg(SnapshotTool::backendName(),
                 (SnapshotTool::backend() == SnapshotTool::Timeshift && !md.isEmpty())
                     ? QString(" (%1)").arg(md) : QString(),
                 fs.isEmpty() ? tr("unknown") : fs);
        if (SnapshotTool::backend() == SnapshotTool::Timeshift) {
            // Say WHERE the restore points go — 26.11 never did, so rsync copies
            // landing on another partition came as a surprise.
            const SnapshotTool::Location loc = SnapshotTool::timeshiftLocation();
            if (!loc.device.isEmpty()) {
                const QString where = loc.label.isEmpty() ? loc.device
                                                          : QString("%1 (%2)").arg(loc.label, loc.device);
                status += "\n" + (loc.sameAsRoot
                    ? tr("Snapshots are stored on the system partition %1 · %2").arg(where, loc.fsType)
                    : tr("Snapshots are stored on %1 · %2 — not on the system partition").arg(where, loc.fsType));
            }
            const QString sv = SnapshotTool::rootSubvolume();
            switch (SnapshotTool::timeshiftIssue()) {
            case SnapshotTool::TsIssue::BtrfsModeWithoutLayout:
                issue = tr("Timeshift is set to BTRFS mode, but this system can't use it: BTRFS snapshots "
                           "need the system installed in a subvolume named “@” (the Ubuntu layout), and "
                           "yours is installed at the top level of the BTRFS volume (subvol=%1). So Timeshift "
                           "can only make rsync copies, on %2.\n"
                           "For instant BTRFS snapshots on this layout use Snapper (sudo apt install snapper, "
                           "then: sudo snapper -c root create-config /). Or open Timeshift → Settings and "
                           "choose RSYNC mode with the system partition as the location.")
                            .arg(sv.isEmpty() ? QStringLiteral("/") : sv,
                                 loc.label.isEmpty() ? loc.device : loc.label);
                break;
            case SnapshotTool::TsIssue::BtrfsModeOnNonBtrfsDevice:
                issue = tr("Timeshift is in BTRFS mode but its snapshot location %1 is %2 — BTRFS snapshots "
                           "can only be kept on the BTRFS system partition. Open Timeshift → Settings → "
                           "Location to fix it.").arg(loc.label.isEmpty() ? loc.device : loc.label, loc.fsType);
                break;
            case SnapshotTool::TsIssue::NoDevice:
                issue = tr("The disk Timeshift is set to store snapshots on is not connected. "
                           "Connect it, or choose another location in Timeshift → Settings.");
                break;
            case SnapshotTool::TsIssue::None:
                break;
            }
        }
        m_snapStatus->setText(Bidi::isolate(status));
    } else if (fs == "btrfs" && !SnapshotTool::timeshiftBtrfsLayoutOk()) {
        m_snapStatus->setText(Bidi::isolate(tr("Your root filesystem is Btrfs, installed without the “@” subvolume "
                                 "layout — Timeshift can only make rsync copies here. For instant Btrfs "
                                 "snapshots use Snapper (sudo apt install snapper, then: "
                                 "sudo snapper -c root create-config /).")));
    } else if (SnapshotTool::hasTimeshift()) {
        m_snapStatus->setText(Bidi::isolate(tr("Timeshift is installed but not set up yet — configure it "
                                 "to enable snapshots on your %1 filesystem.")
                                  .arg(fs.isEmpty() ? tr("current") : fs)));
    } else if (fs == "btrfs") {
        m_snapStatus->setText(Bidi::isolate(tr("Your root filesystem is Btrfs — install Timeshift or "
                                 "Snapper for instant snapshots.")));
    } else if (fs == "zfs") {
        m_snapStatus->setText(Bidi::isolate(tr("Your root filesystem is ZFS — install the zfs tools to "
                                 "manage snapshots.")));
    } else {
        m_snapStatus->setText(Bidi::isolate(tr("Your root filesystem is %1 — install Timeshift for "
                                 "rsync-based snapshots.").arg(fs.isEmpty() ? tr("unknown") : fs)));
    }

    m_snapIssue->setText(Bidi::isolate(issue));
    m_snapIssue->setVisible(!issue.isEmpty());

    // Actions are usable only with an active engine; restore only where wired.
    for (QPushButton *b : {m_snapLoad, m_snapCreate, m_snapDelete})
        b->setEnabled(ok);
    m_snapRestore->setEnabled(ok && SnapshotTool::canRestore());
    m_snapComment->setEnabled(ok);
    if (ok && !SnapshotTool::canRestore())
        m_snapRestore->setToolTip(tr("Restore isn't available for %1 yet.").arg(SnapshotTool::backendName()));
}

void BackupPage::onBackendChanged()
{
    if (!m_engineCombo) return;
    SnapshotTool::setBackend(SnapshotTool::Backend(m_engineCombo->currentData().toInt()));
    m_snapTable->setRowCount(0);
    m_snaps.clear();
    updateSnapStatus();
}

// Run a snapshot op (create/delete/restore) off the UI thread with the overlay
// up. `fn` returns the tool's bool; the CommandResult behind it (exit code +
// output) is captured on the worker thread so a failure can be EXPLAINED —
// 26.11 could only guess "authorization declined?".
static void runOff(LoadingOverlay *ov, const QString &title, const QString &detail, QWidget *parent,
                   std::function<bool()> fn, std::function<void(const CommandResult &)> done)
{
    ov->start(title, detail);
    auto *w = new QFutureWatcher<CommandResult>(parent);
    QObject::connect(w, &QFutureWatcher<CommandResult>::finished, parent, [ov, w, done]{
        const CommandResult r = w->result();
        ov->stop();
        w->deleteLater();
        if (done) done(r);
    });
    w->setFuture(QtConcurrent::run([fn]{
        const bool ok = fn();
        CommandResult r = CommandUtil::lastResult();
        if (ok)              { r.started = true; r.timedOut = false; r.exitCode = 0; }
        else if (r.ok())     { r.exitCode = 1; }    // the tool refused before running a command
        return r;
    }));
}

// The Timeshift settings problem (if any), appended to a failure message.
QString BackupPage::snapshotIssueHint() const
{
    return (m_snapIssue && m_snapIssue->isVisible()) ? "\n\n" + m_snapIssue->text() : QString();
}

void BackupPage::loadSnapshots()
{
    m_overlay->start(tr("Loading snapshots"), tr("Asking the snapshot tool for its list…"));
    using Res = QPair<QVector<Snapshot>, CommandResult>;
    auto *w = new QFutureWatcher<Res>(this);
    connect(w, &QFutureWatcher<Res>::finished, this, [this, w]{
        const Res res = w->result();
        m_snaps = res.first;
        w->deleteLater();
        m_snapTable->setRowCount(m_snaps.size());
        for (int r = 0; r < m_snaps.size(); ++r) {
            const Snapshot &s = m_snaps.at(r);
            m_snapTable->setItem(r, 0, new QTableWidgetItem(s.date));
            m_snapTable->setItem(r, 1, new QTableWidgetItem(s.tags));
            m_snapTable->setItem(r, 2, new QTableWidgetItem(s.description));
        }
        m_overlay->stop();
        // An empty list is either "no snapshots yet" or a failure — say which.
        const bool noneYet = res.second.output.contains("no snapshots", Qt::CaseInsensitive);
        if (!res.second.ok() && !noneYet)
            FailureText::show(this, tr("Snapshot"), tr("Could not list the snapshots.")
                              + snapshotIssueHint(), res.second);
        else if (m_snaps.isEmpty())
            m_snapStatus->setText(Bidi::isolate(m_snapStatus->text().section('\n', 0, 1) + "\n"
                                  + tr("No snapshots yet — create the first one below.")));
    });
    w->setFuture(QtConcurrent::run([]{
        const QVector<Snapshot> list = SnapshotTool::list();
        return Res(list, CommandUtil::lastResult());
    }));
}

void BackupPage::createSnapshot()
{
    const QString comment = m_snapComment->text().trimmed();
    runOff(m_overlay, tr("Creating a snapshot"), tr("This can take a while — keep the computer on."), this,
        [comment]{ return SnapshotTool::create(comment); },
        [this](const CommandResult &r){
            if (r.ok()) { m_snapComment->clear(); loadSnapshots(); return; }
            FailureText::show(this, tr("Snapshot"),
                              tr("The snapshot was not created.") + snapshotIssueHint(), r);
        });
}

void BackupPage::deleteSnapshot()
{
    const int r = m_snapTable->currentRow();
    if (r < 0 || r >= m_snaps.size()) {
        QMessageBox::information(this, tr("Snapshot"), Bidi::isolate(tr("Select a snapshot first."))); return;
    }
    const Snapshot s = m_snaps.at(r);
    if (QMessageBox::question(this, tr("Delete snapshot"),
            Bidi::isolate(tr("Delete the snapshot from %1?").arg(s.date))) != QMessageBox::Yes) return;
    runOff(m_overlay, tr("Deleting the snapshot"), s.date, this,
        [s]{ return SnapshotTool::remove(s); },
        [this](const CommandResult &r){
            if (r.ok()) { loadSnapshots(); return; }
            FailureText::show(this, tr("Snapshot"), tr("The snapshot was not deleted."), r);
        });
}

void BackupPage::restoreSnapshot()
{
    const int r = m_snapTable->currentRow();
    if (r < 0 || r >= m_snaps.size()) {
        QMessageBox::information(this, tr("Snapshot"), Bidi::isolate(tr("Select a snapshot first."))); return;
    }
    const Snapshot s = m_snaps.at(r);
    if (SnapshotTool::backend() == SnapshotTool::Zfs) { restoreZfs(r); return; }
    QMessageBox box(QMessageBox::Warning, tr("Restore snapshot"),
        tr("Restore the system to the snapshot from %1?\n\n"
           "This overwrites current system files and REBOOTS the machine to "
           "finish. Save your work first.").arg(s.date),
        QMessageBox::Cancel, this);
    QPushButton *go = box.addButton(tr("Restore && reboot"), QMessageBox::DestructiveRole);
    box.exec();
    if (box.clickedButton() != go) return;
    runOff(m_overlay, tr("Restoring the snapshot"), tr("The system will reboot when it is done."), this,
        [s]{ return SnapshotTool::restore(s); },
        [this](const CommandResult &r){
            if (r.ok()) return;
            FailureText::show(this, tr("Restore snapshot"),
                              tr("The restore did not start — your system was not changed."), r);
        });
}

// ZFS rolls back LIVE, while the system runs, and must destroy every newer
// snapshot of the dataset to do so. Say exactly that, require an explicit
// tick, then offer the restart that should follow (26.12).
void BackupPage::restoreZfs(int row)
{
    const Snapshot s = m_snaps.at(row);
    const QString dataset = s.id.section('@', 0, 0);
    QStringList newer;                    // list is sorted by creation (oldest first)
    for (int i = row + 1; i < m_snaps.size(); ++i)
        if (m_snaps.at(i).id.section('@', 0, 0) == dataset) newer << m_snaps.at(i).id.section('@', 1);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Restore ZFS snapshot"));
    auto *v = new QVBoxLayout(&dlg);
    auto *text = new QLabel(&dlg);
    text->setWordWrap(true);
    text->setTextFormat(Qt::RichText);
    QString html = tr("<p><b>Restore %1 to the restore point from %2?</b></p>"
                      "<p>ZFS restores <b>immediately, while the system is running</b> — it does "
                      "not wait for a reboot. System files go back to that moment right now, so "
                      "open programs may misbehave until you restart. <b>Restart right after the "
                      "restore.</b></p>"
                      "<p>Save your work and close other programs first.</p>")
                       .arg(dataset.toHtmlEscaped(), s.date.toHtmlEscaped());
    if (!newer.isEmpty())
        html += tr("<p><b>%1 newer restore point(s) will be permanently deleted</b> — ZFS can "
                   "only roll back past them by removing them: %2</p>")
                    .arg(newer.size()).arg(newer.mid(0, 8).join(", ").toHtmlEscaped());
    text->setText(html);
    text->setMinimumWidth(480);
    v->addWidget(text);
    auto *ack = new QCheckBox(newer.isEmpty()
        ? tr("I understand the restore happens immediately")
        : tr("I understand the restore happens immediately and newer restore points are deleted"), &dlg);
    v->addWidget(ack);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton *go = buttons->addButton(tr("Restore now"), QDialogButtonBox::DestructiveRole);
    go->setObjectName("dangerButton");
    go->setEnabled(false);
    connect(ack, &QCheckBox::toggled, go, &QPushButton::setEnabled);
    connect(go, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    v->addWidget(buttons);
    if (dlg.exec() != QDialog::Accepted) return;

    runOff(m_overlay, tr("Restoring the ZFS snapshot"), s.date, this,
        [s]{ return SnapshotTool::restore(s); },
        [this](const CommandResult &r){
            if (!r.ok()) {
                FailureText::show(this, tr("Restore snapshot"),
                                  tr("The restore did not complete."), r);
                return;
            }
            loadSnapshots();
            if (QMessageBox::question(this, tr("Restore snapshot"),
                    Bidi::isolate(tr("The system was restored. Restart now so every program starts from the "
                       "restored files?"))) == QMessageBox::Yes)
                CommandUtil::execProgram("systemctl", {"reboot"}, 15000);
        });
}

// ── home backup ──────────────────────────────────────────────────────────────
void BackupPage::buildBackupSection()
{
    m_backupBox = new QGroupBox(tr("Home backup (rsync mirror)"), this);
    m_backupBox->setObjectName("reliefGroup");
    auto *v = new QVBoxLayout(m_backupBox);

    v->addWidget(new QLabel(tr("Mirror your home folder to another disk. Caches and "
                               "the trash are skipped."), m_backupBox));

    auto *destRow = new QHBoxLayout;
    destRow->addWidget(new QLabel(tr("Destination"), m_backupBox));
    m_dest = new QLineEdit(m_backupBox);
    m_dest->setPlaceholderText(tr("/run/media/you/BackupDrive/home-backup"));
    auto *browse = new QPushButton(tr("Browse…"), m_backupBox);
    destRow->addWidget(m_dest, 1);
    destRow->addWidget(browse);
    v->addLayout(destRow);

    auto *optRow = new QHBoxLayout;
    m_mirror = new QCheckBox(tr("Mirror (delete files at the destination that no longer exist)"), m_backupBox);
    m_mirror->setChecked(true);
    m_backupBtn = new QPushButton(tr("Back up now"), m_backupBox);
    m_backupBtn->setObjectName("primaryButton");
    m_cancelBtn = new QPushButton(tr("Cancel"), m_backupBox);
    m_cancelBtn->setEnabled(false);
    optRow->addWidget(m_mirror, 1);
    optRow->addWidget(m_backupBtn);
    optRow->addWidget(m_cancelBtn);
    v->addLayout(optRow);

    m_progress = new QProgressBar(m_backupBox);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    v->addWidget(m_progress);

    m_backupStatus = new QLabel(m_backupBox);
    m_backupStatus->setObjectName("infoValue");
    m_backupStatus->setWordWrap(true);
    v->addWidget(m_backupStatus);

    connect(browse,      &QPushButton::clicked, this, &BackupPage::browseDest);
    connect(m_backupBtn, &QPushButton::clicked, this, &BackupPage::startBackup);
    connect(m_cancelBtn, &QPushButton::clicked, this, &BackupPage::cancelBackup);
}

void BackupPage::browseDest()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("Choose backup destination"),
                                                        m_dest->text().isEmpty() ? QDir::homePath() : m_dest->text());
    if (!d.isEmpty()) m_dest->setText(d);
}

void BackupPage::setBackupRunning(bool running)
{
    m_backupBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_dest->setEnabled(!running);
    m_mirror->setEnabled(!running);
    m_progress->setVisible(running);
}

namespace {
QString fsKindNote(const BackupTool::Destination &d)
{
    switch (d.kind) {
    case BackupTool::FsKind::Ntfs:
        return BackupPage::tr("The destination is NTFS (%1): Linux permissions and owners can't be "
                              "stored there, so they are not copied.").arg(d.fsType);
    case BackupTool::FsKind::Fat32:
        return BackupPage::tr("The destination is FAT32 (%1): it can't store Linux permissions, "
                              "symbolic links, or files of 4 GB or more — those are skipped. "
                              "Names with characters like : ? * \" < > | can't be saved either. "
                              "For a complete backup, use an ext4, Btrfs or exFAT/NTFS drive.").arg(d.fsType);
    case BackupTool::FsKind::ExFat:
        return BackupPage::tr("The destination is exFAT: Linux permissions and symbolic links can't "
                              "be stored there and are skipped. Names with characters like : ? * \" < > | "
                              "can't be saved either.");
    case BackupTool::FsKind::Smb:
        return BackupPage::tr("The destination is a network share (%1): permissions and symbolic links "
                              "are not copied.").arg(d.fsType);
    case BackupTool::FsKind::Posix:
        break;
    }
    return {};
}
} // namespace

void BackupPage::startBackup()
{
    const QString destText = m_dest->text().trimmed();
    if (destText.isEmpty()) { QMessageBox::information(this, tr("Backup"), Bidi::isolate(tr("Choose a destination folder first."))); return; }
    const QString destAbs = QDir::cleanPath(QFileInfo(destText).absoluteFilePath());
    const QString home = QDir::cleanPath(BackupTool::homePath());

    // Hard refusals — checked BEFORE anything is created. Backing up into home
    // itself or one of its parents (e.g. /home) would copy home into itself and,
    // in mirror mode, DELETE the home folder (it isn't "in" the source).
    if (destAbs == home || home.startsWith(destAbs == "/" ? QStringLiteral("/") : destAbs + '/')) {
        QMessageBox::critical(this, tr("Backup"),
            Bidi::isolate(tr("The destination can't be your home folder or a folder that contains it (%1). "
               "Choose a folder on another disk, e.g. on a USB drive.").arg(destAbs)));
        return;
    }
    QDir().mkpath(destAbs);
    if (!QFileInfo(destAbs).isDir() || !QFileInfo(destAbs).isWritable()) {
        QMessageBox::warning(this, tr("Backup"), Bidi::isolate(tr("The destination is not writable.")));
        return;
    }

    const BackupTool::Destination d = BackupTool::inspect(destAbs);
    const bool mirror = m_mirror->isChecked();

    // Everything the user should know before starting, in one confirmation.
    QStringList notes;
    const QString fsNote = fsKindNote(d);
    if (!fsNote.isEmpty()) notes << fsNote;
    if (d.insideHome)
        notes << tr("The destination is inside your home folder — it is excluded from the copy, "
                    "but a backup on the same disk won't survive a disk failure.");
    else if (d.sameDisk)
        notes << tr("The destination is on the same disk as your home folder — this won't "
                    "protect you from a disk failure.");
    const bool foreignFiles = mirror && d.nonEmpty && !d.hasMarker;
    const int  boldNote = foreignFiles ? notes.size() : -1;   // shown in bold
    if (foreignFiles)
        notes << tr("This folder already contains files that were not put there by a GT-STACER "
                    "backup. Mirror mode will DELETE everything in it that is not in your home "
                    "folder. To keep them, cancel and choose an empty folder (or a new "
                    "sub-folder) or untick Mirror.");
    else if (mirror)
        notes << tr("Mirror mode deletes files at the destination that no longer exist in your home folder.");

    if (!notes.isEmpty()) {
        QMessageBox box(QMessageBox::Question, tr("Back up home folder"),
            tr("Back up %1 → %2 (filesystem: %3)?").arg(home, destAbs,
                d.fsType.isEmpty() ? tr("unknown") : d.fsType).toHtmlEscaped(),
            QMessageBox::Cancel, this);
        QStringList html;
        for (int i = 0; i < notes.size(); ++i) {
            const QString e = notes[i].toHtmlEscaped();   // notes contain < > characters
            html << (i == boldNote ? "<b>" + e + "</b>" : e);
        }
        box.setTextFormat(Qt::RichText);
        box.setInformativeText("• " + html.join("<br>• "));
        QPushButton *go = box.addButton(tr("Back up now"), QMessageBox::AcceptRole);
        box.setDefaultButton(foreignFiles ? qobject_cast<QPushButton *>(box.button(QMessageBox::Cancel)) : go);
        box.exec();
        if (box.clickedButton() != go) return;
    }

    // rsync runs under the stop guard: Stop (or GT-STACER exiting in any way,
    // even force-killed) closes our pipe and the guard stops rsync — 26.11 left
    // rsync copying in the background after a force-close.
    QString prog = "rsync";
    QStringList args = BackupTool::rsyncArgs(d, BackupTool::defaultExcludes(), mirror);
    CommandUtil::guardForStop(prog, args, /*asRoot=*/false);

    m_stopRequested = false;
    m_rsyncErrors.clear();
    m_outTail.clear();
    m_runDest = destAbs;
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, [this]{
        const QString out = QString::fromUtf8(m_proc->readAll());
        // Keep rsync's own error messages for the final report. They are often
        // glued to the end of a progress line with no line break before them,
        // and a message can straddle two reads — so scan a carried-over buffer
        // for "rsync: …" / "rsync error: …" anywhere, up to the line end.
        m_outTail += out;
        static const QRegularExpression err(QStringLiteral("rsync(?:: | error: )[^\\r\\n]*[\\r\\n]"));
        auto em = err.globalMatch(m_outTail);
        int consumed = 0;
        while (em.hasNext()) {
            const auto m = em.next();
            if (m_rsyncErrors.size() < 500) m_rsyncErrors << m.captured(0).trimmed();
            consumed = m.capturedEnd();
        }
        // Drop what's been scanned; keep only a possibly-incomplete tail.
        const int lastBreak = std::max(m_outTail.lastIndexOf('\n'), m_outTail.lastIndexOf('\r'));
        m_outTail = m_outTail.mid(std::max(consumed, lastBreak + 1));
        if (m_outTail.size() > 8192) m_outTail = m_outTail.right(8192);
        // rsync --info=progress2 prints lines containing an overall "NN%".
        static const QRegularExpression pct(QStringLiteral("(\\d{1,3})%"));
        auto it = pct.globalMatch(out);
        int last = -1;
        while (it.hasNext()) last = it.next().captured(1).toInt();
        if (last >= 0 && !m_stopRequested) {
            m_progress->setValue(last);
            m_backupStatus->setText(tr("Backing up… %1%").arg(last));
        }
    });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e){
        if (e != QProcess::FailedToStart) return;     // finished() won't follow
        setBackupRunning(false);
        m_backupStatus->setText(tr("Could not start the backup."));
        m_proc->deleteLater(); m_proc = nullptr;
    });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus){
        setBackupRunning(false);
        // rsync exit codes: 0 ok · 24 some files vanished while copying (normal
        // for a live home) · 23 some files couldn't be copied · 20 stopped ·
        // 11/12 I/O or out of space · 127 rsync missing.
        const bool spaceFull = std::any_of(m_rsyncErrors.cbegin(), m_rsyncErrors.cend(),
            [](const QString &l){ return l.contains("No space left on device"); });
        int errors = 0;
        for (const QString &l : m_rsyncErrors) if (!l.startsWith("rsync error")) ++errors;
        if (code == 23 && errors == 0) errors = 1;   // at least one, by definition
        QString msg;
        bool good = false;
        if (m_stopRequested)        msg = tr("Backup stopped. The files copied so far are kept.");
        else if (code == 0)         { msg = tr("Backup complete."); good = true; }
        else if (code == 24)        { msg = tr("Backup complete (some files changed or disappeared while being copied — that's normal for files in use)."); good = true; }
        else if (code == 23)        { msg = tr("Backup finished, but %1 item(s) could not be copied.").arg(errors); good = true; }
        else if (spaceFull)         msg = tr("Backup failed: the destination is full.");
        else if (code == 127)       msg = tr("Backup failed: rsync is not installed.");
        else {
            // Explain it from rsync's own messages (read-only disk, permission,
            // disconnected drive …) rather than just a number.
            CommandResult r;
            r.program = "rsync"; r.started = true; r.exitCode = code;
            r.output = m_rsyncErrors.join('\n');
            msg = tr("Backup failed.") + " " + FailureText::brief(r);
            if (code == 11 || code == 12 || code == 10)
                msg += " " + tr("Check that the destination drive is still connected.");
        }

        if (good) BackupTool::writeMarker(m_runDest);
        m_backupStatus->setText(msg);
        m_progress->setValue(code == 0 || code == 24 ? 100 : m_progress->value());
        m_proc->deleteLater(); m_proc = nullptr;
        NotificationTool::notify(tr("Home backup — finished"), msg,
            good || m_stopRequested ? NotificationTool::Urgency::Normal : NotificationTool::Urgency::Critical,
            "gt-stacer");
        if (!m_stopRequested && code != 0 && code != 24 && !m_rsyncErrors.isEmpty()) {
            QMessageBox box(good ? QMessageBox::Information : QMessageBox::Warning, tr("Home backup"), msg,
                            QMessageBox::Ok, this);
            box.setInformativeText(tr("Click “Show Details” for the list."));
            box.setDetailedText(m_rsyncErrors.join('\n'));
            box.exec();
        }
    });
    setBackupRunning(true);
    m_backupStatus->setText(tr("Starting…"));
    m_proc->start(prog, args);
}

void BackupPage::cancelBackup()
{
    if (!(m_proc && m_proc->state() != QProcess::NotRunning)) return;
    // Closing the pipe tells the guard to stop rsync; no UI freeze (26.11 blocked
    // the window for 2 s here and then SIGKILLed only the parent rsync).
    m_stopRequested = true;
    m_cancelBtn->setEnabled(false);
    m_backupStatus->setText(tr("Stopping the backup…"));
    m_proc->closeWriteChannel();
    // Safety net: rsync normally stops within a second. If not, signal it
    // directly (it runs as us, so no password is needed).
    QTimer::singleShot(8000, this, [this]{
        if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
        if (!CommandUtil::inFlatpak())
            CommandUtil::execProgram("pkill", {"-TERM", "-P", QString::number(m_proc->processId()), "-x", "rsync"}, 5000);
        else
            m_proc->terminate();
    });
}

// ── boilerplate ──────────────────────────────────────────────────────────────
void BackupPage::changeEvent(QEvent *event) { QWidget::changeEvent(event); }

void BackupPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_overlay) m_overlay->resize(size());
}
