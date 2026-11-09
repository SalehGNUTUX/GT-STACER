#include "removable_tab.h"
#include "../../Widgets/failure_text.h"
#include "../../Widgets/bidi_text.h"
#include "../../Widgets/loading_overlay.h"
#include "../../Managers/theme.h"
#include "../../../gt-stacer-core/Tools/disk_image_tool.h"
#include "../../../gt-stacer-core/Tools/notification_tool.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDBusConnection>
#include <QDate>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHash>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

enum Col { ColName, ColFs, ColSize, ColUsed, ColMount, ColCount };
constexpr int kDiskRole = Qt::UserRole;        // index into m_disks
constexpr int kPartRole = Qt::UserRole + 1;    // partition device ("" = disk row)

QString fsName(const QString &fs)
{
    static const QHash<QString, QString> names = {
        {"vfat", "FAT32"}, {"exfat", "exFAT"}, {"ntfs", "NTFS"}, {"ext4", "ext4"}, {"ext3", "ext3"},
        {"ext2", "ext2"}, {"btrfs", "Btrfs"}, {"xfs", "XFS"}, {"f2fs", "F2FS"}, {"udf", "UDF"},
        {"nilfs2", "NILFS2"}};
    return names.value(fs, fs);
}

// Who can use a filesystem: Windows, macOS, Linux, TVs/cars/cameras.
// 2 = yes, 1 = partly (read-only / recent devices only), 0 = no.
struct Compat { int win, mac, linux, devices; const char *maxFile; };
Compat compatOf(const QString &fs)
{
    if (fs == "vfat")  return {2, 2, 2, 2, "4 GB"};
    if (fs == "exfat") return {2, 2, 2, 1, "∞"};
    if (fs == "ntfs")  return {2, 1, 2, 0, "∞"};
    if (fs == "udf")   return {2, 2, 2, 1, "∞"};
    return {0, 0, 2, 0, "∞"};                       // ext*, btrfs, xfs, f2fs, nilfs2: Linux only
}

struct OpOutcome {
    CommandResult result;
    bool          consistent = true;
    bool          remounted = true;
    QStringList   holders;
};

} // namespace

RemovableTab::RemovableTab(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 12, 0, 0);
    root->setSpacing(10);

    m_intro = new QLabel(this);
    m_intro->setObjectName("introText");
    m_intro->setWordWrap(true);
    root->addWidget(m_intro);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(ColCount);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(ColName, 300);
    m_tree->setColumnWidth(ColFs, 90);
    m_tree->setColumnWidth(ColSize, 90);
    m_tree->setColumnWidth(ColUsed, 90);
    root->addWidget(m_tree, 1);

    m_empty = new QLabel(this);
    m_empty->setObjectName("introText");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setVisible(false);
    root->addWidget(m_empty, 1);

    // Actions, grouped from harmless to destructive (left → right).
    auto *row = new QHBoxLayout;
    m_mountBtn  = new QPushButton(this);
    m_openBtn   = new QPushButton(this);
    m_checkBtn  = new QPushButton(this);
    m_repairBtn = new QPushButton(this);
    m_healthBtn = new QPushButton(this);
    m_scanBtn   = new QPushButton(this);
    m_backupBtn = new QPushButton(this);
    m_formatBtn = new QPushButton(this);
    m_formatBtn->setObjectName("dangerButton");
    m_refreshBtn = new QPushButton(this);
    // Two rows so the page never gets wider than the window: everyday actions
    // first, then the slow / destructive ones with Format alone at the end.
    for (QPushButton *b : {m_mountBtn, m_openBtn, m_checkBtn, m_repairBtn}) row->addWidget(b);
    row->addStretch();
    row->addWidget(m_refreshBtn);
    root->addLayout(row);
    auto *row2 = new QHBoxLayout;
    for (QPushButton *b : {m_healthBtn, m_scanBtn, m_backupBtn}) row2->addWidget(b);
    row2->addStretch();
    row2->addWidget(m_formatBtn);
    root->addLayout(row2);

    auto *progressRow = new QHBoxLayout;
    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    m_stopBtn = new QPushButton(this);
    m_stopBtn->setVisible(false);
    progressRow->addWidget(m_progress, 1);
    progressRow->addWidget(m_stopBtn);
    root->addLayout(progressRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &RemovableTab::updateButtons);
    connect(m_mountBtn,  &QPushButton::clicked, this, &RemovableTab::toggleMount);
    connect(m_openBtn,   &QPushButton::clicked, this, &RemovableTab::openFolder);
    connect(m_checkBtn,  &QPushButton::clicked, this, [this]{ checkOrRepair(false); });
    connect(m_repairBtn, &QPushButton::clicked, this, [this]{ checkOrRepair(true); });
    connect(m_healthBtn, &QPushButton::clicked, this, &RemovableTab::showHealth);
    connect(m_scanBtn,   &QPushButton::clicked, this, &RemovableTab::surfaceScan);
    connect(m_backupBtn, &QPushButton::clicked, this, &RemovableTab::backupImage);
    connect(m_formatBtn, &QPushButton::clicked, this, &RemovableTab::formatTarget);
    connect(m_refreshBtn, &QPushButton::clicked, this, &RemovableTab::refresh);
    connect(m_stopBtn,   &QPushButton::clicked, this, &RemovableTab::stopJob);

    // Plugging / unplugging a stick updates the list by itself.
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(700);
    connect(m_debounce, &QTimer::timeout, this, [this]{
        if (isVisible() && !m_busy) refresh(); else m_dirty = true;
    });
    QDBusConnection bus = QDBusConnection::systemBus();
    for (const char *sig : {"InterfacesAdded", "InterfacesRemoved"})
        bus.connect("org.freedesktop.UDisks2", "/org/freedesktop/UDisks2",
                    "org.freedesktop.DBus.ObjectManager", sig, this, SLOT(onUdisksChanged()));

    retranslate();
    refresh();
}

