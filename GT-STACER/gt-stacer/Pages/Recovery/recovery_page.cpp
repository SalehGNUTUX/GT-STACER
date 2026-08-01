#include "recovery_page.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include "../../Widgets/empty_state.h"
#include "../../Widgets/sidebar_icons.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QListWidget>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

RecoveryPage::RecoveryPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("File Recovery"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto *intro = new QLabel(tr(
        "Recover lost or deleted files by carving a disk, partition or image with "
        "PhotoRec. Choose a destination on a DIFFERENT disk — never the one you are "
        "recovering from. Reading a raw device needs authorization."));
    intro->setWordWrap(true);
    intro->setObjectName("introText");
    root->addWidget(intro);

    if (!RecoveryTool::available()) {
        auto *empty = new EmptyState;
        empty->setIconSvg(SidebarIcons::recovery(), 64);
        empty->setTitle(tr("File recovery needs PhotoRec"));
        empty->setSubtitle(tr("Install the “testdisk” package to carve lost or deleted "
                              "files from a disk, partition or image."));
        root->addWidget(empty, 1);
        return;
    }

    // Source row.
    auto *srcRow = new QHBoxLayout;
    srcRow->addWidget(new QLabel(tr("Recover from")));
    m_source  = new QComboBox;
    m_refresh = new QPushButton(tr("Refresh"));
    srcRow->addWidget(m_source, 1);
    srcRow->addWidget(m_refresh);
    root->addLayout(srcRow);

    // Destination row.
    auto *dstRow = new QHBoxLayout;
    dstRow->addWidget(new QLabel(tr("Save recovered files to")));
    m_dest   = new QLineEdit;
    m_dest->setPlaceholderText(tr("/run/media/you/OtherDrive/recovered"));
    m_browse = new QPushButton(tr("Browse…"));
    dstRow->addWidget(m_dest, 1);
    dstRow->addWidget(m_browse);
    root->addLayout(dstRow);

    // File-type selection — PhotoRec can target specific families; we default to
    // everything, and can sort the results into a folder per type afterwards.
    auto *optRow = new QHBoxLayout;
    m_allTypes = new QCheckBox(tr("Recover all file types"));
    m_allTypes->setChecked(true);
    m_organize = new QCheckBox(tr("Sort recovered files into a folder per type"));
    m_organize->setChecked(true);
    optRow->addWidget(m_allTypes);
    optRow->addSpacing(16);
    optRow->addWidget(m_organize);
    optRow->addStretch();
    root->addLayout(optRow);

    m_typeList = new QListWidget;
    m_typeList->setMaximumHeight(150);
    m_typeList->setEnabled(false);   // active only when "all types" is unchecked
    for (const FileFamily &f : RecoveryTool::fileFamilies()) {
        auto *it = new QListWidgetItem(QString("%1   ·   %2").arg(f.label, f.group), m_typeList);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
        it->setData(Qt::UserRole, f.id);
    }
    root->addWidget(m_typeList);
    connect(m_allTypes, &QCheckBox::toggled, this, [this](bool all){ m_typeList->setEnabled(!all); });

    // Actions.
    auto *actRow = new QHBoxLayout;
    m_start  = new QPushButton(tr("Start recovery"));
    m_start->setObjectName("primaryButton");
    m_cancel = new QPushButton(tr("Cancel"));
    m_cancel->setEnabled(false);
    m_open   = new QPushButton(tr("Open folder"));
    m_open->setEnabled(false);
    actRow->addWidget(m_start);
    actRow->addWidget(m_cancel);
    actRow->addWidget(m_open);
    actRow->addStretch();
    root->addLayout(actRow);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);          // indeterminate — PhotoRec has no clean %
    m_progress->setVisible(false);
    root->addWidget(m_progress);

    m_status = new QLabel;
    m_status->setObjectName("infoValue");
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    root->addStretch();

    m_timer = new QTimer(this);
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, &RecoveryPage::tickCount);

    connect(m_refresh, &QPushButton::clicked, this, &RecoveryPage::refreshSources);
    connect(m_browse,  &QPushButton::clicked, this, &RecoveryPage::browseDest);
    connect(m_start,   &QPushButton::clicked, this, &RecoveryPage::startRecovery);
    connect(m_cancel,  &QPushButton::clicked, this, &RecoveryPage::cancelRecovery);
    connect(m_open,    &QPushButton::clicked, this, &RecoveryPage::openFolder);

    refreshSources();
}

RecoveryPage::~RecoveryPage()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) { m_proc->kill(); m_proc->waitForFinished(1000); }
}

void RecoveryPage::refreshSources()
{
    if (!m_source) return;
    m_sources = RecoveryTool::sources();
    m_source->clear();
    for (const RecoverySource &s : m_sources) {
        QString label = s.label();
        if (s.mounted()) label += "  ⚠ " + tr("mounted");
        m_source->addItem(label, s.path);
    }
}

void RecoveryPage::browseDest()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("Choose where to save recovered files"),
        m_dest->text().isEmpty() ? QDir::homePath() : m_dest->text());
    if (!d.isEmpty()) m_dest->setText(d);
}

