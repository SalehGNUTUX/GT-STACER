#include "backup_page.h"
#include "../../Widgets/table_util.h"
#include "../../../gt-stacer-core/Tools/backup_tool.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include "../../Managers/theme.h"
#include "../../Widgets/loading_overlay.h"
#include "../../Widgets/status_pill.h"
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
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
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
    if (m_proc && m_proc->state() != QProcess::NotRunning) { m_proc->kill(); m_proc->waitForFinished(1000); }
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
}

void BackupPage::updateSnapStatus()
{
    const bool ok = SnapshotTool::available();
    const QString fs = SnapshotTool::rootFsType();
    if (m_snapBadge)
        setStatusPill(m_snapBadge,
                      ok ? SnapshotTool::backendName() : tr("Not configured"),
                      ok ? Theme::green() : Theme::subtext());
    if (ok) {
        const QString md = SnapshotTool::mode();
        m_snapStatus->setText(tr("Engine: %1%2 · filesystem: %3 · restores need a reboot")
            .arg(SnapshotTool::backendName(),
                 (SnapshotTool::backend() == SnapshotTool::Timeshift && !md.isEmpty())
                     ? QString(" (%1)").arg(md) : QString(),
                 fs.isEmpty() ? tr("unknown") : fs));
    } else if (SnapshotTool::hasTimeshift()) {
        m_snapStatus->setText(tr("Timeshift is installed but not set up yet — configure it "
                                 "to enable snapshots on your %1 filesystem.")
                                  .arg(fs.isEmpty() ? tr("current") : fs));
    } else if (fs == "btrfs") {
        m_snapStatus->setText(tr("Your root filesystem is Btrfs — install Timeshift or "
                                 "Snapper for instant snapshots."));
    } else if (fs == "zfs") {
        m_snapStatus->setText(tr("Your root filesystem is ZFS — install the zfs tools to "
                                 "manage snapshots."));
    } else {
        m_snapStatus->setText(tr("Your root filesystem is %1 — install Timeshift for "
                                 "rsync-based snapshots.").arg(fs.isEmpty() ? tr("unknown") : fs));
    }

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

// Run a snapshot op (list/create/delete/restore) off the UI thread with the
// overlay up, then reload the list. `fn` returns true on success.
static void runOff(LoadingOverlay *ov, const QString &msg, QWidget *parent,
                   std::function<bool()> fn, std::function<void(bool)> done)
{
    ov->start(msg);
    auto *w = new QFutureWatcher<bool>(parent);
    QObject::connect(w, &QFutureWatcher<bool>::finished, parent, [ov, w, done]{
        const bool ok = w->result();
        ov->stop();
        w->deleteLater();
        if (done) done(ok);
    });
    w->setFuture(QtConcurrent::run(std::move(fn)));
}

void BackupPage::loadSnapshots()
{
    m_overlay->start(tr("Loading snapshots…"));
    auto *w = new QFutureWatcher<QVector<Snapshot>>(this);
    connect(w, &QFutureWatcher<QVector<Snapshot>>::finished, this, [this, w]{
        m_snaps = w->result();
        w->deleteLater();
        m_snapTable->setRowCount(m_snaps.size());
        for (int r = 0; r < m_snaps.size(); ++r) {
            const Snapshot &s = m_snaps.at(r);
            m_snapTable->setItem(r, 0, new QTableWidgetItem(s.date));
            m_snapTable->setItem(r, 1, new QTableWidgetItem(s.tags));
            m_snapTable->setItem(r, 2, new QTableWidgetItem(s.description));
        }
        m_overlay->stop();
    });
    w->setFuture(QtConcurrent::run([]{ return SnapshotTool::list(); }));
}

void BackupPage::createSnapshot()
{
    const QString comment = m_snapComment->text().trimmed();
    runOff(m_overlay, tr("Creating snapshot… this can take a while."), this,
        [comment]{ return SnapshotTool::create(comment); },
        [this](bool ok){
            if (ok) { m_snapComment->clear(); loadSnapshots(); }
            else QMessageBox::warning(this, tr("Snapshot"),
                     tr("Could not create the snapshot (authorization declined?)."));
        });
}

void BackupPage::deleteSnapshot()
{
    const int r = m_snapTable->currentRow();
    if (r < 0 || r >= m_snaps.size()) {
        QMessageBox::information(this, tr("Snapshot"), tr("Select a snapshot first.")); return;
    }
    const Snapshot s = m_snaps.at(r);
    if (QMessageBox::question(this, tr("Delete snapshot"),
            tr("Delete the snapshot from %1?").arg(s.date)) != QMessageBox::Yes) return;
    runOff(m_overlay, tr("Deleting snapshot…"), this,
        [s]{ return SnapshotTool::remove(s); },
        [this](bool ok){ if (ok) loadSnapshots();
            else QMessageBox::warning(this, tr("Snapshot"), tr("Could not delete the snapshot.")); });
}

void BackupPage::restoreSnapshot()
{
    const int r = m_snapTable->currentRow();
    if (r < 0 || r >= m_snaps.size()) {
        QMessageBox::information(this, tr("Snapshot"), tr("Select a snapshot first.")); return;
    }
    const Snapshot s = m_snaps.at(r);
    QMessageBox box(QMessageBox::Warning, tr("Restore snapshot"),
        tr("Restore the system to the snapshot from %1?\n\n"
           "This overwrites current system files and REBOOTS the machine to "
           "finish. Save your work first.").arg(s.date),
        QMessageBox::Cancel, this);
    QPushButton *go = box.addButton(tr("Restore && reboot"), QMessageBox::DestructiveRole);
    box.exec();
    if (box.clickedButton() != go) return;
    runOff(m_overlay, tr("Restoring… the system will reboot."), this,
        [s]{ return SnapshotTool::restore(s); },
        [this](bool ok){ if (!ok) QMessageBox::warning(this, tr("Snapshot"),
            tr("Could not start the restore (authorization declined?).")); });
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

void BackupPage::startBackup()
{
    const QString dest = m_dest->text().trimmed();
    if (dest.isEmpty()) { QMessageBox::information(this, tr("Backup"), tr("Choose a destination folder first.")); return; }
    QDir().mkpath(dest);
    if (!QFileInfo(dest).isWritable()) { QMessageBox::warning(this, tr("Backup"), tr("The destination is not writable.")); return; }
    if (m_mirror->isChecked() &&
        QMessageBox::question(this, tr("Mirror backup"),
            tr("Mirror mode will DELETE files at the destination that are no longer "
               "in your home folder. Continue?")) != QMessageBox::Yes) return;

    QString prog = "rsync";
    QStringList args = BackupTool::rsyncArgs(dest, BackupTool::defaultExcludes(), m_mirror->isChecked());
    CommandUtil::wrapForHost(prog, args);   // no-op outside Flatpak

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, [this]{
        const QString out = QString::fromUtf8(m_proc->readAll());
        // rsync --info=progress2 prints lines containing an overall "NN%".
        static const QRegularExpression pct(QStringLiteral("(\\d{1,3})%"));
        auto it = pct.globalMatch(out);
        int last = -1;
        while (it.hasNext()) last = it.next().captured(1).toInt();
        if (last >= 0) { m_progress->setValue(last); m_backupStatus->setText(tr("Backing up… %1%").arg(last)); }
    });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus){
        setBackupRunning(false);
        const QString msg = code == 0 ? tr("Backup complete.")
                                      : tr("Backup stopped (exit %1).").arg(code);
        m_backupStatus->setText(msg);
        m_progress->setValue(code == 0 ? 100 : m_progress->value());
        m_proc->deleteLater(); m_proc = nullptr;
        NotificationTool::notify(tr("Home backup — finished"), msg,
            code == 0 ? NotificationTool::Urgency::Normal : NotificationTool::Urgency::Critical,
            "gt-stacer");
    });
    setBackupRunning(true);
    m_backupStatus->setText(tr("Starting…"));
    m_proc->start(prog, args);
}

void BackupPage::cancelBackup()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        m_proc->waitForFinished(2000);
        if (m_proc->state() != QProcess::NotRunning) m_proc->kill();
    }
}

// ── boilerplate ──────────────────────────────────────────────────────────────
void BackupPage::changeEvent(QEvent *event) { QWidget::changeEvent(event); }

void BackupPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_overlay) m_overlay->resize(size());
}