RemovableTab::~RemovableTab()
{
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->closeWriteChannel();               // the root guard stops the job
            if (!m_proc->waitForFinished(3000)) { m_proc->kill(); m_proc->waitForFinished(1000); }
        }
        if (m_job == Job::Backup && !m_jobOutFile.isEmpty()) QFile::remove(m_jobOutFile);
    }
}

void RemovableTab::retranslate()
{
    m_intro->setText(Bidi::isolate(tr("Check, repair, format and back up USB sticks and memory cards. "
                        "Internal disks are never shown here, and a removable disk that holds "
                        "your system is locked. Most actions need no password.")));
    m_empty->setText(Bidi::isolate(tr("No USB stick or memory card is connected.\n\n"
                        "Plug one in — it appears here automatically.")));
    m_mountBtn->setText(tr("Mount"));
    m_openBtn->setText(tr("Open folder"));
    m_checkBtn->setText(tr("Check"));
    m_checkBtn->setToolTip(tr("Look for filesystem errors without changing anything."));
    m_repairBtn->setText(tr("Repair"));
    m_repairBtn->setToolTip(tr("Fix filesystem errors (the partition is unmounted while it runs)."));
    m_healthBtn->setText(tr("Health"));
    m_healthBtn->setToolTip(tr("Read the device's own health report (S.M.A.R.T.), if it has one."));
    m_scanBtn->setText(tr("Surface scan"));
    m_scanBtn->setToolTip(tr("Read every block to find unreadable areas. Read-only — nothing is written."));
    m_backupBtn->setText(tr("Back up to image…"));
    m_backupBtn->setToolTip(tr("Save an exact copy of the whole device (or partition) to an image file."));
    m_formatBtn->setText(tr("Format…"));
    m_refreshBtn->setText(tr("Refresh"));
    m_stopBtn->setText(tr("Stop"));
    m_tree->setHeaderLabels({tr("Device / partition"), tr("Filesystem"), tr("Size"), tr("Used"),
                             tr("Mounted at")});
    populate();
}

void RemovableTab::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && m_intro) retranslate();
    QWidget::changeEvent(event);
}

void RemovableTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_busy && (m_dirty || m_disks.isEmpty())) { m_dirty = false; refresh(); }
}

void RemovableTab::onUdisksChanged() { m_debounce->start(); }

// ── List ────────────────────────────────────────────────────────────────

void RemovableTab::refresh()
{
    m_disks = RemovableTool::disks();
    populate();
}

QString RemovableTab::lockedText(const QString &reason) const
{
    if (reason == RemovableTool::kProtSwap) return tr("Locked — used as swap");
    return tr("Locked — holds the system or is mounted outside the removable-media folders");
}

void RemovableTab::populate()
{
    if (!m_tree) return;
    const QString prev = target().device;
    m_tree->clear();
    QTreeWidgetItem *reselect = nullptr;
    for (int i = 0; i < m_disks.size(); ++i) {
        const RemovableTool::Disk &d = m_disks[i];
        auto *top = new QTreeWidgetItem(m_tree);
        QString name = d.title();
        if (d.readOnly) name += "  " + tr("[write-protected]");
        top->setText(ColName, name);
        top->setToolTip(ColName, QString("%1 · %2 · %3").arg(d.device, d.transport.toUpper(),
                                  d.tableType.isEmpty() ? tr("no partition table") : d.tableType.toUpper()));
        top->setText(ColSize, FormatUtil::formatBytes(d.size));
        top->setData(ColName, kDiskRole, i);
        top->setData(ColName, kPartRole, QString());
        QFont f = top->font(ColName); f.setBold(true); top->setFont(ColName, f);
        if (!d.protectedReason.isEmpty()) {
            top->setText(ColMount, lockedText(d.protectedReason));
            top->setForeground(ColMount, Theme::red());
        }
        if (d.device == prev) reselect = top;
        for (const RemovableTool::Part &p : d.parts) {
            if (p.device == d.device) {         // filesystem directly on the disk
                top->setText(ColFs, fsName(p.fsType));
                top->setText(ColMount, d.protectedReason.isEmpty()
                                 ? (p.mountPoint.isEmpty() ? tr("(not mounted)") : p.mountPoint)
                                 : top->text(ColMount));
                if (p.used >= 0) top->setText(ColUsed, FormatUtil::formatBytes(p.used));
                continue;
            }
            auto *c = new QTreeWidgetItem(top);
            const QString dev = p.device.mid(p.device.lastIndexOf('/') + 1);
            c->setText(ColName, p.label.isEmpty() ? dev : QString("%1  (%2)").arg(p.label, dev));
            c->setToolTip(ColName, p.device);
            c->setText(ColFs, p.fsType.isEmpty() ? tr("(empty)") : fsName(p.fsType));
            c->setText(ColSize, FormatUtil::formatBytes(p.size));
            if (p.used >= 0) c->setText(ColUsed, FormatUtil::formatBytes(p.used));
            c->setText(ColMount, p.mountPoint.isEmpty() ? tr("(not mounted)") : p.mountPoint);
            c->setData(ColName, kDiskRole, i);
            c->setData(ColName, kPartRole, p.device);
            if (p.device == prev) reselect = c;
        }
        top->setExpanded(true);
    }
    if (!reselect && m_tree->topLevelItemCount() > 0) reselect = m_tree->topLevelItem(0);
    if (reselect) m_tree->setCurrentItem(reselect);

    const bool none = m_disks.isEmpty();
    m_tree->setVisible(!none);
    m_empty->setVisible(none);
    if (!m_busy)
        m_status->setText(Bidi::isolate(none ? QString() : tr("Select a device or one of its partitions.")));
    updateButtons();
}

