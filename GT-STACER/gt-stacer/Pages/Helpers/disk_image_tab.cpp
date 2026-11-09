#include "disk_image_tab.h"
#include "../../Widgets/failure_text.h"
#include "../../Widgets/bidi_text.h"
#include "../../Widgets/loading_overlay.h"
#include "../../../gt-stacer-core/Utils/command_util.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Tools/notification_tool.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

constexpr int kIndexRole = Qt::UserRole;        // index into m_images
constexpr int kMountRole = Qt::UserRole + 1;    // a partition's mount point

enum Col { ColName, ColType, ColSize, ColAccess, ColMount, ColCount };

struct MountOutcome {
    CommandResult result;
    QStringList   mounted;
    bool          wasAttached = false;   // the image was already attached before us
};

struct UnmountOutcome {
    CommandResult result;
    QStringList   holders;               // programs keeping it busy (if busy)
};

QString canonical(const QString &p)
{
    const QString c = QFileInfo(p).canonicalFilePath();
    return c.isEmpty() ? QFileInfo(p).absoluteFilePath() : c;
}

bool isAttached(const QString &image)
{
    const QString want = canonical(image);
    for (const DiskImageTool::Attached &a : DiskImageTool::attached())
        if (canonical(a.file) == want) return true;
    return false;
}

// "ubuntu-24.04-desktop-amd64.iso" → "ubuntu-24.04-desktop-amd64";
// "raspios.img.xz" is never extracted, but strip both anyway.
QString baseNameOf(const QString &image)
{
    QString n = QFileInfo(image).fileName();
    static const QRegularExpression ext(
        "\\.(iso|img|raw|bin|udf|dd|image)(\\.(xz|gz|zst|bz2))?$",
        QRegularExpression::CaseInsensitiveOption);
    n.remove(ext);
    return n.isEmpty() ? QStringLiteral("image") : n;
}

// A partition label as a folder name: no '/', no leading dot, not empty.
QString folderNameFor(const DiskImageTool::Part &p, int index)
{
    QString n = p.label.trimmed();
    n.replace('/', '_');
    while (n.startsWith('.')) n.remove(0, 1);
    if (n.isEmpty()) n = QStringLiteral("partition-%1").arg(index + 1);
    return n;
}

bool dirHasEntries(const QString &dir)
{
    return !QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
                                | QDir::System).isEmpty();
}

} // namespace

DiskImageTab::DiskImageTab(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 12, 0, 0);
    root->setSpacing(10);

    m_intro = new QLabel(this);
    m_intro->setObjectName("introText");
    m_intro->setWordWrap(true);
    root->addWidget(m_intro);

    // Optical drive row — hidden on machines without a CD/DVD drive.
    m_discRow = new QFrame(this);
    m_discRow->setObjectName("dashCard");
    auto *discLay = new QHBoxLayout(m_discRow);
    discLay->setContentsMargins(10, 6, 10, 6);
    m_discCombo = new QComboBox(m_discRow);
    m_discCombo->setVisible(false);
    m_discLabel = new QLabel(m_discRow);
    m_discLabel->setWordWrap(true);
    m_discSaveBtn  = new QPushButton(m_discRow);
    m_discOpenBtn  = new QPushButton(m_discRow);
    m_discEjectBtn = new QPushButton(m_discRow);
    discLay->addWidget(m_discCombo);
    discLay->addWidget(m_discLabel, 1);
    discLay->addWidget(m_discSaveBtn);
    discLay->addWidget(m_discOpenBtn);
    discLay->addWidget(m_discEjectBtn);
    m_discBurnBtn = new QPushButton(m_discRow);
    discLay->addWidget(m_discBurnBtn);
    m_discRow->setVisible(false);
    root->addWidget(m_discRow);

    auto *actions = new QHBoxLayout;
    m_mountBtn   = new QPushButton(this);
    m_mountBtn->setObjectName("primaryButton");
    m_extractBtn = new QPushButton(this);
    m_openBtn    = new QPushButton(this);
    m_unmountBtn = new QPushButton(this);
    m_refreshBtn = new QPushButton(this);
    for (QPushButton *b : {m_mountBtn, m_extractBtn, m_openBtn, m_unmountBtn}) actions->addWidget(b);
    actions->addStretch();
    actions->addWidget(m_refreshBtn);
    root->addLayout(actions);

    auto *options = new QHBoxLayout;
    m_readOnly  = new QCheckBox(this);
    m_readOnly->setChecked(true);
    m_overwrite = new QCheckBox(this);
    m_overwrite->setChecked(false);
    options->addWidget(m_readOnly);
    options->addSpacing(16);
    options->addWidget(m_overwrite);
    options->addStretch();
    root->addLayout(options);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(ColCount);
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setAcceptDrops(true);
    m_tree->viewport()->installEventFilter(this);
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(ColName, 260);
    m_tree->setColumnWidth(ColType, 110);
    m_tree->setColumnWidth(ColSize, 90);
    m_tree->setColumnWidth(ColAccess, 100);
    root->addWidget(m_tree, 1);

    auto *progressRow = new QHBoxLayout;
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setVisible(false);
    m_stopBtn = new QPushButton(this);
    m_stopBtn->setObjectName("dangerButton");
    m_stopBtn->setVisible(false);
    progressRow->addWidget(m_progress, 1);
    progressRow->addWidget(m_stopBtn);
    root->addLayout(progressRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);

    connect(m_mountBtn,   &QPushButton::clicked, this, &DiskImageTab::chooseAndMount);
    connect(m_extractBtn, &QPushButton::clicked, this, &DiskImageTab::chooseAndExtract);
    connect(m_openBtn,    &QPushButton::clicked, this, &DiskImageTab::openSelected);
    connect(m_unmountBtn, &QPushButton::clicked, this, &DiskImageTab::unmountSelected);
    connect(m_refreshBtn, &QPushButton::clicked, this, &DiskImageTab::refresh);
    connect(m_stopBtn,    &QPushButton::clicked, this, &DiskImageTab::stopExtraction);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &DiskImageTab::updateButtons);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this]{ openSelected(); });
    connect(m_discSaveBtn,  &QPushButton::clicked, this, &DiskImageTab::saveDisc);
    connect(m_discOpenBtn,  &QPushButton::clicked, this, &DiskImageTab::openDisc);
    connect(m_discEjectBtn, &QPushButton::clicked, this, &DiskImageTab::ejectDisc);
    connect(m_discBurnBtn,  &QPushButton::clicked, this, &DiskImageTab::burnIso);
    connect(m_discCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]{ showDiscs(); });

    // A disc can be inserted or removed at any time: re-check while the tab is shown.
    m_discTimer = new QTimer(this);
    m_discTimer->setInterval(3000);
    connect(m_discTimer, &QTimer::timeout, this, &DiskImageTab::pollDiscs);

    retranslate();
    refresh();
    pollDiscs();
}

DiskImageTab::~DiskImageTab()
{
    // Leaving mid-extraction (app quit): stop the copy, then put back what we
    // mounted for it. The guard would stop it anyway once our pipe closes.
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->closeWriteChannel();
            if (!m_proc->waitForFinished(3000)) { m_proc->kill(); m_proc->waitForFinished(1000); }
        }
    }
    releaseExtractMounts();
    if (m_discCopy && !m_discPart.isEmpty()) QFile::remove(m_discPart);   // never leave a half image
}

void DiskImageTab::retranslate()
{
    m_intro->setText(Bidi::isolate(tr("Open ISO and IMG disk images without a password: mount them to browse "
                        "their files, or extract their contents to a folder. Partitioned images "
                        "(e.g. Raspberry Pi .img) get every partition mounted. You can also drop "
                        "an image file onto the list.")));
    m_mountBtn->setText(tr("Mount image…"));
    m_extractBtn->setText(tr("Extract image…"));
    m_openBtn->setText(tr("Open folder"));
    m_unmountBtn->setText(tr("Unmount"));
    m_refreshBtn->setText(tr("Refresh"));
    m_stopBtn->setText(tr("Stop"));
    m_readOnly->setText(tr("Mount read-only (recommended)"));
    m_readOnly->setToolTip(tr("Read-only protects the image file from any change. Untick it only "
                              "to edit files inside a writable image (e.g. a Raspberry Pi card "
                              "image). ISO images are always read-only."));
    m_overwrite->setText(tr("When extracting, replace files that already exist"));
    m_overwrite->setToolTip(tr("Unticked: files already in the destination are kept and skipped. "
                               "Ticked: only files with the same name are replaced. "
                               "Nothing else in the folder is ever deleted."));
    m_discSaveBtn->setText(tr("Save as ISO…"));
    m_discSaveBtn->setToolTip(tr("Copy the disc in the drive into an .iso image file on this computer."));
    m_discOpenBtn->setText(tr("Open"));
    m_discOpenBtn->setToolTip(tr("Mount the disc (if needed) and open it in the file manager."));
    m_discEjectBtn->setText(tr("Eject"));
    m_discBurnBtn->setText(tr("Burn ISO…"));
    m_discBurnBtn->setToolTip(tr("Write an .iso image onto a blank CD, DVD or Blu-ray (needs a drive "
                                 "that can write discs), then check the disc against the image."));
    showDiscs();
    m_tree->setHeaderLabels({tr("Image / partition"), tr("Filesystem"), tr("Size"),
                             tr("Access"), tr("Mounted at")});
    populate();
}