static QString deviceOf(const QString &path)
{
    return CommandUtil::execProgramOutput("findmnt", {"-no", "SOURCE", "-T", path}, 5000).trimmed();
}
static QString baseDisk(const QString &dev)   // /dev/sda2 -> /dev/sda
{
    QString d = dev;
    while (!d.isEmpty() && d.back().isDigit()) d.chop(1);
    if (d.endsWith('p')) d.chop(1);   // nvme0n1p2 -> nvme0n1
    return d;
}

void RecoveryPage::startRecovery()
{
    const int idx = m_source->currentIndex();
    if (idx < 0 || idx >= m_sources.size()) { QMessageBox::information(this, tr("Recovery"), tr("Choose a source first.")); return; }
    const RecoverySource src = m_sources.at(idx);
    const QString dest = m_dest->text().trimmed();
    if (dest.isEmpty()) { QMessageBox::information(this, tr("Recovery"), tr("Choose a destination folder first.")); return; }
    QDir().mkpath(dest);

    // Safety: never write the recovered files onto the device being carved.
    const QString destDev = deviceOf(dest);
    if (!destDev.isEmpty() && (destDev == src.path || baseDisk(destDev) == baseDisk(src.path))) {
        QMessageBox::critical(this, tr("Unsafe destination"),
            tr("The destination is on the same disk you are recovering from (%1). "
               "Recovering onto it would overwrite the very files you are trying to "
               "rescue. Choose a folder on another disk.").arg(src.path));
        return;
    }
    if (src.mounted() &&
        QMessageBox::warning(this, tr("Source is mounted"),
            tr("%1 is mounted at %2. Recovery is more reliable on an unmounted "
               "partition. Continue anyway?").arg(src.path, src.mountpoint),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Which file families to recover ("all" → empty list).
    QStringList typeIds;
    if (!m_allTypes->isChecked()) {
        for (int i = 0; i < m_typeList->count(); ++i)
            if (m_typeList->item(i)->checkState() == Qt::Checked)
                typeIds << m_typeList->item(i)->data(Qt::UserRole).toString();
        if (typeIds.isEmpty()) {
            QMessageBox::information(this, tr("Recovery"),
                tr("Pick at least one file type, or tick “Recover all file types”."));
            return;
        }
    }

    m_destUsed = dest;
    QString prog = "pkexec";
    QStringList args = QStringList{"photorec"} + RecoveryTool::photorecArgs(src.path, dest, typeIds);
    CommandUtil::wrapForHost(prog, args);

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, [this]{
        const QStringList lines = QString::fromUtf8(m_proc->readAll()).split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty()) m_status->setText(lines.last().trimmed());
    });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus){
        m_timer->stop();
        const int n = recoveredCount();
        QString msg = code == 0
            ? tr("Recovery finished — %1 file(s) recovered to %2").arg(n).arg(m_destUsed)
            : tr("Recovery stopped (exit %1) — %2 file(s) recovered so far").arg(code).arg(n);
        if (m_organize->isChecked() && n > 0) {
            const int org = RecoveryTool::organizeByType(m_destUsed);
            msg += tr("  ·  sorted %1 file(s) into per-type folders").arg(org);
        }
        setRunning(false);
        m_status->setText(msg);
        m_open->setEnabled(true);
        m_proc->deleteLater(); m_proc = nullptr;
    });

    setRunning(true);
    m_status->setText(tr("Starting PhotoRec on %1…").arg(src.path));
    m_proc->start(prog, args);
    m_timer->start();
}

void RecoveryPage::cancelRecovery()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        m_proc->waitForFinished(2000);
        if (m_proc->state() != QProcess::NotRunning) m_proc->kill();
    }
}

void RecoveryPage::openFolder()
{
    if (!m_destUsed.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(m_destUsed));
}

int RecoveryPage::recoveredCount() const
{
    if (m_destUsed.isEmpty()) return 0;
    int n = 0;
    // PhotoRec writes into gt-stacer-recovery.1, .2, … under the destination.
    const QDir base(m_destUsed);
    for (const QString &d : base.entryList({"gt-stacer-recovery.*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDirIterator it(base.filePath(d), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); ++n; }
    }
    return n;
}

void RecoveryPage::tickCount()
{
    m_status->setText(tr("Recovering… %1 file(s) so far").arg(recoveredCount()));
}

void RecoveryPage::setRunning(bool running)
{
    m_progress->setVisible(running);
    m_start->setEnabled(!running);
    m_cancel->setEnabled(running);
    m_source->setEnabled(!running);
    m_dest->setEnabled(!running);
    m_browse->setEnabled(!running);
    m_refresh->setEnabled(!running);
    m_allTypes->setEnabled(!running);
    m_organize->setEnabled(!running);
    m_typeList->setEnabled(!running && !m_allTypes->isChecked());
    if (running) m_open->setEnabled(false);
}

void RecoveryPage::changeEvent(QEvent *event) { QWidget::changeEvent(event); }

void RecoveryPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_source && (m_proc == nullptr)) refreshSources();   // devices may have changed
}