RemovableTab::Target RemovableTab::target() const
{
    Target t;
    if (!m_tree) return t;
    const QTreeWidgetItem *it = m_tree->currentItem();
    if (!it) return t;
    bool ok = false;
    t.disk = it->data(ColName, kDiskRole).toInt(&ok);
    if (!ok || t.disk < 0 || t.disk >= m_disks.size()) { t.disk = -1; return t; }
    const RemovableTool::Disk &d = m_disks[t.disk];
    const QString partDev = it->data(ColName, kPartRole).toString();
    if (partDev.isEmpty()) {
        t.isDisk = true;
        t.device = d.device;
        // A disk row also stands for its filesystem when there is exactly one.
        const auto &parts = d.parts;
        if (parts.size() == 1) { t.part = parts.first(); t.hasPart = true; }
    } else {
        t.device = partDev;
        for (const RemovableTool::Part &p : d.parts)
            if (p.device == partDev) { t.part = p; t.hasPart = true; }
    }
    return t;
}

void RemovableTab::updateButtons()
{
    const Target t = target();
    const bool sel = t.disk >= 0;
    const bool locked = sel && !m_disks[t.disk].protectedReason.isEmpty();
    const bool ro = sel && m_disks[t.disk].readOnly;
    const bool fs = t.hasPart && !t.part.fsType.isEmpty();
    const bool mounted = t.hasPart && !t.part.mountPoint.isEmpty();
    const bool idle = !m_busy;

    m_mountBtn->setText(mounted ? tr("Unmount") : tr("Mount"));
    m_mountBtn->setEnabled(idle && fs && !locked);
    m_openBtn->setEnabled(idle && mounted);
    m_checkBtn->setEnabled(idle && fs && !locked);
    m_repairBtn->setEnabled(idle && fs && !locked && !ro);
    m_healthBtn->setEnabled(idle && sel);
    m_scanBtn->setEnabled(idle && sel && !locked);
    m_backupBtn->setEnabled(idle && sel && !locked);
    m_formatBtn->setEnabled(idle && sel && !locked && !ro);
    m_refreshBtn->setEnabled(idle);
    m_formatBtn->setText(sel && t.isDisk ? tr("Format device…") : tr("Format partition…"));
}

void RemovableTab::setBusy(bool busy, bool stoppable)
{
    m_busy = busy;
    m_tree->setEnabled(!busy);
    m_progress->setVisible(busy);
    if (busy && !stoppable) m_progress->setRange(0, 0);
    m_stopBtn->setVisible(busy && stoppable);
    m_stopBtn->setEnabled(busy && stoppable);
    updateButtons();
    // Short steps show the waiting card; a stoppable job keeps its bar + Stop.
    if (!busy || stoppable) LoadingOverlay::on(this)->stop();
}

bool RemovableTab::verifyOrExplain(const Target &t)
{
    const RemovableTool::Disk &d = m_disks[t.disk];
    const QString why = RemovableTool::verifyTarget(t.device, d.serial, d.size);
    if (why.isEmpty()) return true;
    QString msg;
    if (why == "gone")          msg = tr("The device was removed.");
    else if (why == "changed")  msg = tr("A different device is now connected under this name. Select it again before continuing.");
    else if (why == "readonly") msg = tr("The device is write-protected. Check the lock switch on the side of the memory card.");
    else                        msg = lockedText(why);
    QMessageBox::warning(this, tr("USB & memory cards"), Bidi::isolate(msg));
    refresh();
    return false;
}

// ── Mount / open ───────────────────────────────────────────────────────

void RemovableTab::toggleMount()
{
    const Target t = target();
    if (!t.hasPart || m_busy) return;
    const QString dev = t.part.device;
    const bool mounted = !t.part.mountPoint.isEmpty();
    const QString mp = t.part.mountPoint;
    setBusy(true);
    m_status->setText(Bidi::isolate(mounted ? tr("Unmounting %1…").arg(dev) : tr("Mounting %1…").arg(dev)));
    LoadingOverlay::on(this)->start(mounted ? tr("Unmounting") : tr("Mounting"), dev);
    auto *w = new QFutureWatcher<OpOutcome>(this);
    connect(w, &QFutureWatcher<OpOutcome>::finished, this, [this, w, dev, mounted]{
        const OpOutcome o = w->result();
        w->deleteLater();
        setBusy(false);
        refresh();
        if (o.result.ok()) {
            m_status->setText(Bidi::isolate(mounted ? tr("%1 was unmounted — it can be removed safely.").arg(dev)
                                      : tr("%1 was mounted.").arg(dev)));
            return;
        }
        if (!o.holders.isEmpty()) {
            QMessageBox::warning(this, tr("USB & memory cards"),
                Bidi::isolate(tr("%1 is still in use, so it was not unmounted.\n\nThese programs have files or "
                   "a folder open on it:\n    %2\n\nClose them, then try again.")
                    .arg(dev, o.holders.join("\n    "))));
        } else {
            FailureText::show(this, tr("USB & memory cards"),
                mounted ? tr("%1 was not unmounted.").arg(dev) : tr("%1 was not mounted.").arg(dev), o.result);
        }
        m_status->setText(Bidi::isolate(FailureText::brief(o.result)));
    });
    w->setFuture(QtConcurrent::run([dev, mounted, mp]{
        OpOutcome o;
        o.result = mounted ? RemovableTool::unmount(dev) : RemovableTool::mount(dev);
        if (mounted && !o.result.ok()
            && CommandFailure::classify(o.result).kind == CommandFailure::Kind::DeviceBusy)
            o.holders = DiskImageTool::holders(mp);
        return o;
    }));
}

void RemovableTab::openFolder()
{
    const Target t = target();
    if (t.hasPart && !t.part.mountPoint.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(t.part.mountPoint));
}

// ── Check / repair ─────────────────────────────────────────────────────