void DiskImageTab::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && m_intro) retranslate();
    QWidget::changeEvent(event);
}

void DiskImageTab::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Images may have been mounted/unmounted from the file manager meanwhile.
    if (!m_busy) refresh();
    m_discTimer->start();
}

void DiskImageTab::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_discTimer->stop();
}

bool DiskImageTab::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tree->viewport()) {
        auto localFile = [](const QMimeData *md) -> QString {
            if (!md || !md->hasUrls()) return {};
            for (const QUrl &u : md->urls())
                if (u.isLocalFile() && QFileInfo(u.toLocalFile()).isFile()) return u.toLocalFile();
            return {};
        };
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *e = static_cast<QDragMoveEvent *>(event);
            if (!m_busy && !localFile(e->mimeData()).isEmpty()) { e->acceptProposedAction(); return true; }
            e->ignore();
            return true;
        }
        if (event->type() == QEvent::Drop) {
            auto *e = static_cast<QDropEvent *>(event);
            const QString file = localFile(e->mimeData());
            if (!m_busy && !file.isEmpty()) {
                e->acceptProposedAction();
                // Let the drag finish before a dialog/long operation starts.
                QTimer::singleShot(0, this, [this, file]{ mountImage(file); });
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── List ────────────────────────────────────────────────────────────────

void DiskImageTab::refresh()
{
    m_images = DiskImageTool::attached();
    populate();
}

void DiskImageTab::populate()
{
    if (!m_tree) return;
    const QString prevMount = selectedMountPoint();
    const int prevImage = selectedImage();
    const QString prevFile = (prevImage >= 0 && prevImage < m_images.size()) ? m_images[prevImage].file : QString();

    m_tree->clear();
    QTreeWidgetItem *reselect = nullptr;
    for (int i = 0; i < m_images.size(); ++i) {
        const DiskImageTool::Attached &a = m_images[i];
        auto *top = new QTreeWidgetItem(m_tree);
        top->setText(ColName, QFileInfo(a.file).fileName());
        top->setToolTip(ColName, a.file + "\n" + a.loop);
        top->setText(ColSize, FormatUtil::formatBytes(QFileInfo(a.file).size()));
        top->setText(ColAccess, a.readOnly ? tr("Read-only") : tr("Writable"));
        top->setData(ColName, kIndexRole, i);
        if (!prevFile.isEmpty() && a.file == prevFile && prevMount.isEmpty()) reselect = top;

        for (int p = 0; p < a.parts.size(); ++p) {
            const DiskImageTool::Part &part = a.parts[p];
            auto *child = new QTreeWidgetItem(top);
            child->setText(ColName, part.label.isEmpty() ? part.device.mid(part.device.lastIndexOf('/') + 1)
                                                         : part.label);
            child->setToolTip(ColName, part.device);
            child->setText(ColType, part.fsType);
            child->setText(ColSize, FormatUtil::formatBytes(part.size));
            child->setText(ColMount, part.mountPoint.isEmpty() ? tr("(not mounted)") : part.mountPoint);
            child->setToolTip(ColMount, part.mountPoint);
            child->setData(ColName, kIndexRole, i);
            child->setData(ColName, kMountRole, part.mountPoint);
            if (!prevMount.isEmpty() && part.mountPoint == prevMount) reselect = child;
        }
        top->setExpanded(true);
    }
    if (!reselect && m_tree->topLevelItemCount() > 0) reselect = m_tree->topLevelItem(0);
    if (reselect) m_tree->setCurrentItem(reselect);

    if (!m_busy)
        m_status->setText(Bidi::isolate(m_images.isEmpty()
            ? tr("No disk images are mounted. Click “Mount image…” or drop an .iso / .img file on the list.")
            : tr("Mounted disk images: %1").arg(m_images.size())));
    updateButtons();
}

int DiskImageTab::selectedImage() const
{
    if (!m_tree) return -1;
    const QTreeWidgetItem *it = m_tree->currentItem();
    if (!it) return -1;
    bool ok = false;
    const int i = it->data(ColName, kIndexRole).toInt(&ok);
    return ok && i >= 0 && i < m_images.size() ? i : -1;
}

QString DiskImageTab::selectedMountPoint() const
{
    if (!m_tree) return {};
    const QTreeWidgetItem *it = m_tree->currentItem();
    if (!it) return {};
    const QString mp = it->data(ColName, kMountRole).toString();
    if (!mp.isEmpty()) return mp;
    // An image row → its first mounted partition.
    const int i = selectedImage();
    if (i < 0 || it->parent()) return {};
    for (const DiskImageTool::Part &p : m_images[i].parts)
        if (!p.mountPoint.isEmpty()) return p.mountPoint;
    return {};
}

void DiskImageTab::updateButtons()
{
    const bool img = selectedImage() >= 0;
    m_mountBtn->setEnabled(!m_busy);
    m_extractBtn->setEnabled(!m_busy);
    m_refreshBtn->setEnabled(!m_busy);
    m_readOnly->setEnabled(!m_busy);
    m_overwrite->setEnabled(!m_busy);
    m_unmountBtn->setEnabled(!m_busy && img);
    m_openBtn->setEnabled(!m_busy && !selectedMountPoint().isEmpty());
    const int d = currentDrive();
    const bool media = d >= 0 && m_drives[d].media;
    m_discCombo->setEnabled(!m_busy);
    m_discSaveBtn->setEnabled(!m_busy && media);
    m_discOpenBtn->setEnabled(!m_busy && media && !m_drives[d].fsType.isEmpty());
    m_discEjectBtn->setEnabled(!m_busy && d >= 0);
    m_discBurnBtn->setEnabled(!m_busy && d >= 0);   // a read-only drive explains itself on click
}

void DiskImageTab::setBusy(bool busy)
{
    m_busy = busy;
    m_tree->setEnabled(!busy);
    updateButtons();
    // The waiting card is only for the short steps that can't be stopped; a
    // job with progress and a Stop button (copy, burn, extract) keeps the bar.
    LoadingOverlay::on(this)->stop();
}

void DiskImageTab::showWait(const QString &title, const QString &detail)
{
    LoadingOverlay::on(this)->start(title, detail);
}

// ── Explanations ────────────────────────────────────────────────────────

bool DiskImageTab::explainUnsupported(const DiskImageTool::Info &info, const QString &path)
{
    const QString name = QFileInfo(path).fileName();
    QString text;
    switch (info.kind) {
    case DiskImageTool::Kind::Compressed: {
        // Shown as advice only — never executed by GT-STACER.
        QString cmd;
        if      (info.detail == "xz")    cmd = "xz -dk";
        else if (info.detail == "gzip")  cmd = "gunzip -k";
        else if (info.detail == "zstd")  cmd = "zstd -d";
        else if (info.detail == "bzip2") cmd = "bunzip2 -k";
        else                             cmd = "7z x";
        text = tr("“%1” is a compressed file (%2), not a disk image yet.\n\n"
                  "Decompress it first — with your archive manager, or in a terminal:\n"
                  "    %3 \"%1\"\n"
                  "then mount or extract the .img / .iso file it produces.")
                   .arg(name, info.detail, cmd);
        break;
    }
    case DiskImageTool::Kind::VirtualDisk:
        text = tr("“%1” is a virtual-machine disk (%2), not a raw disk image.\n\n"
                  "Open it in its virtual-machine program, or convert it to a raw image first:\n"
                  "    qemu-img convert -O raw \"%1\" \"%1.img\"")
                   .arg(name, info.detail);
        break;
    case DiskImageTool::Kind::Empty:
        text = tr("“%1” is empty (or contains only zeros) — there is nothing to mount.\n\n"
                  "If it was just downloaded, the download may have failed; download it again.")
                   .arg(name);
        break;
    case DiskImageTool::Kind::Unknown:
        text = tr("“%1” is not a disk image this tool recognises.\n\n"
                  "Supported: ISO and UDF images (CD/DVD, installers), partitioned disk images "
                  "(.img of a USB stick or SD card) and filesystem images (ext4, FAT, NTFS, "
                  "squashfs …). The file may also be damaged or incomplete.")
                   .arg(name);
        break;
    default:
        return false;
    }
    QMessageBox::information(this, tr("Disk images"), Bidi::isolate(text));
    m_status->setText(Bidi::isolate(text.section('\n', 0, 0)));
    return true;
}

// ── Mount / unmount / open ──────────────────────────────────────────────

static const char *kImageFilter =
    QT_TRANSLATE_NOOP("DiskImageTab", "Disk images (*.iso *.ISO *.img *.IMG *.raw *.bin *.udf *.dd);;All files (*)");

void DiskImageTab::chooseAndMount()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose a disk image to mount"),
                                                      QDir::homePath(), tr(kImageFilter));
    if (!path.isEmpty()) mountImage(path);
}