void RemovableTab::checkOrRepair(bool repair)
{
    const Target t = target();
    if (!t.hasPart || t.part.fsType.isEmpty() || m_busy) return;
    if (!verifyOrExplain(t)) return;
    const QString dev = t.part.device;
    const QString mp = t.part.mountPoint;

    if (repair && QMessageBox::question(this, tr("Repair"),
            Bidi::isolate(tr("Repair the filesystem on %1?\n\nRepair writes to the device to fix errors. It is "
               "usually safe, but if the files on it matter, copy them somewhere else first if you "
               "still can.%2")
                .arg(dev, mp.isEmpty() ? QString() : "\n\n" + tr("It is unmounted while the repair runs and mounted again afterwards."))))
            != QMessageBox::Yes)
        return;

    setBusy(true);
    m_status->setText(Bidi::isolate(repair ? tr("Repairing %1…").arg(dev) : tr("Checking %1…").arg(dev)));
    LoadingOverlay::on(this)->start(repair ? tr("Repairing the filesystem") : tr("Checking the filesystem"),
        tr("%1 — your password may be asked.").arg(dev));
    auto *w = new QFutureWatcher<OpOutcome>(this);
    connect(w, &QFutureWatcher<OpOutcome>::finished, this, [this, w, dev, repair]{
        const OpOutcome o = w->result();
        w->deleteLater();
        setBusy(false);
        refresh();
        if (!o.holders.isEmpty()) {
            QMessageBox::warning(this, tr("USB & memory cards"),
                Bidi::isolate(tr("%1 is in use, so it could not be unmounted for the %2.\n\nThese programs have "
                   "files or a folder open on it:\n    %3\n\nClose them, then try again.")
                    .arg(dev, repair ? tr("repair") : tr("check"), o.holders.join("\n    "))));
            m_status->setText(Bidi::isolate(FailureText::brief(o.result)));
            return;
        }
        if (!o.result.ok()) {
            FailureText::show(this, repair ? tr("Repair") : tr("Check"),
                repair ? tr("%1 could not be repaired.").arg(dev) : tr("%1 could not be checked.").arg(dev),
                o.result);
            m_status->setText(Bidi::isolate(FailureText::brief(o.result)));
            return;
        }
        QString msg;
        if (repair)
            msg = o.consistent ? tr("%1 was repaired — no errors remain.").arg(dev)
                               : tr("The repair ran, but %1 still has errors. Copy your files off it and "
                                    "format it.").arg(dev);
        else
            msg = o.consistent ? tr("No errors found on %1.").arg(dev)
                               : tr("Errors were found on %1. Click “Repair” to fix them.").arg(dev);
        if (!o.remounted) msg += " " + tr("(It could not be mounted again — click “Mount”.)");
        m_status->setText(Bidi::isolate(msg));
        QMessageBox::information(this, repair ? tr("Repair") : tr("Check"), Bidi::isolate(msg));
    });
    w->setFuture(QtConcurrent::run([dev, mp, repair]{
        OpOutcome o;
        if (!mp.isEmpty()) {                       // UDisks2 works on unmounted filesystems only
            o.result = RemovableTool::unmount(dev);
            if (!o.result.ok()) {
                if (CommandFailure::classify(o.result).kind == CommandFailure::Kind::DeviceBusy)
                    o.holders = DiskImageTool::holders(mp);
                return o;
            }
        }
        if (repair) {
            o.result = RemovableTool::repair(dev);
            if (o.result.ok()) RemovableTool::check(dev, &o.consistent);
        } else {
            o.result = RemovableTool::check(dev, &o.consistent);
        }
        if (!mp.isEmpty()) o.remounted = RemovableTool::mount(dev).ok();   // put it back as it was
        return o;
    }));
}

// ── Health ─────────────────────────────────────────────────────────────

void RemovableTab::showHealth()
{
    const Target t = target();
    if (t.disk < 0 || m_busy) return;
    const QString disk = m_disks[t.disk].device;
    const QString title = m_disks[t.disk].title();
    setBusy(true);
    m_status->setText(Bidi::isolate(tr("Reading the health report of %1… (your password may be asked)").arg(title)));
    LoadingOverlay::on(this)->start(tr("Reading the health report"),
        tr("%1 — your password may be asked.").arg(title));
    struct H { RemovableTool::Health h; CommandResult r; };
    auto *w = new QFutureWatcher<H>(this);
    connect(w, &QFutureWatcher<H>::finished, this, [this, w, title]{
        const H x = w->result();
        w->deleteLater();
        setBusy(false);
        if (!x.r.ok() && !x.h.supported) {
            FailureText::show(this, tr("Health"), tr("The health report of %1 could not be read.").arg(title), x.r);
            m_status->setText(Bidi::isolate(FailureText::brief(x.r)));
            return;
        }
        if (!x.h.supported) {
            const QString msg = tr("%1 does not report its health (S.M.A.R.T.).\n\nThat is normal "
                                   "for USB sticks and memory cards — almost none of them have it — "
                                   "and it is not a sign of a problem. Use “Surface scan” to find "
                                   "unreadable areas instead.").arg(title);
            m_status->setText(Bidi::isolate(msg.section('\n', 0, 0)));
            QMessageBox box(QMessageBox::Information, tr("Health"), Bidi::isolate(msg), QMessageBox::Ok, this);
            box.setDetailedText(x.r.output);     // smartctl's own answer, for the curious
            box.exec();
            return;
        }
        QStringList lines;
        lines << (x.h.passed ? tr("Overall: <b>healthy</b> (the device's self-assessment passed).")
                             : tr("Overall: <b>FAILING</b> — the device reports that it is about to fail. "
                                  "Copy your files off it now and replace it."));
        if (x.h.powerOnHours >= 0) lines << tr("Powered on for: %1 hours").arg(x.h.powerOnHours);
        if (x.h.temperature  >= 0) lines << tr("Temperature: %1 °C").arg(x.h.temperature);
        if (x.h.reallocated  >= 0) lines << (x.h.reallocated == 0
            ? tr("Replaced (reallocated) sectors: 0 — good")
            : tr("Replaced (reallocated) sectors: %1 — the surface is wearing out; keep a backup").arg(x.h.reallocated));
        if (x.h.pending >= 0 && x.h.pending > 0)
            lines << tr("Sectors waiting to be replaced: %1 — some data may already be unreadable").arg(x.h.pending);
        QMessageBox box(x.h.passed ? QMessageBox::Information : QMessageBox::Warning,
                        tr("Health"), QString("<b>%1</b><br><br>%2").arg(title.toHtmlEscaped(), lines.join("<br>")),
                        QMessageBox::Ok, this);
        box.setTextFormat(Qt::RichText);
        box.setDetailedText(x.r.output);
        box.exec();
        m_status->setText(Bidi::isolate(x.h.passed ? tr("%1: healthy.").arg(title) : tr("%1: FAILING.").arg(title)));
    });
    w->setFuture(QtConcurrent::run([disk]{ H x; x.h = RemovableTool::health(disk, &x.r); return x; }));
}

// ── Long jobs: surface scan, image backup ──────────────────────────────

void RemovableTab::surfaceScan()
{
    const Target t = target();
    if (t.disk < 0 || m_busy) return;
    if (!verifyOrExplain(t)) return;
    // A COPY: refresh() (here, or from a udisks change while a dialog is open)
    // rebuilds m_disks — a reference would dangle and crash (26.12, found in testing).
    const RemovableTool::Disk d = m_disks[t.disk];
    // badblocks refuses a mounted device — ask to unmount what is mounted.
    QStringList mounted;
    for (const RemovableTool::Part &p : d.parts)
        if (!p.mountPoint.isEmpty() && (t.isDisk || p.device == t.device)) mounted << p.device;
    if (QMessageBox::question(this, tr("Surface scan"),
            Bidi::isolate(tr("Read every block of %1 to find unreadable areas?\n\nThis only READS — nothing on the "
               "device is changed. It can take from a few minutes to over an hour, depending on the "
               "size and speed. You can stop it at any time.%2\n\nYour password will be asked.")
                .arg(t.isDisk ? d.title() : t.device,
                     mounted.isEmpty() ? QString() : "\n\n" + tr("It will be unmounted first."))))
            != QMessageBox::Yes)
        return;
    m_jobUnmounted.clear();
    for (const QString &dev : mounted) {
        const CommandResult r = RemovableTool::unmount(dev);
        if (!r.ok()) {
            FailureText::show(this, tr("Surface scan"), tr("%1 could not be unmounted.").arg(dev), r);
            for (const QString &done : m_jobUnmounted) RemovableTool::mount(done);   // undo
            m_jobUnmounted.clear();
            refresh();
            return;
        }
        m_jobUnmounted << dev;
    }
    refresh();
    m_badBlocks = 0;
    m_job = Job::Scan;
    startJob(RemovableTool::surfaceScanArgv(t.device), t.isDisk ? d.title() : t.device, QString());
}