void DiskImageTab::mountImage(const QString &path)
{
    if (m_busy) return;
    const QFileInfo fi(path);
    if (!fi.isFile() || !fi.isReadable()) {
        QMessageBox::warning(this, tr("Disk images"),
            Bidi::isolate(tr("“%1” can't be read. Check that the file still exists and that you have "
               "permission to read it.").arg(fi.fileName())));
        return;
    }
    const DiskImageTool::Info info = DiskImageTool::inspect(path);
    if (explainUnsupported(info, path)) return;

    const bool readOnly = m_readOnly->isChecked();
    setBusy(true);
    m_progress->setRange(0, 0);                  // busy indicator
    m_progress->setVisible(true);
    m_status->setText(Bidi::isolate(tr("Mounting %1 (%2)…").arg(fi.fileName(), info.detail)));
    showWait(tr("Mounting the image"), QString("%1 (%2)").arg(fi.fileName(), info.detail));

    auto *watcher = new QFutureWatcher<MountOutcome>(this);
    connect(watcher, &QFutureWatcher<MountOutcome>::finished, this, [this, watcher, path]{
        const MountOutcome o = watcher->result();
        watcher->deleteLater();
        m_progress->setVisible(false);
        m_progress->setRange(0, 100);
        setBusy(false);
        refresh();
        const QString name = QFileInfo(path).fileName();
        if (o.mounted.isEmpty()) {
            m_status->setText(Bidi::isolate(tr("%1 was not mounted: %2").arg(name, FailureText::brief(o.result))));
            FailureText::show(this, tr("Disk images"), tr("%1 was not mounted.").arg(name), o.result);
            return;
        }
        m_status->setText(Bidi::isolate(tr("Mounted %1 at: %2").arg(name, o.mounted.join(QStringLiteral(", ")))));
        // Select the first new mount point so "Open folder" goes straight to it.
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
            if ((*it)->data(ColName, kMountRole).toString() == o.mounted.first()) {
                m_tree->setCurrentItem(*it); break;
            }
        if (!o.result.ok())
            FailureText::show(this, tr("Disk images"),
                tr("%1 was mounted, but one of its partitions could not be.").arg(name), o.result);
    });
    watcher->setFuture(QtConcurrent::run([path, readOnly]{
        MountOutcome o;
        o.wasAttached = isAttached(path);
        o.result = DiskImageTool::mount(path, readOnly, &o.mounted);
        return o;
    }));
}

void DiskImageTab::unmountSelected()
{
    const int i = selectedImage();
    if (m_busy || i < 0) return;
    const DiskImageTool::Attached img = m_images[i];
    const QString name = QFileInfo(img.file).fileName();

    setBusy(true);
    m_progress->setRange(0, 0);
    m_progress->setVisible(true);
    m_status->setText(Bidi::isolate(tr("Unmounting %1…").arg(name)));
    showWait(tr("Unmounting the image"), name);

    auto *watcher = new QFutureWatcher<UnmountOutcome>(this);
    connect(watcher, &QFutureWatcher<UnmountOutcome>::finished, this, [this, watcher, name]{
        const UnmountOutcome o = watcher->result();
        watcher->deleteLater();
        m_progress->setVisible(false);
        m_progress->setRange(0, 100);
        setBusy(false);
        refresh();
        if (o.result.ok()) { m_status->setText(Bidi::isolate(tr("%1 was unmounted.").arg(name))); return; }

        m_status->setText(Bidi::isolate(tr("%1 was not unmounted: %2").arg(name, FailureText::brief(o.result))));
        if (!o.holders.isEmpty()) {
            // Name the culprits instead of a bare "device is busy".
            QMessageBox box(QMessageBox::Warning, tr("Disk images"),
                tr("%1 is still in use, so it was not unmounted.\n\n"
                   "These programs have files or a folder open inside it:\n    %2\n\n"
                   "Close them (or move their window/terminal out of that folder), then click "
                   "Unmount again.").arg(name, o.holders.join(QStringLiteral("\n    "))),
                QMessageBox::Ok, this);
            box.setDetailedText(o.result.output);
            box.exec();
        } else {
            FailureText::show(this, tr("Disk images"), tr("%1 was not unmounted.").arg(name), o.result);
        }
    });
    watcher->setFuture(QtConcurrent::run([img]{
        UnmountOutcome o;
        o.result = DiskImageTool::unmount(img);
        if (!o.result.ok() && CommandFailure::classify(o.result).kind == CommandFailure::Kind::DeviceBusy)
            for (const DiskImageTool::Part &p : img.parts)
                for (const QString &h : DiskImageTool::holders(p.mountPoint))
                    if (!o.holders.contains(h)) o.holders << h;
        return o;
    }));
}

void DiskImageTab::openSelected()
{
    const QString mp = selectedMountPoint();
    if (mp.isEmpty()) return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(mp)))
        m_status->setText(Bidi::isolate(tr("Could not open a file manager. The files are in: %1").arg(mp)));
}

// ── Extraction ──────────────────────────────────────────────────────────

void DiskImageTab::chooseAndExtract()
{
    if (m_busy) return;
    // A mounted image is selected → extract that one; otherwise ask.
    const int i = selectedImage();
    QString path = i >= 0 ? m_images[i].file : QString();
    if (path.isEmpty() || !QFileInfo(path).isFile())
        path = QFileDialog::getOpenFileName(this, tr("Choose a disk image to extract"),
                                            QDir::homePath(), tr(kImageFilter));
    if (!path.isEmpty()) extractImage(path);
}

void DiskImageTab::extractImage(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.isFile() || !fi.isReadable()) {
        QMessageBox::warning(this, tr("Disk images"),
            Bidi::isolate(tr("“%1” can't be read. Check that the file still exists and that you have "
               "permission to read it.").arg(fi.fileName())));
        return;
    }
    const DiskImageTool::Info info = DiskImageTool::inspect(path);
    if (explainUnsupported(info, path)) return;

    // Destination: a NEW folder named after the image, inside the folder the
    // user picks. (GT-IMT extracted straight into the chosen folder and its
    // "overwrite" emptied it first with rm -rf — never again.)
    const QString parent = QFileDialog::getExistingDirectory(this,
        tr("Extract into which folder? (a subfolder named after the image is created)"),
        fi.absolutePath());
    if (parent.isEmpty()) return;
    const QString dest = QDir(parent).filePath(baseNameOf(path));
    const bool overwrite = m_overwrite->isChecked();

    if (QFileInfo(dest).exists() && !QFileInfo(dest).isDir()) {
        QMessageBox::warning(this, tr("Disk images"),
            Bidi::isolate(tr("“%1” already exists and is a file, not a folder. Choose another place.").arg(dest)));
        return;
    }
    if (QFileInfo(dest).isDir() && dirHasEntries(dest)) {
        const QString how = overwrite
            ? tr("Files with the same names will be REPLACED by the ones from the image.")
            : tr("Files that already exist will be KEPT (skipped).");
        if (QMessageBox::question(this, tr("Disk images"),
                Bidi::isolate(tr("The folder “%1” already exists and is not empty.\n\n%2\n"
                   "Nothing else in the folder is deleted.\n\nExtract into it anyway?")
                    .arg(dest, how))) != QMessageBox::Yes)
            return;
    }
    if (!QDir().mkpath(dest) || !QFileInfo(QFileInfo(dest).absoluteFilePath()).isWritable()) {
        QMessageBox::warning(this, tr("Disk images"),
            Bidi::isolate(tr("Can't create or write to “%1”. Choose a folder you own (e.g. in your home "
               "folder).").arg(dest)));
        return;
    }
    // The extracted files are at most about the image's size: warn, don't block.
    const QStorageInfo st(dest);
    if (st.isValid() && st.bytesAvailable() < info.size) {
        if (QMessageBox::question(this, tr("Disk images"),
                Bidi::isolate(tr("The image is %1, but only %2 is free at the destination. Its files may not "
                   "fit.\n\nContinue anyway?")
                    .arg(FormatUtil::formatBytes(info.size), FormatUtil::formatBytes(st.bytesAvailable()))))
                != QMessageBox::Yes)
            return;
    }

    m_extractDest = dest;
    m_extractImage = path;
    m_steps.clear();
    m_stepIdx = 0;
    m_warnings = 0;
    m_stopRequested = false;
    m_mountedForExtract.clear();

    // Optical images: read directly by 7z / bsdtar / xorriso — no mount needed.
    if (info.kind == DiskImageTool::Kind::Iso || info.kind == DiskImageTool::Kind::Udf) {
        const QStringList argv = DiskImageTool::isoExtractArgv(path, dest, overwrite,
                                                               info.kind == DiskImageTool::Kind::Udf);
        if (!argv.isEmpty()) {
            m_steps << Step{fi.fileName(), argv};
            setBusy(true);
            runNextStep();
            return;
        }
        // No extractor installed → mount it and copy the files instead.
    }

    // Disk / filesystem images (or an ISO with no extractor): mount read-only,
    // copy each partition's files, then unmount what we mounted.
    setBusy(true);
    m_progress->setRange(0, 0);
    m_progress->setVisible(true);
    m_status->setText(Bidi::isolate(tr("Mounting %1 to read its files…").arg(fi.fileName())));
    showWait(tr("Opening the image"), tr("%1 — mounting it to read its files…").arg(fi.fileName()));

    auto *watcher = new QFutureWatcher<MountOutcome>(this);
    connect(watcher, &QFutureWatcher<MountOutcome>::finished, this, [this, watcher, path, overwrite]{
        const MountOutcome o = watcher->result();
        watcher->deleteLater();
        const QString name = QFileInfo(path).fileName();
        if (o.mounted.isEmpty()) {
            m_progress->setVisible(false);
            m_progress->setRange(0, 100);
            setBusy(false);
            refresh();
            m_status->setText(Bidi::isolate(tr("%1 could not be read: %2").arg(name, FailureText::brief(o.result))));
            FailureText::show(this, tr("Disk images"), tr("%1 could not be read.").arg(name), o.result);
            return;
        }
        // Remember what we attached so it goes away again afterwards; an image
        // the user had mounted before stays as it was.
        m_images = DiskImageTool::attached();
        populate();
        QVector<DiskImageTool::Part> parts;
        for (const DiskImageTool::Attached &a : m_images)
            if (canonical(a.file) == canonical(path)) {
                if (!o.wasAttached) m_mountedForExtract << a;
                for (const DiskImageTool::Part &p : a.parts)
                    if (!p.mountPoint.isEmpty() && o.mounted.contains(p.mountPoint)) parts << p;
            }
        if (parts.isEmpty()) {          // listed mount points didn't match — use what mount() said
            for (const QString &mp : o.mounted) { DiskImageTool::Part p; p.mountPoint = mp; parts << p; }
        }
        if (!o.result.ok()) ++m_warnings;   // a partition could not be mounted

        for (int k = 0; k < parts.size(); ++k) {
            QString target = m_extractDest;
            if (parts.size() > 1) {
                target = QDir(m_extractDest).filePath(folderNameFor(parts[k], k));
                QDir().mkpath(target);
            }
            const QString label = parts.size() > 1 ? folderNameFor(parts[k], k) : name;
            m_steps << Step{label, DiskImageTool::copyArgv(parts[k].mountPoint, target, overwrite)};
        }
        runNextStep();
    });
    watcher->setFuture(QtConcurrent::run([path]{
        MountOutcome o;
        o.wasAttached = isAttached(path);
        o.result = DiskImageTool::mount(path, /*readOnly=*/true, &o.mounted);
        return o;
    }));
}

void DiskImageTab::runNextStep()
{
    LoadingOverlay::on(this)->stop();        // the copy has its own bar and Stop
    if (m_stepIdx >= m_steps.size()) {
        const QString name = QFileInfo(m_extractImage).fileName();
        finishExtraction(true, m_warnings > 0
            ? tr("%1 was extracted to %2, with warnings: some files could not be copied.")
                  .arg(name, m_extractDest)
            : tr("%1 was extracted to %2.").arg(name, m_extractDest));
        return;
    }
    const Step step = m_steps[m_stepIdx];
    QStringList args = step.argv;
    QString prog = args.takeFirst();
    m_program = prog;
    m_output.clear();
    // Not root, but the guard still matters: it stops the copy on Stop and
    // when GT-STACER quits or is killed, instead of leaving it running.
    CommandUtil::guardForStop(prog, args, /*asRoot=*/false);

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    const bool knowsPercent = m_program.startsWith("7z") || m_program == "rsync";
    m_progress->setRange(0, knowsPercent ? 100 : 0);
    m_progress->setValue(m_stepIdx * 100 / m_steps.size());
    m_progress->setVisible(true);
    m_stopBtn->setVisible(true);
    m_stopBtn->setEnabled(true);
    m_status->setText(Bidi::isolate(m_steps.size() > 1
        ? tr("Extracting %1 (%2 of %3)…").arg(step.label).arg(m_stepIdx + 1).arg(m_steps.size())
        : tr("Extracting %1…").arg(step.label)));

    connect(m_proc, &QProcess::readyRead, this, [this]{
        const QString chunk = QString::fromUtf8(m_proc->readAll());
        m_output = (m_output + chunk).right(20000);
        // 7z (-bsp1) and rsync (--info=progress2) both print "NN%"; take the last.
        static const QRegularExpression pct("(\\d{1,3})%");
        int last = -1;
        for (auto it = pct.globalMatch(chunk); it.hasNext();) last = it.next().captured(1).toInt();
        if (last >= 0 && last <= 100 && m_progress->maximum() == 100)
            m_progress->setValue((m_stepIdx * 100 + last) / qMax(1, int(m_steps.size())));
    });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status){
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_stopRequested) {
            finishExtraction(false, tr("Extraction stopped. The files copied so far are in %1.")
                                        .arg(m_extractDest));
            return;
        }
        // Partial success is not failure: 7z 1 = warnings; rsync 23/24 =
        // some files couldn't be read / vanished.
        const bool warning = (m_program.startsWith("7z") && code == 1)
                          || (m_program == "rsync" && (code == 23 || code == 24));
        if (status == QProcess::NormalExit && (code == 0 || warning)) {
            if (warning) ++m_warnings;
            ++m_stepIdx;
            runNextStep();
            return;
        }
        CommandResult r;
        r.program = m_program; r.started = true;
        r.exitCode = status == QProcess::NormalExit ? code : -1;
        r.output = m_output;
        const QString name = QFileInfo(m_extractImage).fileName();
        finishExtraction(false, tr("%1 was not fully extracted: %2").arg(name, FailureText::brief(r)));
        FailureText::show(this, tr("Disk images"),
                          tr("%1 was not fully extracted. The files copied so far are in %2.")
                              .arg(name, m_extractDest), r);
    });
    m_proc->start(prog, args);
}