void RemovableTab::backupImage()
{
    const Target t = target();
    if (t.disk < 0 || m_busy) return;
    if (!verifyOrExplain(t)) return;
    // A COPY: refresh() (here, or from a udisks change while a dialog is open)
    // rebuilds m_disks — a reference would dangle and crash (26.12, found in testing).
    const RemovableTool::Disk d = m_disks[t.disk];
    const qint64 size = t.isDisk ? d.size : t.part.size;
    QString base = QString(d.vendor + "-" + d.model).simplified();
    base.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "-");
    base.remove(QRegularExpression("^-+|-+$"));
    if (base.isEmpty()) base = t.device.mid(t.device.lastIndexOf('/') + 1);
    const QString suggested = QDir::home().filePath(
        QString("%1-%2.img").arg(base, QDate::currentDate().toString("yyyy-MM-dd")));
    QString path = QFileDialog::getSaveFileName(this, tr("Save the device image as"), suggested,
                                                tr("Disk images (*.img)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".img", Qt::CaseInsensitive)) path += ".img";
    // Never write the image onto the device being copied.
    for (const RemovableTool::Part &p : d.parts)
        if (!p.mountPoint.isEmpty() && (path.startsWith(p.mountPoint + "/"))) {
            QMessageBox::warning(this, tr("Back up to image"),
                Bidi::isolate(tr("The image can't be saved on the device it copies. Choose another place.")));
            return;
        }
    const QStorageInfo st(QFileInfo(path).absolutePath());
    if (st.isValid() && st.bytesAvailable() < size) {
        QMessageBox::warning(this, tr("Back up to image"),
            Bidi::isolate(tr("Not enough free space: the image needs %1, but only %2 is free there.")
                .arg(FormatUtil::formatBytes(size), FormatUtil::formatBytes(st.bytesAvailable()))));
        return;
    }
    // A consistent copy needs nothing writing to it: unmount first.
    m_jobUnmounted.clear();
    for (const RemovableTool::Part &p : d.parts) {
        if (p.mountPoint.isEmpty() || !(t.isDisk || p.device == t.device)) continue;
        const CommandResult r = RemovableTool::unmount(p.device);
        if (!r.ok()) {
            FailureText::show(this, tr("Back up to image"),
                tr("%1 could not be unmounted, so a consistent copy can't be made.").arg(p.device), r);
            for (const QString &done : m_jobUnmounted) RemovableTool::mount(done);   // undo
            m_jobUnmounted.clear();
            refresh();
            return;
        }
        m_jobUnmounted << p.device;
    }
    m_jobSize = size;
    m_job = Job::Backup;
    startJob(RemovableTool::imageArgv(t.device), t.isDisk ? d.title() : t.device, path);
}

void RemovableTab::startJob(const QStringList &argvIn, const QString &label, const QString &outFile)
{
    QStringList args = argvIn;
    QString prog = args.takeFirst();
    // Root, so we can't signal it — the guard stops it when we close our pipe
    // (Stop) or when GT-STACER exits in any way.
    CommandUtil::guardForStop(prog, args, /*asRoot=*/true);
    m_jobLabel = label;
    m_jobOutFile = outFile;
    m_jobOutput.clear();
    m_stopRequested = false;

    m_proc = new QProcess(this);
    if (!outFile.isEmpty()) {
        // dd writes the image to stdout; the file is created by US, so it is the
        // user's file (not root's), and progress comes on stderr.
        m_proc->setStandardOutputFile(outFile, QIODevice::Truncate);
        m_progress->setRange(0, 100);
    } else {
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        m_progress->setRange(0, 100);
    }
    m_progress->setValue(0);
    setBusy(true, /*stoppable=*/true);
    m_status->setText(Bidi::isolate(outFile.isEmpty() ? tr("Scanning %1… (waiting for authorization)").arg(label)
                                        : tr("Copying %1… (waiting for authorization)").arg(label)));

    auto onOutput = [this](const QByteArray &bytes) {
        const QString chunk = QString::fromUtf8(bytes);
        m_jobOutput = (m_jobOutput + chunk).right(20000);
        if (m_job == Job::Scan) {
            static const QRegularExpression pct("(\\d+(?:\\.\\d+)?)% done");
            static const QRegularExpression errs("\\((\\d+)/(\\d+)/(\\d+) errors\\)");
            double last = -1; QRegularExpressionMatch em;
            for (auto it = pct.globalMatch(chunk); it.hasNext();) last = it.next().captured(1).toDouble();
            for (auto it = errs.globalMatch(chunk); it.hasNext();) em = it.next();
            if (last >= 0) m_progress->setValue(int(last));
            if (em.hasMatch()) m_badBlocks = em.captured(1).toInt() + em.captured(2).toInt() + em.captured(3).toInt();
            if (last >= 0)
                m_status->setText(Bidi::isolate(tr("Scanning %1… %2% — unreadable blocks so far: %3")
                                      .arg(m_jobLabel).arg(int(last)).arg(m_badBlocks)));
        } else if (m_job == Job::Backup) {
            static const QRegularExpression bytesRe("(\\d+) bytes");
            qint64 done = -1;
            for (auto it = bytesRe.globalMatch(chunk); it.hasNext();) done = it.next().captured(1).toLongLong();
            if (done >= 0 && m_jobSize > 0) {
                m_progress->setValue(int(qMin<qint64>(100, done * 100 / m_jobSize)));
                m_status->setText(Bidi::isolate(tr("Copying %1… %2 of %3").arg(m_jobLabel,
                    FormatUtil::formatBytes(done), FormatUtil::formatBytes(m_jobSize))));
            }
        }
    };
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this, onOutput]{ onOutput(m_proc->readAllStandardOutput()); });
    connect(m_proc, &QProcess::readyReadStandardError,  this, [this, onOutput]{ onOutput(m_proc->readAllStandardError()); });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st){
        m_proc->deleteLater();
        m_proc = nullptr;
        const Job job = m_job;
        m_job = Job::None;
        // Put the device back as it was — whatever the outcome (done, failed,
        // stopped). A device that was pulled out meanwhile simply isn't there.
        QStringList notRemounted;
        for (const QString &dev : std::as_const(m_jobUnmounted))
            if (QFileInfo::exists(dev) && !RemovableTool::mount(dev).ok()) notRemounted << dev;
        m_jobUnmounted.clear();
        setBusy(false);
        refresh();
        if (!notRemounted.isEmpty()) {
            // Appended after the result message below has been set.
            const QString note = tr("%1 could not be mounted again — select it and click “Mount”.")
                                     .arg(notRemounted.join(", "));
            QTimer::singleShot(0, this, [this, note]{
                m_status->setText(Bidi::isolate(m_status->text() + "  " + note));
            });
        }
        CommandResult r;
        r.program = job == Job::Scan ? "badblocks" : "dd";
        r.viaPkexec = true; r.started = true;
        r.exitCode = st == QProcess::NormalExit ? code : -1;
        r.output = m_jobOutput;

        if (job == Job::Backup) {
            const qint64 written = QFileInfo(m_jobOutFile).size();
            const bool complete = r.ok() && written == m_jobSize;
            if (!complete && !m_jobOutFile.isEmpty()) QFile::remove(m_jobOutFile);   // never leave a partial image
            if (m_stopRequested) { m_status->setText(Bidi::isolate(tr("Backup stopped — the incomplete image was deleted."))); return; }
            if (!complete) {
                m_status->setText(Bidi::isolate(tr("The backup did not complete — the incomplete image was deleted.")));
                FailureText::show(this, tr("Back up to image"), tr("The backup of %1 did not complete.").arg(m_jobLabel), r);
                return;
            }
            const QString msg = tr("Image of %1 saved: %2 (%3).").arg(m_jobLabel, m_jobOutFile, FormatUtil::formatBytes(written));
            m_status->setText(Bidi::isolate(msg));
            NotificationTool::notify(tr("USB & memory cards"), msg, NotificationTool::Urgency::Normal, "gt-stacer");
            return;
        }
        // Surface scan
        if (m_stopRequested) {
            m_status->setText(Bidi::isolate(tr("Scan stopped. Unreadable blocks found so far: %1.").arg(m_badBlocks)));
            return;
        }
        if (!r.ok()) {
            m_status->setText(Bidi::isolate(FailureText::brief(r)));
            FailureText::show(this, tr("Surface scan"), tr("The scan of %1 did not complete.").arg(m_jobLabel), r);
            return;
        }
        const QString msg = m_badBlocks == 0
            ? tr("Surface scan of %1 finished: every block could be read — no bad blocks.").arg(m_jobLabel)
            : tr("Surface scan of %1 finished: %2 unreadable block(s). The device is failing — copy your "
                 "files off it and stop using it for anything important.").arg(m_jobLabel).arg(m_badBlocks);
        m_status->setText(Bidi::isolate(msg));
        QMessageBox::information(this, tr("Surface scan"), Bidi::isolate(msg));
        NotificationTool::notify(tr("USB & memory cards"), msg, NotificationTool::Urgency::Normal, "gt-stacer");
    });
    m_proc->start(prog, args);
}

void RemovableTab::stopJob()
{
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
    m_stopRequested = true;
    m_stopBtn->setEnabled(false);
    m_status->setText(Bidi::isolate(tr("Stopping…")));
    m_proc->closeWriteChannel();          // the root guard SIGTERMs the job
}

// ── Format ─────────────────────────────────────────────────────────────