void DiskImageTab::stopExtraction()
{
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
    if (m_burn == Burn::Writing
        && QMessageBox::warning(this, tr("Disk images"),
               Bidi::isolate(tr("Stop burning now?\n\nA CD-R, DVD-R or BD-R that is stopped mid-burn "
                  "can't be used again. A rewritable disc (-RW / -RE) can be erased and burnt again.")),
               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;   // ended while we asked
    m_stopRequested = true;
    m_stopBtn->setEnabled(false);
    m_status->setText(Bidi::isolate(tr("Stopping…")));
    m_proc->closeWriteChannel();        // the guard SIGTERMs the copy

    // Not root, so if it ignores SIGTERM we can end it ourselves.
    QTimer::singleShot(10000, this, [this]{
        if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
        if (!CommandUtil::inFlatpak())
            CommandUtil::execProgram("pkill", {"-KILL", "-P", QString::number(m_proc->processId())}, 5000);
        m_proc->kill();
    });
}

void DiskImageTab::releaseExtractMounts()
{
    for (const DiskImageTool::Attached &a : m_mountedForExtract) DiskImageTool::unmount(a);
    m_mountedForExtract.clear();
}

void DiskImageTab::finishExtraction(bool ok, const QString &message)
{
    // Put the system back as it was: unmount the image if WE mounted it.
    // (Quick — udisks unmounts a read-only mount nobody else uses at once.)
    QStringList stillMounted;
    for (const DiskImageTool::Attached &a : m_mountedForExtract)
        if (!DiskImageTool::unmount(a).ok()) stillMounted << QFileInfo(a.file).fileName();
    m_mountedForExtract.clear();

    m_steps.clear();
    m_progress->setVisible(false);
    m_progress->setRange(0, 100);
    m_stopBtn->setVisible(false);
    setBusy(false);
    refresh();

    QString msg = message;
    if (!stillMounted.isEmpty())
        msg += " " + tr("(%1 is still mounted — unmount it from the list when you're done.)")
                         .arg(stillMounted.join(", "));
    m_status->setText(Bidi::isolate(msg));
    if (ok && !m_extractDest.isEmpty()) {
        // Offer the result folder directly.
        if (QMessageBox::question(this, tr("Disk images"), Bidi::isolate(msg + "\n\n" + tr("Open the folder now?")))
                == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_extractDest));
    }
    NotificationTool::notify(tr("Disk images"), msg, NotificationTool::Urgency::Normal, "gt-stacer");
}

// ── Optical discs ───────────────────────────────────────────────────────

void DiskImageTab::pollDiscs()
{
    if (m_discPolling || m_busy) return;
    m_discPolling = true;
    auto *watcher = new QFutureWatcher<QVector<DiskImageTool::OpticalDrive>>(this);
    connect(watcher, &QFutureWatcher<QVector<DiskImageTool::OpticalDrive>>::finished, this, [this, watcher]{
        m_discPolling = false;
        const auto drives = watcher->result();
        watcher->deleteLater();
        if (m_busy) return;                       // a copy started meanwhile — keep its view
        m_drives = drives;
        showDiscs();
    });
    watcher->setFuture(QtConcurrent::run([]{ return DiskImageTool::opticalDrives(); }));
}

int DiskImageTab::currentDrive() const
{
    if (m_drives.isEmpty()) return -1;
    const int i = m_drives.size() > 1 ? m_discCombo->currentIndex() : 0;
    return i >= 0 && i < m_drives.size() ? i : 0;
}

namespace {
QString discTitle(const DiskImageTool::OpticalDrive &d)
{
    const QString kind = d.mediaType.isEmpty() ? QStringLiteral("CD/DVD") : d.mediaType;
    return d.label.isEmpty() ? kind : QStringLiteral("%1 “%2”").arg(kind, d.label);
}
} // namespace

void DiskImageTab::showDiscs()
{
    if (!m_discRow) return;
    m_discRow->setVisible(!m_drives.isEmpty());
    // The combo lists the drives only when there is a choice to make.
    const QString prev = m_discCombo->currentData().toString();
    {
        const QSignalBlocker block(m_discCombo);
        m_discCombo->clear();
        for (const auto &d : m_drives)
            m_discCombo->addItem(d.model.isEmpty() ? d.device : d.model, d.device);
        const int keep = m_discCombo->findData(prev);
        if (keep >= 0) m_discCombo->setCurrentIndex(keep);
    }
    m_discCombo->setVisible(m_drives.size() > 1);

    const int i = currentDrive();
    if (i < 0) { updateButtons(); return; }
    const DiskImageTool::OpticalDrive &d = m_drives[i];
    QString text;
    if (!d.media)            text = tr("Disc drive: no disc inserted.");
    else if (d.blank)        text = tr("Disc drive: a blank %1 (nothing to copy).").arg(discTitle(d));
    else if (d.audioOnly())  text = tr("Disc drive: an audio CD (%n track(s)).", nullptr, d.audioTracks);
    else                     text = tr("Disc drive: %1 · %2").arg(discTitle(d), FormatUtil::formatBytes(d.size));
    if (d.media && !d.mountPoint.isEmpty()) text += " · " + tr("mounted at %1").arg(d.mountPoint);
    m_discLabel->setText(Bidi::isolate(text));
    m_discLabel->setToolTip(d.device + (d.model.isEmpty() ? QString() : "  —  " + d.model));
    updateButtons();
}

bool DiskImageTab::explainDisc(const DiskImageTool::OpticalDrive &d)
{
    QString text;
    if (!d.media)
        text = tr("There is no disc in the drive (%1).\n\nInsert a CD, DVD or Blu-ray, wait a few "
                  "seconds until the drive stops spinning up, then try again.").arg(d.device);
    else if (d.blank)
        text = tr("The disc in the drive is blank — it has no data to save as an image.");
    else if (d.audioOnly())
        text = tr("This is an audio CD. Audio CDs have no filesystem, so they can't be saved as an "
                  ".iso image (an ISO file would be empty or unplayable).\n\n"
                  "To copy the music, use a CD ripper such as Sound Juicer, Asunder or K3b, or in a "
                  "terminal:\n    cdparanoia -B");
    else if (!d.readable)
        text = tr("You don't have permission to read the disc drive %1.\n\n"
                  "Normally the user logged in at the computer may read it. If you are logged in "
                  "remotely, or your system doesn't grant this, add yourself to the “cdrom” group "
                  "and log out and back in:\n    sudo usermod -aG cdrom $USER").arg(d.device);
    else if (d.size <= 0)
        text = tr("The drive did not report the size of the disc, so it can't be copied. The disc "
                  "may still be loading, or it may be unreadable — wait a moment and try again.");
    else
        return false;
    QMessageBox::information(this, tr("Disk images"), Bidi::isolate(text));
    m_status->setText(Bidi::isolate(text.section('\n', 0, 0)));
    return true;
}

void DiskImageTab::saveDisc()
{
    const int i = currentDrive();
    if (m_busy || i < 0) return;
    const DiskImageTool::OpticalDrive d = m_drives[i];
    if (explainDisc(d)) return;

    // Reading the volume descriptor may have to spin the disc up — off the UI thread.
    setBusy(true);
    m_progress->setRange(0, 0);
    m_progress->setVisible(true);
    m_status->setText(Bidi::isolate(tr("Reading the disc…")));
    showWait(tr("Reading the disc"), tr("The drive may need a few seconds to spin up."));
    auto *watcher = new QFutureWatcher<qint64>(this);
    connect(watcher, &QFutureWatcher<qint64>::finished, this, [this, watcher, d]{
        const qint64 bytes = watcher->result();
        watcher->deleteLater();
        m_progress->setVisible(false);
        m_progress->setRange(0, 100);
        setBusy(false);
        m_status->clear();

        QString note;
        if (d.audioTracks > 0)
            note = tr("This disc also has %n audio track(s); an ISO image holds only the data "
                      "part, not the music.", nullptr, d.audioTracks) + "\n\n";
        if (!note.isEmpty()
            && QMessageBox::question(this, tr("Disk images"),
                   Bidi::isolate(note + tr("Save the data part as an image anyway?"))) != QMessageBox::Yes)
            return;

        QString base = d.label.trimmed();
        base.replace('/', '_');
        while (base.startsWith('.')) base.remove(0, 1);
        if (base.isEmpty()) base = QStringLiteral("disc");
        QString dest = QFileDialog::getSaveFileName(this, tr("Save the disc as an image"),
                           QDir(QDir::homePath()).filePath(base + ".iso"), tr("ISO images (*.iso)"));
        if (dest.isEmpty()) return;
        if (!dest.endsWith(".iso", Qt::CaseInsensitive)) dest += ".iso";
        startDiscCopy(d, bytes, dest);
    });
    watcher->setFuture(QtConcurrent::run([d]{ return DiskImageTool::opticalImageSize(d); }));
}

void DiskImageTab::startDiscCopy(const DiskImageTool::OpticalDrive &d, qint64 bytes, const QString &dest)
{
    const QFileInfo fi(dest);
    const QString dir = fi.absolutePath();
    if (!QFileInfo(dir).isDir() || !QFileInfo(dir).isWritable()) {
        QMessageBox::warning(this, tr("Disk images"),
            Bidi::isolate(tr("Can't write to “%1”. Choose a folder you own (e.g. in your home folder).").arg(dir)));
        return;
    }
    // The image is exactly `bytes` long: unlike extraction, a shortfall is certain failure.
    const QStorageInfo st(dir);
    const qint64 reuse = fi.exists() ? fi.size() : 0;            // an old file being replaced
    if (st.isValid() && st.bytesAvailable() + reuse < bytes) {
        QMessageBox::warning(this, tr("Disk images"),
            Bidi::isolate(tr("The disc image needs %1, but only %2 is free in “%3”.\n\n"
               "Free some space or choose another drive.")
                .arg(FormatUtil::formatBytes(bytes), FormatUtil::formatBytes(st.bytesAvailable()), dir)));
        return;
    }
    const QString fs = QString::fromLatin1(st.fileSystemType()).toLower();
    if ((fs == "vfat" || fs == "msdos" || fs == "fat") && bytes > 4294967295LL) {
        QMessageBox::warning(this, tr("Disk images"),
            Bidi::isolate(tr("“%1” is on a FAT32 drive, which can't hold a file larger than 4 GB — and "
               "this image is %2.\n\nSave it on a drive formatted as exFAT, NTFS or ext4 (e.g. your "
               "home folder).").arg(dir, FormatUtil::formatBytes(bytes))));
        return;
    }

    m_discCopy  = true;
    m_discDest  = fi.absoluteFilePath();
    m_discPart  = m_discDest + ".part";
    m_discBytes = bytes;
    m_discName  = discTitle(d);
    m_stopRequested = false;
    QFile::remove(m_discPart);                 // a leftover of an earlier, killed run

    QStringList args = DiskImageTool::discCopyArgv(d.device, m_discPart, bytes);
    QString prog = args.takeFirst();
    m_program = prog;
    m_output.clear();
    CommandUtil::guardForStop(prog, args, /*asRoot=*/false);   // stops with Stop / when we die

    setBusy(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(true);
    m_stopBtn->setVisible(true);
    m_stopBtn->setEnabled(true);
    m_status->setText(Bidi::isolate(tr("Saving %1 to %2…").arg(m_discName, m_discDest)));

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, [this]{
        const QString chunk = QString::fromUtf8(m_proc->readAll());
        m_output = (m_output + chunk).right(20000);
        // dd status=progress: "123456789 bytes (123 MB, 118 MiB) copied, …"
        static const QRegularExpression done("(\\d+) bytes");
        qint64 last = -1;
        for (auto it = done.globalMatch(chunk); it.hasNext();) last = it.next().captured(1).toLongLong();
        if (last >= 0 && m_discBytes > 0)
            m_progress->setValue(int(qMin<qint64>(100, last * 100 / m_discBytes)));
    });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status){
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_stopRequested) {
            QFile::remove(m_discPart);
            finishDiscCopy(false, tr("Saving the disc was stopped. The incomplete image was deleted."));
            return;
        }
        const qint64 got = QFileInfo(m_discPart).size();
        if (status == QProcess::NormalExit && code == 0 && got == m_discBytes) {
            QFile::remove(m_discDest);         // the user confirmed replacing it in the save dialog
            if (!QFile::rename(m_discPart, m_discDest)) {
                finishDiscCopy(false, tr("The disc was copied, but the file could not be renamed. "
                                         "It is saved as %1.").arg(m_discPart));
                m_discPart.clear();            // keep it: it is complete
                return;
            }
            const DiskImageTool::Info info = DiskImageTool::inspect(m_discDest);
            QString msg = tr("%1 was saved as %2 (%3).").arg(m_discName, m_discDest,
                                                           FormatUtil::formatBytes(got));
            if (!info.mountable())
                msg += " " + tr("Note: no filesystem was recognised in the image — the disc may "
                                "use a format this tool can't open, but the copy is complete.");
            finishDiscCopy(true, msg);
            return;
        }

        // Failed: explain, and never leave a half image that looks like a real one.
        QFile::remove(m_discPart);
        CommandResult r;
        r.program = m_program; r.started = true;
        r.exitCode = status == QProcess::NormalExit ? code : -1;
        r.output = m_output;
        QString why;
        if (m_output.contains("No medium found", Qt::CaseInsensitive))
            why = tr("The disc was removed (or the drive opened) during the copy.");
        else if (m_output.contains("Input/output error", Qt::CaseInsensitive))
            why = tr("The drive could not read part of the disc. It may be scratched, dirty or "
                     "damaged — clean it with a soft cloth (from the centre outwards) and try again. "
                     "Commercial video DVDs and Blu-rays are copy-protected and can't be copied "
                     "sector by sector; GT-STACER does not bypass copy protection.\n\n"
                     "For a damaged data disc, GNU ddrescue can recover what is still readable:\n"
                     "    ddrescue -b 2048 %1 disc.iso disc.map").arg(m_drives.value(currentDrive()).device);
        else if (m_output.contains("No space left", Qt::CaseInsensitive))
            why = tr("The destination drive became full during the copy.");
        else if (m_output.contains("File too large", Qt::CaseInsensitive))
            why = tr("The destination filesystem can't hold a file this large (FAT32: 4 GB at most).");
        finishDiscCopy(false, tr("%1 was not saved: %2").arg(m_discName,
                                 why.isEmpty() ? FailureText::brief(r) : why.section('\n', 0, 0)));
        if (why.isEmpty())
            FailureText::show(this, tr("Disk images"), tr("%1 was not saved. The incomplete image was deleted.")
                                                           .arg(m_discName), r);
        else {
            QMessageBox box(QMessageBox::Warning, tr("Disk images"),
                Bidi::isolate(tr("%1 was not saved. The incomplete image was deleted.").arg(m_discName)
                              + "\n\n" + why), QMessageBox::Ok, this);
            box.setDetailedText(m_output.right(4000));
            box.exec();
        }
    });
    m_proc->start(prog, args);
}

void DiskImageTab::finishDiscCopy(bool ok, const QString &message)
{
    m_discCopy = false;
    m_progress->setVisible(false);
    m_stopBtn->setVisible(false);
    setBusy(false);
    m_status->setText(Bidi::isolate(message));
    NotificationTool::notify(tr("Disk images"), message, NotificationTool::Urgency::Normal, "gt-stacer");
    if (ok) {
        QMessageBox box(QMessageBox::Information, tr("Disk images"), Bidi::isolate(message), QMessageBox::NoButton, this);
        QPushButton *mountB = box.addButton(tr("Mount it"), QMessageBox::AcceptRole);
        QPushButton *folderB = box.addButton(tr("Open folder"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Close);
        box.exec();
        if (box.clickedButton() == mountB) mountImage(m_discDest);
        else if (box.clickedButton() == folderB)
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_discDest).absolutePath()));
    }
    pollDiscs();
}

void DiskImageTab::openDisc()
{
    const int i = currentDrive();
    if (m_busy || i < 0) return;
    const DiskImageTool::OpticalDrive d = m_drives[i];
    if (explainDisc(d)) return;
    if (!d.mountPoint.isEmpty()) { QDesktopServices::openUrl(QUrl::fromLocalFile(d.mountPoint)); return; }

    setBusy(true);
    m_progress->setRange(0, 0);
    m_progress->setVisible(true);
    m_status->setText(Bidi::isolate(tr("Mounting %1…").arg(discTitle(d))));
    showWait(tr("Opening the disc"), discTitle(d));
    auto *watcher = new QFutureWatcher<CommandResult>(this);
    connect(watcher, &QFutureWatcher<CommandResult>::finished, this, [this, watcher, d]{
        const CommandResult r = watcher->result();
        watcher->deleteLater();
        m_progress->setVisible(false);
        m_progress->setRange(0, 100);
        setBusy(false);
        const auto m = QRegularExpression("Mounted \\S+ at (.+?)\\.?\\s*$", QRegularExpression::MultilineOption)
                           .match(r.output);
        if (r.ok() && m.hasMatch()) {
            const QString mp = m.captured(1).trimmed();
            m_status->setText(Bidi::isolate(tr("Mounted %1 at: %2").arg(discTitle(d), mp)));
            QDesktopServices::openUrl(QUrl::fromLocalFile(mp));
        } else {
            m_status->setText(Bidi::isolate(tr("%1 was not mounted: %2").arg(discTitle(d), FailureText::brief(r))));
            FailureText::show(this, tr("Disk images"), tr("%1 was not mounted.").arg(discTitle(d)), r);
        }
        pollDiscs();
    });
    const QString dev = d.device;
    watcher->setFuture(QtConcurrent::run([dev]{
        return CommandUtil::runCommand("udisksctl", {"mount", "-b", dev}, 60000);
    }));
}