void RemovableTab::formatTarget()
{
    const Target t = target();
    if (t.disk < 0 || m_busy) return;
    if (!verifyOrExplain(t)) return;
    const RemovableTool::Disk d = m_disks[t.disk];
    const bool whole = t.isDisk;
    const QStringList types = RemovableTool::formatTypes();
    if (types.isEmpty()) {
        QMessageBox::warning(this, tr("Format"),
            Bidi::isolate(tr("No formatting tools are installed. Install dosfstools (FAT32), exfatprogs (exFAT), "
               "ntfs-3g (NTFS) or e2fsprogs (ext4).")));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(whole ? tr("Format device") : tr("Format partition"));
    auto *v = new QVBoxLayout(&dlg);
    auto *what = new QLabel(&dlg);
    what->setWordWrap(true);
    what->setTextFormat(Qt::RichText);
    what->setText(Bidi::isolate(whole
        ? tr("<b>%1</b><br>The whole device gets a new partition table and one partition using all "
             "of its space. <b>Every partition and file on it is erased.</b>").arg(d.title().toHtmlEscaped())
        : tr("<b>%1</b> on %2<br>Only this partition is formatted; other partitions are not touched. "
             "<b>Every file on it is erased.</b>")
              .arg((t.part.label.isEmpty() ? t.device : t.part.label + " (" + t.device + ")").toHtmlEscaped(),
                   d.title().toHtmlEscaped())));
    v->addWidget(what);

    const qint64 targetSize = whole ? d.size : t.part.size;
    const RemovableTool::Advice advice = RemovableTool::advise(d, targetSize);
    const QVector<RemovableTool::FsOption> fsOptions = RemovableTool::formatOptions();

    auto *form = new QFormLayout;
    auto *fsBox = new QComboBox(&dlg);
    fsBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    fsBox->setMinimumContentsLength(18);
    // Recommended first, then the rest in their usual order; the ones this
    // system can't create are listed disabled, with the package to install.
    QVector<RemovableTool::FsOption> ordered;
    for (const auto &o : fsOptions) if (o.fs == advice.fs) ordered << o;
    for (const auto &o : fsOptions) if (o.fs != advice.fs) ordered << o;
    for (const auto &o : ordered) {
        QString text = fsName(o.fs);
        if (o.fs == advice.fs) text += "  — " + tr("recommended");
        if (!o.available)      text += "  — " + tr("needs the “%1” package").arg(o.package);
        fsBox->addItem(text, o.fs);
        if (!o.available)
            if (auto *model = qobject_cast<QStandardItemModel *>(fsBox->model()))
                if (QStandardItem *item = model->item(fsBox->count() - 1)) item->setEnabled(false);
    }
    form->addRow(tr("Filesystem:"), fsBox);

    auto *card = new QLabel(&dlg);                 // compatibility + advice for the selected one
    card->setWordWrap(true);
    card->setTextFormat(Qt::RichText);
    card->setMinimumWidth(460);
    form->addRow(QString(), card);

    auto *labelEdit = new QLineEdit(&dlg);
    form->addRow(tr("Name (label):"), labelEdit);
    auto *hint = new QLabel(&dlg);
    hint->setObjectName("introText");
    form->addRow(QString(), hint);
    v->addLayout(form);

    auto *ack = new QCheckBox(tr("I understand that everything on it will be permanently erased"), &dlg);
    v->addWidget(ack);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton *go = buttons->addButton(tr("Format"), QDialogButtonBox::DestructiveRole);
    go->setObjectName("dangerButton");
    v->addWidget(buttons);

    const QString ok = Theme::green().name(), part = Theme::yellow().name(), no = Theme::red().name();
    auto mark = [&](int level) {
        return level == 2 ? QString("<span style='color:%1'>✔</span>").arg(ok)
             : level == 1 ? QString("<span style='color:%1'>◐</span>").arg(part)
                          : QString("<span style='color:%1'>✘</span>").arg(no);
    };
    const qint64 gb32 = qint64(32) * 1000 * 1000 * 1000 + qint64(512) * 1024 * 1024;

    auto describe = [&](const QString &fs) -> QString {
        if (fs == "vfat")  return tr("The most compatible: every computer, TV, car radio, camera and game console reads it. Files can't be larger than 4 GB.");
        if (fs == "exfat") return tr("Like FAT32 but without the 4 GB file limit. Windows, macOS, Linux and most devices made after about 2012 read it.");
        if (fs == "ntfs")  return tr("The Windows filesystem. macOS can only read it, and most TVs, cars and cameras can't use it.");
        if (fs == "ext4")  return tr("The standard Linux filesystem: reliable, with a journal and Linux permissions.");
        if (fs == "f2fs")  return tr("Designed for flash memory (USB sticks, SD cards): spreads writes to reduce wear. Linux and Android.");
        if (fs == "btrfs") return tr("Modern Linux filesystem: checksums, compression and snapshots. Best on larger drives.");
        if (fs == "xfs")   return tr("Linux filesystem for large drives and big files. It can grow but never shrink.");
        if (fs == "ext3")  return tr("The older version of ext4 — choose ext4 unless an old Linux system must read it.");
        if (fs == "ext2")  return tr("Linux without a journal: fewer writes to the flash, but unplugging without “Unmount” can damage it.");
        if (fs == "udf")   return tr("Read by Windows, macOS and Linux with large files, but many TVs, cars and cameras don't support it.");
        if (fs == "nilfs2")return tr("Linux filesystem that keeps continuous snapshots — for special uses.");
        return {};
    };

    auto validate = [&]{
        const QString fs = fsBox->currentData().toString();
        const Compat c = compatOf(fs);
        QString html = "<p>" + describe(fs).toHtmlEscaped() + "</p>";
        html += QString("<p>%1 %2 &nbsp; %3 %4 &nbsp; %5 %6 &nbsp; %7 %8 &nbsp; · &nbsp;%9</p>")
                    .arg(mark(c.win), tr("Windows"), mark(c.mac), tr("macOS"), mark(c.linux), tr("Linux"),
                         mark(c.devices), tr("TVs, cars, cameras"),
                         tr("largest file: %1").arg(QString::fromUtf8(c.maxFile) == "∞" ? tr("no practical limit")
                                                                                      : QString::fromUtf8(c.maxFile)));
        // Advice for THIS device.
        QStringList notes;
        if (fs == advice.fs) {
            if (advice.why == "sd-small")  notes << tr("Recommended for this card: FAT32 is the SD standard for cards up to 32 GB (SDHC) — cameras, phones and consoles expect it.");
            if (advice.why == "sd-large")  notes << tr("Recommended for this card: exFAT is the SD standard for cards above 32 GB (SDXC).");
            if (advice.why == "usb-small") notes << tr("Recommended: works with every computer and device. Choose exFAT only if you need files larger than 4 GB.");
            if (advice.why == "usb-large") notes << tr("Recommended for this size: large files, and readable on Windows, macOS, Linux and most recent TVs.");
            if (advice.why == "drive")     notes << tr("Recommended to share this large drive between systems. Choose NTFS if it stays with Windows, ext4 if it stays with Linux.");
        } else {
            if (fs == "vfat" && targetSize > gb32)
                notes << tr("⚠ Larger than 32 GB: Windows can read FAT32 of this size but can't create or repair it, and no file can exceed 4 GB. exFAT suits this size better.");
            if (fs == "vfat" && advice.sdCard && targetSize > gb32)
                notes << tr("⚠ SD cards above 32 GB are made for exFAT; some cameras refuse FAT32 on them.");
            if (fs == "exfat" && advice.sdCard && targetSize <= gb32)
                notes << tr("⚠ Cards up to 32 GB are made for FAT32; some cameras and older devices don't read exFAT on them.");
            if (c.win == 0 && c.mac == 0)
                notes << tr("⚠ Windows and macOS can't open it without extra software — use it only if this device stays with Linux.");
            if (fs == "btrfs" && targetSize < qint64(16) * 1000 * 1000 * 1000)
                notes << tr("⚠ On a small stick Btrfs's extra metadata costs space with little benefit — ext4 or F2FS fit better.");
            if (fs == "ntfs" && advice.sdCard)
                notes << tr("⚠ Cameras and phones can't use NTFS memory cards.");
        }
        const qint64 minimum = RemovableTool::minSize(fs);
        const bool tooSmall = minimum > 0 && targetSize < minimum;
        if (tooSmall)
            notes << tr("✘ Too small for %1 — it needs at least %2.").arg(fsName(fs), FormatUtil::formatBytes(minimum));
        if (!notes.isEmpty()) html += "<p>" + notes.join("<br>").toHtmlEscaped().replace("&lt;br&gt;", "<br>") + "</p>";
        card->setText(Bidi::isolate(html));

        const QString problem = RemovableTool::labelProblem(fs, labelEdit->text());
        if (problem == "too-long")
            hint->setText(Bidi::isolate(tr("Too long — %1 allows at most %2 characters.").arg(fsName(fs)).arg(RemovableTool::maxLabelLength(fs))));
        else if (problem == "bad-chars")
            hint->setText(Bidi::isolate(tr("FAT32 names can't contain * ? . , ; : / \\ | + = < > [ ] \"")));
        else
            hint->setText(Bidi::isolate(tr("Optional · up to %1 characters").arg(RemovableTool::maxLabelLength(fs))));
        go->setEnabled(ack->isChecked() && problem.isEmpty() && !tooSmall
                       && fsBox->model()->flags(fsBox->model()->index(fsBox->currentIndex(), 0)) & Qt::ItemIsEnabled);
    };
    connect(fsBox, &QComboBox::currentIndexChanged, &dlg, validate);
    connect(labelEdit, &QLineEdit::textChanged, &dlg, validate);
    connect(ack, &QCheckBox::toggled, &dlg, validate);
    connect(go, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    fsBox->setCurrentIndex(0);
    validate();
    if (dlg.exec() != QDialog::Accepted) return;

    const QString fs = fsBox->currentData().toString();
    const QString label = labelEdit->text();
    // Same device as confirmed? (re-checked AFTER the dialog: the user may have
    // swapped sticks while it was open)
    if (!verifyOrExplain(t)) return;

    QStringList toUnmount;
    for (const RemovableTool::Part &p : d.parts)
        if (!p.mountPoint.isEmpty() && (whole || p.device == t.device)) toUnmount << p.device;
    const QString dev = t.device;
    const QString title = whole ? d.title() : dev;

    setBusy(true);
    m_status->setText(Bidi::isolate(tr("Formatting %1 as %2…").arg(title, fsName(fs))));
    LoadingOverlay::on(this)->start(tr("Formatting"),
        tr("%1 as %2 — do not unplug it.").arg(title, fsName(fs)));
    auto *w = new QFutureWatcher<OpOutcome>(this);
    connect(w, &QFutureWatcher<OpOutcome>::finished, this, [this, w, title, fs]{
        const OpOutcome o = w->result();
        w->deleteLater();
        setBusy(false);
        refresh();
        if (!o.result.ok()) {
            m_status->setText(Bidi::isolate(FailureText::brief(o.result)));
            FailureText::show(this, tr("Format"), tr("%1 was not formatted.").arg(title), o.result);
            return;
        }
        const QString msg = tr("%1 was formatted as %2.").arg(title, fsName(fs));
        m_status->setText(Bidi::isolate(msg));
        QMessageBox::information(this, tr("Format"), Bidi::isolate(msg));
    });
    w->setFuture(QtConcurrent::run([toUnmount, dev, whole, fs, label]{
        OpOutcome o;
        for (const QString &p : toUnmount) {
            o.result = RemovableTool::unmount(p);
            if (!o.result.ok()) return o;
        }
        o.result = whole ? RemovableTool::formatDisk(dev, fs, label)
                         : RemovableTool::formatPartition(dev, fs, label);
        return o;
    }));
}