void DiskImageTab::ejectDisc()
{
    const int i = currentDrive();
    if (m_busy || i < 0) return;
    const DiskImageTool::OpticalDrive d = m_drives[i];
    setBusy(true);
    m_status->setText(Bidi::isolate(tr("Ejecting…")));
    showWait(tr("Ejecting the disc"), discTitle(d));
    struct Ejected { CommandResult result; QStringList holders; };
    auto *watcher = new QFutureWatcher<Ejected>(this);
    connect(watcher, &QFutureWatcher<Ejected>::finished, this, [this, watcher, d]{
        const Ejected e = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        if (e.result.ok()) { m_status->setText(Bidi::isolate(tr("The disc was ejected."))); pollDiscs(); return; }
        m_status->setText(Bidi::isolate(tr("The disc was not ejected: %1").arg(FailureText::brief(e.result))));
        if (!e.holders.isEmpty())
            QMessageBox::warning(this, tr("Disk images"),
                Bidi::isolate(tr("The disc is still in use, so it was not ejected.\n\n"
                   "These programs have files or a folder open on it:\n    %1\n\n"
                   "Close them, then click Eject again.").arg(e.holders.join(QStringLiteral("\n    ")))));
        else
            FailureText::show(this, tr("Disk images"), tr("The disc was not ejected."), e.result);
        pollDiscs();
    });
    watcher->setFuture(QtConcurrent::run([d]{
        Ejected e;
        // Unmount through udisks first: `eject` alone can't unmount a udisks mount as a user.
        if (!d.mountPoint.isEmpty()) {
            e.result = CommandUtil::runCommand("udisksctl", {"unmount", "-b", d.device}, 30000);
            if (!e.result.ok()) {
                if (CommandFailure::classify(e.result).kind == CommandFailure::Kind::DeviceBusy)
                    e.holders = DiskImageTool::holders(d.mountPoint);
                return e;
            }
        }
        e.result = CommandUtil::runCommand("eject", {d.device}, 30000);
        return e;
    }));
}

// ── Burning ─────────────────────────────────────────────────────────────

void DiskImageTab::burnIso()
{
    const int i = currentDrive();
    if (m_busy || i < 0) return;
    const DiskImageTool::OpticalDrive d = m_drives[i];
    auto inform = [this](const QString &text) {
        QMessageBox::information(this, tr("Disk images"), Bidi::isolate(text));
        m_status->setText(Bidi::isolate(text.section('\n', 0, 0)));
    };
    if (!d.canWrite) {
        inform(tr("The drive %1 can only read discs — it can't burn them.\n\n"
                  "Burning needs a drive that writes discs (its name usually contains “RW”, "
                  "“Writer” or “Burner”, e.g. DVD±RW). An external USB writer works too.")
                   .arg(d.model.isEmpty() ? d.device : d.model));
        return;
    }
    if (!CommandUtil::commandExists("xorriso")) {
        inform(tr("Burning needs xorriso (the libburn disc-burning tool), which isn't installed.\n\n"
                  "Install it with your package manager, e.g.:\n"
                  "    sudo apt install xorriso\n    sudo dnf install xorriso\n    sudo pacman -S libisoburn"));
        return;
    }
    if (!d.media) {
        inform(tr("There is no disc in the drive (%1).\n\nInsert a blank CD, DVD or Blu-ray (or a "
                  "rewritable one you may erase), wait a few seconds, then try again.").arg(d.device));
        return;
    }

    const QString image = QFileDialog::getOpenFileName(this, tr("Choose the ISO image to burn"),
                              QDir::homePath(), tr("ISO images (*.iso *.ISO);;All files (*)"));
    if (image.isEmpty()) return;
    const DiskImageTool::Info info = DiskImageTool::inspect(image);
    if (explainUnsupported(info, image)) return;
    if (info.kind != DiskImageTool::Kind::Iso && info.kind != DiskImageTool::Kind::Udf) {
        inform(tr("“%1” is not a CD/DVD image (it is a %2 image). Only ISO / UDF images are burnt "
                  "onto discs.\n\nA disk image for a USB stick or SD card (.img) is written with the "
                  "“USB drives” tab instead.").arg(QFileInfo(image).fileName(), info.detail));
        return;
    }
    const qint64 bytes = info.size;

    // Ask the drive about the disc (and release it if it is mounted) off the UI thread.
    setBusy(true);
    m_progress->setRange(0, 0);
    m_progress->setVisible(true);
    m_status->setText(Bidi::isolate(tr("Checking the disc in the drive…")));
    showWait(tr("Checking the disc"), tr("Is it blank, and is there room for the image?"));
    struct Probe { DiskImageTool::BurnMedia media; CommandResult unmount; bool writable = true; };
    auto *watcher = new QFutureWatcher<Probe>(this);
    connect(watcher, &QFutureWatcher<Probe>::finished, this, [this, watcher, d, image, bytes, inform]{
        const Probe p = watcher->result();
        watcher->deleteLater();
        m_progress->setVisible(false);
        m_progress->setRange(0, 100);
        setBusy(false);
        m_status->clear();
        const QString name = QFileInfo(image).fileName();

        if (!p.unmount.ok()) {
            FailureText::show(this, tr("Disk images"),
                tr("The disc is mounted and could not be unmounted, so it can't be burnt."), p.unmount);
            return;
        }
        if (!p.writable) {
            inform(tr("You don't have permission to write to the disc drive %1.\n\n"
                      "Add yourself to the “cdrom” group, then log out and back in:\n"
                      "    sudo usermod -aG cdrom $USER").arg(d.device));
            return;
        }
        const DiskImageTool::BurnMedia &m = p.media;
        if (!m.ok) {
            QMessageBox box(QMessageBox::Warning, tr("Disk images"),
                Bidi::isolate(tr("The drive did not report the disc, so nothing was burnt. The disc may "
                   "still be loading, be unreadable, or another program (a burning or media app) may be "
                   "using the drive. Wait a moment, close such programs, and try again.")),
                QMessageBox::Ok, this);
            box.setDetailedText(m.output.right(4000));
            box.exec();
            return;
        }
        const bool rewritable = d.rewritable || m.media.contains(QRegularExpression("RW|-RE|RAM"));
        if (!m.blank && !rewritable) {
            inform(m.closed
                ? tr("The disc in the drive (%1) is already written and closed — it can't take an "
                     "image.\n\nInsert a blank disc, or a rewritable one (-RW / -RE) that may be erased.")
                      .arg(m.media)
                : tr("The disc in the drive (%1) already holds data. An ISO image has to be burnt onto "
                     "a blank disc — insert a blank one.").arg(m.media));
            return;
        }
        if (m.blank && m.freeBytes >= 0 && bytes > m.freeBytes) {
            inform(tr("“%1” is %2, but the disc holds only %3.\n\nUse a larger disc (a DVD holds "
                      "4.7 GB, a dual-layer DVD 8.5 GB, a Blu-ray 25 GB).")
                       .arg(name, FormatUtil::formatBytes(bytes), FormatUtil::formatBytes(m.freeBytes)));
            return;
        }

        const bool blankFirst = !m.blank;          // a written rewritable disc
        QString text = tr("Burn “%1” (%2) onto the %3 in the drive?").arg(name, FormatUtil::formatBytes(bytes), m.media);
        if (blankFirst)
            text += "\n\n" + tr("The disc is not empty: EVERYTHING on it will be ERASED first.");
        text += "\n\n" + tr("While burning, don't open the drive or put the computer to sleep. "
                            "Stopping in the middle makes a write-once disc (CD-R, DVD-R, BD-R) unusable.");
        QMessageBox box(QMessageBox::Question, tr("Disk images"), Bidi::isolate(text),
                        QMessageBox::Yes | QMessageBox::No, this);
        box.setDefaultButton(QMessageBox::No);
        box.button(QMessageBox::Yes)->setText(blankFirst ? tr("Erase and burn") : tr("Burn"));
        auto *verify = new QCheckBox(tr("Check the disc against the image afterwards (recommended)"), &box);
        verify->setChecked(true);
        box.setCheckBox(verify);
        if (box.exec() != QMessageBox::Yes) return;
        startBurn(d.device, image, bytes, blankFirst, verify->isChecked());
    });
    const QString dev = d.device, mp = d.mountPoint;
    watcher->setFuture(QtConcurrent::run([dev, mp]{
        Probe p;
        p.unmount.started = true; p.unmount.exitCode = 0;
        if (!mp.isEmpty()) {
            p.unmount = CommandUtil::runCommand("udisksctl", {"unmount", "-b", dev}, 30000);
            if (!p.unmount.ok()) return p;
        }
        p.writable = CommandUtil::execProgram("test", {"-w", dev}, 5000) == 0;
        if (p.writable) p.media = DiskImageTool::burnMedia(dev);
        return p;
    }));
}

void DiskImageTab::startBurn(const QString &device, const QString &image, qint64 bytes,
                             bool blankFirst, bool verify)
{
    m_burn = Burn::Writing;
    m_burnDevice = device;
    m_burnImage = QFileInfo(image).absoluteFilePath();
    m_burnBytes = bytes;
    m_burnVerify = verify;
    m_stopRequested = false;

    QStringList args = DiskImageTool::burnArgv(device, m_burnImage, blankFirst);
    QString prog = args.takeFirst();
    m_program = prog;
    m_output.clear();
    CommandUtil::guardForStop(prog, args, /*asRoot=*/false);

    setBusy(true);
    m_progress->setRange(0, blankFirst ? 0 : 100);
    m_progress->setValue(0);
    m_progress->setVisible(true);
    m_stopBtn->setVisible(true);
    m_stopBtn->setEnabled(true);
    m_status->setText(Bidi::isolate(blankFirst ? tr("Erasing the disc…")
                                               : tr("Burning %1…").arg(QFileInfo(image).fileName())));

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, [this]{
        const QString chunk = QString::fromUtf8(m_proc->readAll());
        m_output = (m_output + chunk).right(20000);
        // "xorriso : UPDATE :   7 of  60 MB written (fifo 0%) [buf 50%]"
        static const QRegularExpression written("(\\d+)\\s+of\\s+(\\d+)\\s+MB written");
        int pct = -1;
        for (auto it = written.globalMatch(chunk); it.hasNext();) {
            const auto m = it.next();
            const int total = m.captured(2).toInt();
            if (total > 0) pct = qMin(100, m.captured(1).toInt() * 100 / total);
        }
        if (pct >= 0) {
            if (m_progress->maximum() == 0) {          // erasing is over, writing has begun
                m_progress->setRange(0, 100);
                m_status->setText(Bidi::isolate(tr("Burning %1…").arg(QFileInfo(m_burnImage).fileName())));
            }
            m_progress->setValue(pct);
        }
    });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status){
        m_proc->deleteLater();
        m_proc = nullptr;
        const QString name = QFileInfo(m_burnImage).fileName();
        if (m_stopRequested) {
            finishBurn(false, tr("Burning was stopped. A write-once disc is probably unusable now; "
                                 "a rewritable disc can be erased and burnt again."));
            return;
        }
        if (status == QProcess::NormalExit && code == 0) {
            if (m_burnVerify) startVerify();
            else finishBurn(true, tr("%1 was burnt onto the disc.").arg(name));
            return;
        }
        // xorriso's own words decide the explanation.
        const QString out = m_output;
        QString why;
        if (out.contains(QRegularExpression("no (medium|media)|No disc", QRegularExpression::CaseInsensitiveOption)))
            why = tr("The disc was removed (or the drive opened) during burning.");
        else if (out.contains(QRegularExpression("busy|Cannot acquire drive|exclusive", QRegularExpression::CaseInsensitiveOption)))
            why = tr("Another program is using the drive (a burning or media program, or a file manager "
                     "showing the disc). Close it and try again.");
        else if (out.contains(QRegularExpression("exceed|too (large|big)|not enough|insufficient",
                                                 QRegularExpression::CaseInsensitiveOption)))
            why = tr("The image is larger than the free space on the disc. Use a larger disc.");
        else if (out.contains(QRegularExpression("Permission denied|not permitted", QRegularExpression::CaseInsensitiveOption)))
            why = tr("No permission to write to the drive. Add yourself to the “cdrom” group "
                     "(sudo usermod -aG cdrom $USER), then log out and back in.");
        else if (out.contains(QRegularExpression("unsuitable|not writ|closed", QRegularExpression::CaseInsensitiveOption)))
            why = tr("The drive refused this disc for writing (it is closed, of a type the drive "
                     "can't write, or not recognised). Try a different blank disc.");
        else if (out.contains(QRegularExpression("SCSI error|write error|burn_|Failed to write",
                                                 QRegularExpression::CaseInsensitiveOption)))
            why = tr("The drive reported an error while writing. The disc may be faulty or of poor "
                     "quality — try another disc, preferably from a different brand. A write-once disc "
                     "that failed mid-burn can't be used again.");
        else
            why = tr("xorriso stopped with code %1 — see the details below.").arg(code);
        finishBurn(false, tr("%1 was not burnt: %2").arg(name, why), out);
    });
    m_proc->start(prog, args);
}

void DiskImageTab::startVerify()
{
    m_burn = Burn::Verifying;
    m_stopRequested = false;
    // Give the drive and udev a moment to report the newly written disc.
    CommandUtil::execProgram("udevadm", {"settle", "-t", "3"}, 5000);

    QStringList args = DiskImageTool::verifyArgv(m_burnImage, m_burnDevice, m_burnBytes);
    QString prog = args.takeFirst();
    m_program = prog;
    m_output.clear();
    CommandUtil::guardForStop(prog, args, /*asRoot=*/false);
    m_progress->setRange(0, 0);
    m_stopBtn->setEnabled(true);
    m_status->setText(Bidi::isolate(tr("Checking the disc against the image…")));

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, [this]{
        m_output = (m_output + QString::fromUtf8(m_proc->readAll())).right(20000);
    });
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status){
        m_proc->deleteLater();
        m_proc = nullptr;
        const QString name = QFileInfo(m_burnImage).fileName();
        const QString manual = QStringLiteral("cmp -n %1 \"%2\" %3").arg(m_burnBytes).arg(m_burnImage, m_burnDevice);
        if (m_stopRequested) {
            finishBurn(true, tr("%1 was burnt; the check was stopped. To check later:\n    %2").arg(name, manual));
            return;
        }
        if (status == QProcess::NormalExit && code == 0) {
            finishBurn(true, tr("%1 was burnt and checked: the disc is identical to the image.").arg(name));
            return;
        }
        if (status == QProcess::NormalExit && code == 1 && m_output.contains("differ")) {
            finishBurn(false, tr("%1 was burnt, but the disc does NOT match the image — the disc is "
                                 "faulty. Burn it again on another disc.").arg(name), m_output);
            return;
        }
        // EOF on the disc / read error: the drive hasn't re-read the new disc yet.
        finishBurn(true, tr("%1 was burnt, but the disc could not be read back to check it (the drive "
                            "may need the disc re-inserted). Eject it, insert it again, then check it "
                            "in a terminal:\n    %2").arg(name, manual), m_output);
    });
    m_proc->start(prog, args);
}

void DiskImageTab::finishBurn(bool ok, const QString &message, const QString &details)
{
    m_burn = Burn::None;
    m_progress->setVisible(false);
    m_progress->setRange(0, 100);
    m_stopBtn->setVisible(false);
    setBusy(false);
    m_status->setText(Bidi::isolate(message.section('\n', 0, 0)));
    NotificationTool::notify(tr("Disk images"), message.section('\n', 0, 0),
                             ok ? NotificationTool::Urgency::Normal : NotificationTool::Urgency::Critical,
                             "gt-stacer");
    QMessageBox box(ok ? QMessageBox::Information : QMessageBox::Warning, tr("Disk images"),
                    Bidi::isolate(message), QMessageBox::NoButton, this);
    if (!details.isEmpty()) box.setDetailedText(details.right(4000));
    QPushButton *eject = ok ? box.addButton(tr("Eject"), QMessageBox::AcceptRole) : nullptr;
    box.addButton(QMessageBox::Close);
    box.exec();
    pollDiscs();
    if (eject && box.clickedButton() == eject) QTimer::singleShot(500, this, &DiskImageTab::ejectDisc);
}
