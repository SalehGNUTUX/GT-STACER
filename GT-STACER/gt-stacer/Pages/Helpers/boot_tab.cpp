#include "boot_tab.h"
#include "../../Widgets/failure_text.h"
#include "../../Widgets/bidi_text.h"
#include "../../Widgets/loading_overlay.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QLocale>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

const char *kStaged = "staged:";

// Paths and versions are left-to-right text: isolate them so a right-to-left
// layout doesn't move their slashes ("/dev/sda" shown as "dev/sda/").
QString ltr(const QString &s) { return QChar(0x2066) + s + QChar(0x2069); }

// GRUB 2.06 turned os-prober OFF by default: an unset GRUB_DISABLE_OS_PROBER
// means "true" there, but "false" on older GRUB.
QString osProberDefault(const QString &grubVersion)
{
    const auto m = QRegularExpression("^(\\d+)\\.(\\d+)").match(grubVersion);
    if (!m.hasMatch()) return QStringLiteral("true");
    const int maj = m.captured(1).toInt(), min = m.captured(2).toInt();
    return (maj > 2 || (maj == 2 && min >= 6)) ? QStringLiteral("true") : QStringLiteral("false");
}

QString effective(const GrubTool::Settings &s, const QString &key, const QString &fallback)
{
    const QString v = s.values.value(key);
    return v.isEmpty() ? fallback : v;
}

// Old → new, one line per changed key, for the confirmation dialogs.
QString describeChanges(const QMap<QString, QString> &changes, const GrubTool::Settings &s)
{
    QStringList lines;
    for (auto it = changes.cbegin(); it != changes.cend(); ++it)
        lines << QString("%1:  %2  →  %3").arg(it.key(),
                     s.values.value(it.key()).isEmpty() ? QStringLiteral("—") : s.values.value(it.key()),
                     it.value().isEmpty() ? QStringLiteral("—") : it.value());
    return lines.join('\n');
}

} // namespace

BootTab::BootTab(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // content follows the width
    auto *page = new QWidget(scroll);
    scroll->setWidget(page);
    outer->addWidget(scroll);

    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(0, 12, 8, 0);
    root->setSpacing(12);

    m_intro = new QLabel(page);
    m_intro->setObjectName("introText");
    m_intro->setWordWrap(true);
    root->addWidget(m_intro);

    // ── This computer ──
    m_sysBox = new QGroupBox(page);
    auto *sysLay = new QVBoxLayout(m_sysBox);
    m_sysText = new QLabel(m_sysBox);
    m_sysText->setTextFormat(Qt::RichText);
    m_sysText->setWordWrap(true);
    m_sysText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sysLay->addWidget(m_sysText);
    root->addWidget(m_sysBox);

    // ── Settings ──
    m_setBox = new QGroupBox(page);
    auto *form = new QFormLayout(m_setBox);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_timeout = new QSpinBox(m_setBox);
    m_timeout->setRange(-1, 600);
    m_lblTimeout = new QLabel(m_setBox);
    auto *tRow = new QHBoxLayout;
    tRow->addWidget(m_timeout);
    tRow->addStretch();
    form->addRow(m_lblTimeout, tRow);

    m_style = new QComboBox(m_setBox);
    m_style->addItem(QString(), "menu");
    m_style->addItem(QString(), "countdown");
    m_style->addItem(QString(), "hidden");
    m_lblStyle = new QLabel(m_setBox);
    form->addRow(m_lblStyle, m_style);

    auto *dRow = new QHBoxLayout;
    m_default = new QComboBox(m_setBox);
        m_loadEntries = new QPushButton(m_setBox);
    dRow->addWidget(m_default, 1);
    dRow->addWidget(m_loadEntries);
    m_lblDefault = new QLabel(m_setBox);
    form->addRow(m_lblDefault, dRow);

    m_osProber = new QCheckBox(m_setBox);
    m_recovery = new QCheckBox(m_setBox);
    form->addRow(QString(), m_osProber);
    form->addRow(QString(), m_recovery);

    m_gfx = new QComboBox(m_setBox);
    m_gfx->setEditable(true);
    for (const char *r : {"auto", "1024x768", "1280x720", "1280x1024", "1366x768", "1600x900",
                          "1920x1080", "2560x1440", "3840x2160"})
        m_gfx->addItem(QString::fromLatin1(r));
    m_lblRes = new QLabel(m_setBox);
    form->addRow(m_lblRes, m_gfx);

    m_params = new QLineEdit(m_setBox);
    m_lblParams = new QLabel(m_setBox);
    form->addRow(m_lblParams, m_params);

    auto *thRow = new QHBoxLayout;
    m_theme = new QComboBox(m_setBox);
    m_installTheme = new QPushButton(m_setBox);
    thRow->addWidget(m_theme, 1);
    thRow->addWidget(m_installTheme);
    m_lblTheme = new QLabel(m_setBox);
    form->addRow(m_lblTheme, thRow);

    m_overrides = new QLabel(m_setBox);
    m_overrides->setObjectName("introText");
    m_overrides->setWordWrap(true);
    form->addRow(QString(), m_overrides);

    auto *aRow = new QHBoxLayout;
    m_revertBtn = new QPushButton(m_setBox);
    m_applyBtn = new QPushButton(m_setBox);
    m_applyBtn->setObjectName("primaryButton");
    aRow->addStretch();
    aRow->addWidget(m_revertBtn);
    aRow->addWidget(m_applyBtn);
    form->addRow(aRow);
    root->addWidget(m_setBox);

    // ── Maintenance ──
    m_maintBox = new QGroupBox(page);
    auto *mLay = new QVBoxLayout(m_maintBox);
    auto *mRow = new QHBoxLayout;
    m_regenBtn = new QPushButton(m_maintBox);
    m_restoreBtn = new QPushButton(m_maintBox);
    m_reinstallBtn = new QPushButton(m_maintBox);
    m_reinstallBtn->setObjectName("dangerButton");
    mRow->addWidget(m_regenBtn);
    mRow->addWidget(m_restoreBtn);
    mRow->addStretch();
    mRow->addWidget(m_reinstallBtn);
    mLay->addLayout(mRow);
    m_regenHint = new QLabel(m_maintBox);
    m_regenHint->setObjectName("introText");
    m_regenHint->setWordWrap(true);
    mLay->addWidget(m_regenHint);
    root->addWidget(m_maintBox);

    m_status = new QLabel(page);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);
    root->addStretch();

    for (QComboBox *c : {m_style, m_default, m_gfx, m_theme}) {
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(12);
    }

    connect(m_loadEntries, &QPushButton::clicked, this, &BootTab::loadMenuEntries);
    connect(m_installTheme, &QPushButton::clicked, this, &BootTab::installTheme);
    connect(m_applyBtn, &QPushButton::clicked, this, &BootTab::applyChanges);
    connect(m_revertBtn, &QPushButton::clicked, this, &BootTab::reload);
    connect(m_regenBtn, &QPushButton::clicked, this, &BootTab::regenerate);
    connect(m_restoreBtn, &QPushButton::clicked, this, &BootTab::showRestorePoints);
    connect(m_reinstallBtn, &QPushButton::clicked, this, &BootTab::reinstall);

    retranslate();
}

void BootTab::retranslate()
{
    m_intro->setText(Bidi::isolate(tr("How this computer starts, and the settings of its boot menu (GRUB). "
                        "Every change makes a restore point first; if the new boot menu can't be "
                        "generated, the previous settings are put back automatically.")));
    m_sysBox->setTitle(tr("This computer"));
    m_setBox->setTitle(tr("Boot menu"));
    m_maintBox->setTitle(tr("Maintenance"));
    m_lblTimeout->setText(tr("Wait before starting (seconds):"));
    m_timeout->setSpecialValueText(tr("forever"));
    m_timeout->setToolTip(tr("-1 = wait until a choice is made, 0 = start immediately."));
    m_lblStyle->setText(tr("Menu display:"));
    m_style->setItemText(0, tr("shown"));
    m_style->setItemText(1, tr("hidden, with a countdown"));
    m_style->setItemText(2, tr("hidden (hold Shift / press Esc to show)"));
    m_lblDefault->setText(tr("Start by default:"));
    m_loadEntries->setText(tr("Load menu entries…"));
    m_loadEntries->setToolTip(tr("Read the current boot menu to choose an entry by name (asks for your password)."));
    m_osProber->setText(tr("Add other operating systems to the menu"));
    m_osProber->setToolTip(tr("Look for Windows and other Linux systems on this computer (os-prober) "
                              "every time the boot menu is generated."));
    m_recovery->setText(tr("Show recovery-mode entries"));
    m_lblRes->setText(tr("Menu screen resolution:"));
    m_lblParams->setText(tr("Kernel parameters:"));
    m_params->setToolTip(tr("Added to every normal start (GRUB_CMDLINE_LINUX_DEFAULT), e.g. “quiet splash”. "
                            "Letters, digits and _ . , : = / + @ % - only."));
    m_lblTheme->setText(tr("Theme:"));
    m_installTheme->setText(tr("Install theme…"));
    m_installTheme->setToolTip(tr("Add a GRUB theme from a folder or an archive (.tar.gz, .tar.xz, .zip) that contains theme.txt."));
    m_revertBtn->setText(tr("Undo edits"));
    m_applyBtn->setText(tr("Apply…"));
    m_regenBtn->setText(tr("Regenerate menu"));
    m_restoreBtn->setText(tr("Restore points…"));
    m_reinstallBtn->setText(tr("Reinstall GRUB…"));
    m_regenHint->setText(Bidi::isolate(tr("“Regenerate” rebuilds the menu from the current settings — e.g. after "
                            "installing another operating system. “Reinstall GRUB” writes the boot loader "
                            "to the disk again; use it only when the computer no longer starts GRUB "
                            "(for example after installing Windows).")));
    reload();
}

void BootTab::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && m_intro) retranslate();
    QWidget::changeEvent(event);
}

void BootTab::reload()
{
    m_info = GrubTool::info();
    m_settings = GrubTool::settings();
    m_stagedThemeDir.clear();
    m_stagedThemeName.clear();

    // ── This computer ──
    QStringList rows;
    QString fw = m_info.uefi ? QStringLiteral("UEFI") : tr("BIOS (legacy)");
    if (m_info.uefi && m_info.secureBoot >= 0)
        fw += " · " + (m_info.secureBoot == 1 ? tr("Secure Boot on") : tr("Secure Boot off"));
    rows << tr("<b>Firmware:</b> %1").arg(fw);
    QString loader = m_info.loader == "grub" ? QString("GRUB %1").arg(m_info.grubVersion)
                   : m_info.loader == "systemd-boot" ? QStringLiteral("systemd-boot")
                   : m_info.loader == "refind" ? QStringLiteral("rEFInd") : tr("unknown");
    rows << tr("<b>Boot loader:</b> %1").arg(ltr(loader.toHtmlEscaped()));
    if (!m_info.uefi)
        rows << tr("<b>Boot disk:</b> %1").arg(m_info.bootDisk.isEmpty()
                ? tr("can't be determined safely (%1)").arg(m_info.bootDiskWhy.toHtmlEscaped())
                : ltr(m_info.bootDisk.toHtmlEscaped()));
    else {
        rows << tr("<b>EFI system partition:</b> %1").arg(m_info.esp.isEmpty() ? tr("not mounted")
                                                                                 : ltr(m_info.esp.toHtmlEscaped()));
        if (!m_info.bootCurrentId.isEmpty())
            rows << tr("<b>Started through:</b> %1").arg(ltr(("EFI/" + m_info.bootCurrentId).toHtmlEscaped()));
    }
    rows << tr("<b>Settings file:</b> %1 · <b>restore points:</b> %2").arg(ltr("/etc/default/grub"), ltr(GrubTool::backupDir()));
    const bool grub = m_info.loader == "grub" && !m_info.mkconfigCmd.isEmpty();
    if (!grub)
        rows << "<br>" + tr("This computer does not start with GRUB, so its boot menu is not edited here. "
                            "systemd-boot and rEFInd keep their settings in the EFI system partition; "
                            "changing the boot loader itself is deliberately not offered, because a "
                            "mistake there can leave the computer unable to start.");
    else if (!m_settings.readable)
        rows << "<br>" + tr("<b>/etc/default/grub could not be read</b>, so nothing is changed here — "
                            "GT-STACER never rewrites a settings file it could not read first.");
    if (grub && m_settings.bls)
        rows << tr("<b>Menu entries:</b> one per installed kernel (BLS, %1)").arg(ltr("/boot/loader/entries"));
    if (grub && m_info.recordfailForced)
        rows << tr("<b>Note:</b> /boot is on %1, which GRUB can't write to, so on UEFI it always waits "
                   "GRUB_RECORDFAIL_TIMEOUT at the menu. The waiting time you set here is written there too.")
                    .arg(ltr(m_info.bootFs.toHtmlEscaped()));
    m_sysText->setText(Bidi::isolate(rows.join("<br>")));
    m_editable = grub && m_settings.readable;
    m_setBox->setEnabled(m_editable && !m_busy);
    m_maintBox->setEnabled(grub && !m_busy);
    m_regenBtn->setEnabled(m_editable);
    m_restoreBtn->setEnabled(grub);    // also (especially) when the settings file is broken

    // ── Settings ──
    const auto &s = m_settings;
    m_timeout->setValue(effective(s, "GRUB_TIMEOUT", "5").toInt());
    const int st = m_style->findData(effective(s, "GRUB_TIMEOUT_STYLE", "menu"));
    m_style->setCurrentIndex(st < 0 ? 0 : st);

    m_default->clear();
    m_default->addItem(tr("The first entry of the menu"), "0");
    m_default->addItem(tr("The last one chosen (remember the choice)"), "saved");
    if (!m_info.envWritable) {
        // GRUB can't save its choice on btrfs / zfs / LVM / RAID / LUKS: every
        // boot would stop with "error: sparse file not allowed".
        if (auto *model = qobject_cast<QStandardItemModel *>(m_default->model()))
            if (QStandardItem *item = model->item(1)) {
                item->setEnabled(false);
                item->setToolTip(tr("Not possible here: GRUB can't write to %1 at boot, so it can't "
                                    "remember the last choice.").arg(m_info.bootFs));
            }
    }
    const QString def = effective(s, "GRUB_DEFAULT", "0");
    if (m_default->findData(def) < 0) m_default->addItem(def, def);
    m_default->setCurrentIndex(m_default->findData(def));

    m_osProber->setChecked(effective(s, "GRUB_DISABLE_OS_PROBER", osProberDefault(m_info.grubVersion)) == "false");
    m_recovery->setChecked(s.values.value("GRUB_DISABLE_RECOVERY") != "true");
    m_gfx->setCurrentText(effective(s, "GRUB_GFXMODE", "auto"));
    m_params->setText(s.values.value("GRUB_CMDLINE_LINUX_DEFAULT"));

    m_theme->clear();
    m_theme->addItem(tr("(no theme)"), QString());
    const auto themes = GrubTool::themes();
    for (auto it = themes.cbegin(); it != themes.cend(); ++it) m_theme->addItem(it.key(), it.value());
    const QString th = s.values.value("GRUB_THEME");
    if (!th.isEmpty() && m_theme->findData(th) < 0) m_theme->addItem(th, th);
    m_theme->setCurrentIndex(qMax(0, m_theme->findData(th)));

    updateOverrides();
    if (!m_busy) m_status->clear();
}

void BootTab::updateOverrides()
{
    QStringList lines;
    for (auto it = m_settings.overriddenIn.cbegin(); it != m_settings.overriddenIn.cend(); ++it)
        if (GrubTool::editableKeys().contains(it.key()))
            lines << tr("%1 is set in %2, which overrides /etc/default/grub — changing it here has no effect.")
                         .arg(it.key(), it.value());
    for (auto it = m_settings.appendedIn.cbegin(); it != m_settings.appendedIn.cend(); ++it)
        if (GrubTool::editableKeys().contains(it.key()))
            lines << tr("%1 is extended in %2 — what you set here is kept, and that file adds to it.")
                         .arg(it.key(), it.value());
    m_overrides->setText(Bidi::isolate(lines.join('\n')));
    m_overrides->setVisible(!lines.isEmpty());
}

QMap<QString, QString> BootTab::pendingChanges() const
{
    QMap<QString, QString> c;
    const auto &s = m_settings;
    auto put = [&](const QString &key, const QString &now, const QString &fallback) {
        if (now != effective(s, key, fallback)) c.insert(key, now);
    };
    put("GRUB_TIMEOUT", QString::number(m_timeout->value()), "5");
    // Where GRUB forces GRUB_RECORDFAIL_TIMEOUT (see reload), the wait must go there too.
    if (m_info.recordfailForced)
        put("GRUB_RECORDFAIL_TIMEOUT", QString::number(qMax(0, m_timeout->value())), "30");
    put("GRUB_TIMEOUT_STYLE", m_style->currentData().toString(), "menu");
    const QString def = m_default->currentData().toString();
    put("GRUB_DEFAULT", def, "0");
    // Choosing "saved" here means "remember the last choice" → GRUB_SAVEDEFAULT=true;
    // added only when the user CHANGES to it (Fedora ships GRUB_DEFAULT=saved
    // with grubby managing saved_entry, and must not get it on every Apply).
    if (def == "saved" && effective(s, "GRUB_DEFAULT", "0") != "saved") put("GRUB_SAVEDEFAULT", "true", "false");
    else if (effective(s, "GRUB_DEFAULT", "0") == "saved" && s.values.value("GRUB_SAVEDEFAULT") == "true")
        c.insert("GRUB_SAVEDEFAULT", "false");     // only when the user moves AWAY from "saved"
    put("GRUB_DISABLE_OS_PROBER", m_osProber->isChecked() ? "false" : "true",
        effective(s, "GRUB_DISABLE_OS_PROBER", osProberDefault(m_info.grubVersion)));
    put("GRUB_DISABLE_RECOVERY", m_recovery->isChecked() ? "false" : "true",
        s.values.value("GRUB_DISABLE_RECOVERY").isEmpty() ? "false" : s.values.value("GRUB_DISABLE_RECOVERY"));
    put("GRUB_GFXMODE", m_gfx->currentText().trimmed(), "auto");
    // Compare normalised: double spaces in the file are not an edit by the user.
    if (m_params->text().simplified() != s.values.value("GRUB_CMDLINE_LINUX_DEFAULT").simplified())
        c.insert("GRUB_CMDLINE_LINUX_DEFAULT", m_params->text().simplified());
    const QString theme = m_theme->currentData().toString();
    if (theme.startsWith(kStaged))
        c.insert("GRUB_THEME", m_info.grubDir + "/themes/" + theme.mid(int(strlen(kStaged))) + "/theme.txt");
    else
        put("GRUB_THEME", theme, QString());
    // Values identical to what is there are not changes.
    for (auto it = c.begin(); it != c.end();)
        it = (s.values.value(it.key()) == it.value() && it.key() != "GRUB_THEME") ? c.erase(it) : std::next(it);
    return c;
}

void BootTab::setBusy(bool busy)
{
    m_busy = busy;
    const bool grub = m_info.loader == "grub" && !m_info.mkconfigCmd.isEmpty();
    m_setBox->setEnabled(!busy && m_editable);
    m_maintBox->setEnabled(!busy && grub);
    if (!busy) LoadingOverlay::on(this)->stop();
}

// ── Menu entries / themes ──────────────────────────────────────────────

void BootTab::loadMenuEntries()
{
    setBusy(true);
    m_status->setText(Bidi::isolate(tr("Reading the boot menu…")));
    LoadingOverlay::on(this)->start(tr("Reading the boot menu"),
        tr("Your password may be asked — only root can read grub.cfg."));
    struct R { QVector<GrubTool::MenuEntry> e; CommandResult r; };
    auto *w = new QFutureWatcher<R>(this);
    connect(w, &QFutureWatcher<R>::finished, this, [this, w]{
        const R x = w->result();
        w->deleteLater();
        setBusy(false);
        if (!x.r.ok()) {
            m_status->setText(Bidi::isolate(FailureText::brief(x.r)));
            FailureText::show(this, tr("Boot (GRUB)"), tr("The boot menu could not be read."), x.r);
            return;
        }
        const QString keep = m_default->currentData().toString();
        while (m_default->count() > 2) m_default->removeItem(2);
        for (const GrubTool::MenuEntry &e : x.e) {
            QString shown = e.title;
            shown.replace('>', "  ›  ");
            m_default->addItem(shown, e.id);      // the id survives title changes
            // An older setting may still name the entry by its title.
            if (keep == e.title) m_default->setItemData(m_default->count() - 1, e.title);
        }
        if (m_default->findData(keep) < 0) m_default->addItem(keep, keep);
        m_default->setCurrentIndex(m_default->findData(keep));
        m_status->setText(Bidi::isolate(tr("%1 menu entries loaded.").arg(x.e.size())));
        m_default->showPopup();
    });
    w->setFuture(QtConcurrent::run([]{ R x; x.e = GrubTool::menuEntryList(&x.r); return x; }));
}

void BootTab::installTheme()
{
    QString src = QFileDialog::getOpenFileName(this, tr("Choose a GRUB theme archive (or cancel to pick a folder)"),
        QDir::homePath(), tr("Theme archives (*.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tar.zst *.tar *.zip)"));
    if (src.isEmpty())
        src = QFileDialog::getExistingDirectory(this, tr("Choose the theme folder (it contains theme.txt)"), QDir::homePath());
    if (src.isEmpty()) return;
    QString name, error;
    const QString dir = GrubTool::stageTheme(src, &name, &error);
    if (dir.isEmpty()) {
        QMessageBox::warning(this, tr("Install theme"), Bidi::isolate(error == "bad-archive"
            ? tr("The archive could not be unpacked. It may be damaged, or of a type that isn't supported.")
            : tr("No GRUB theme was found in it — a theme is a folder that contains a file named theme.txt.")));
        return;
    }
    const QString existing = m_info.grubDir + "/themes/" + name + "/theme.txt";
    if (GrubTool::themes().value(name) == existing
        && QMessageBox::question(this, tr("Install theme"),
               Bidi::isolate(tr("A theme named “%1” is already installed (%2). Replace it with the new one?")
                   .arg(name, m_info.grubDir + "/themes/" + name))) != QMessageBox::Yes)
        return;
    m_stagedThemeDir = dir;
    m_stagedThemeName = name;
    const QString data = QString(kStaged) + name;
    if (m_theme->findData(data) < 0) m_theme->addItem(tr("%1 (new — installed on Apply)").arg(name), data);
    m_theme->setCurrentIndex(m_theme->findData(data));
    m_status->setText(Bidi::isolate(tr("Theme “%1” is ready. Click “Apply…” to install and use it.").arg(name)));
}

// ── Apply / regenerate / restore / reinstall ───────────────────────────

void BootTab::applyChanges()
{
    QMap<QString, QString> changes = pendingChanges();
    if (changes.isEmpty()) { m_status->setText(Bidi::isolate(tr("Nothing has changed."))); return; }
    // A theme from /usr/share is copied under the GRUB folder: GRUB can't read
    // /usr at boot when / is encrypted / on LVM and /boot is separate.
    const QString th = changes.value("GRUB_THEME");
    if (th.startsWith("/usr/share/grub/themes/")) {
        const QString dir = QFileInfo(th).absolutePath();
        m_stagedThemeDir = dir;
        m_stagedThemeName = QFileInfo(dir).fileName();
        changes.insert("GRUB_THEME", m_info.grubDir + "/themes/" + m_stagedThemeName + "/theme.txt");
    }
    const QString style = m_style->currentData().toString();
    if (m_timeout->value() < 0 && style != "menu") {
        QMessageBox::warning(this, tr("Boot (GRUB)"), Bidi::isolate(
            tr("“Wait forever” with a hidden menu would show a black screen that waits for ever — the "
               "computer would look frozen. Show the menu, or choose a waiting time.")));
        return;
    }
    if (m_timeout->value() == 0 && style == "hidden"
        && QMessageBox::question(this, tr("Boot (GRUB)"), Bidi::isolate(
               tr("A hidden menu with no waiting time starts immediately: reaching the menu (e.g. for "
                  "recovery mode) then needs Shift or Esc held at exactly the right moment. Continue?")))
               != QMessageBox::Yes)
        return;
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
        if (it.key() == "GRUB_THEME" && it.value().endsWith("/" + m_stagedThemeName + "/theme.txt")
            && !m_stagedThemeName.isEmpty())
            continue;                          // created by the transaction itself
        const QString why = GrubTool::validate(it.key(), it.value());
        if (why.isEmpty()) continue;
        QString msg;
        if (why == "number")          msg = tr("The waiting time must be a whole number from -1 to 600.");
        else if (why == "resolution") msg = tr("The resolution must look like 1920x1080 or auto (several separated by commas).");
        else if (why == "params")     msg = tr("Kernel parameters may contain only letters, digits, spaces and _ . , : = / + @ % -");
        else if (why == "theme")      msg = tr("The theme must be a theme.txt inside a GRUB themes folder.");
        else                          msg = tr("The value of %1 is not valid.").arg(it.key());
        QMessageBox::warning(this, tr("Boot (GRUB)"), Bidi::isolate(msg));
        return;
    }
    if (QMessageBox::question(this, tr("Apply boot settings"),
            Bidi::isolate(tr("These settings will change:\n\n%1\n\nA restore point is made first. If the new boot menu "
               "can't be generated, the previous settings are put back automatically. Your password "
               "will be asked.").arg(describeChanges(changes, m_settings)))) != QMessageBox::Yes)
        return;
    const bool withTheme = changes.contains("GRUB_THEME") && !m_stagedThemeName.isEmpty()
                        && changes.value("GRUB_THEME").contains("/" + m_stagedThemeName + "/");
    // BLS (Fedora): the kernel parameters are stored in each entry; without
    // --update-bls-cmdline, changing them here would silently do nothing.
    const bool cmdline = changes.contains("GRUB_CMDLINE_LINUX_DEFAULT") && m_settings.bls;
    if (cmdline && m_info.blsCmdlineOpt.isEmpty()) {
        QMessageBox::warning(this, tr("Boot (GRUB)"), Bidi::isolate(
            tr("On this system the kernel parameters are stored in each boot entry (BLS), and this "
               "version of grub2-mkconfig can't update them. Change them in a terminal instead:\n"
               "    sudo grubby --update-kernel=ALL --args=\"…\"")));
        return;
    }
    runApply(GrubTool::applyChanges(m_settings.raw, changes), tr("The boot settings were applied."),
             withTheme ? m_stagedThemeDir : QString(), withTheme ? m_stagedThemeName : QString(), cmdline);
}

void BootTab::regenerate()
{
    runApply(m_settings.raw, tr("The boot menu was regenerated."));
}

void BootTab::runApply(const QString &newText, const QString &what, const QString &themeDir,
                       const QString &themeName, bool updateBls)
{
    if (!m_editable) return;               // never write a file that could not be read
    setBusy(true);
    m_status->setText(Bidi::isolate(tr("Applying… generating the boot menu can take a minute (it looks for other systems).")));
    LoadingOverlay::on(this)->start(tr("Applying the boot settings"),
        tr("Generating the boot menu can take a minute — it looks for other systems."));
    auto *w = new QFutureWatcher<CommandResult>(this);
    connect(w, &QFutureWatcher<CommandResult>::finished, this, [this, w, what]{
        const CommandResult r = w->result();
        w->deleteLater();
        setBusy(false);
        if (!r.ok()) {
            reload();
            // The script's own "gt-stacer:" line says what was (not) changed.
            QString line;
            for (const QString &l : r.output.split('\n'))
                if (l.startsWith("gt-stacer:")) line = l.mid(10).trimmed();
            m_status->setText(Bidi::isolate(line.isEmpty() ? FailureText::brief(r) : line));
            FailureText::show(this, tr("Boot (GRUB)"),
                              line.isEmpty() ? tr("The change was not applied.") : line, r);
            return;
        }
        reload();
        m_status->setText(Bidi::isolate(what));
        QMessageBox::information(this, tr("Boot (GRUB)"),
            Bidi::isolate(what + "\n\n" + tr("A restore point was saved. The new settings take effect at the next start.")));
    });
    const QByteArray base = m_settings.rawBytes;   // exact bytes read — refused if the file changed since
    w->setFuture(QtConcurrent::run([newText, base, themeDir, themeName, updateBls]{
        return GrubTool::apply(newText, base, themeDir, themeName, updateBls);
    }));
}

void BootTab::showRestorePoints()
{
    const QStringList points = GrubTool::restorePoints();
    if (points.isEmpty()) {
        QMessageBox::information(this, tr("Restore points"),
            Bidi::isolate(tr("There are no restore points yet. One is made automatically before every change made here.")));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Restore points"));
    dlg.resize(760, 480);
    auto *v = new QVBoxLayout(&dlg);
    auto *hint = new QLabel(tr("Choose a restore point to see its settings. Restoring makes a new restore "
                               "point of the current settings first, so it can be undone too."), &dlg);
    hint->setWordWrap(true);
    v->addWidget(hint);
    auto *h = new QHBoxLayout;
    auto *list = new QListWidget(&dlg);
    for (const QString &p : points) {
        const QDateTime dt = QDateTime::fromString(p.left(15), "yyyyMMdd-HHmmss");
        auto *it = new QListWidgetItem(dt.isValid() ? QLocale().toString(dt, QLocale::ShortFormat) + (p.size() > 15 ? " (" + p.mid(16) + ")" : QString()) : p, list);
        it->setData(Qt::UserRole, p);
    }
    auto *view = new QPlainTextEdit(&dlg);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    h->addWidget(list, 1);
    h->addWidget(view, 3);
    v->addLayout(h, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    QPushButton *restore = buttons->addButton(tr("Restore this"), QDialogButtonBox::AcceptRole);
    restore->setEnabled(false);
    v->addWidget(buttons);
    connect(list, &QListWidget::currentItemChanged, &dlg, [&](QListWidgetItem *it){
        view->setPlainText(it ? GrubTool::restorePointText(it->data(Qt::UserRole).toString()) : QString());
        restore->setEnabled(it && !view->toPlainText().isEmpty());
    });
    connect(restore, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    list->setCurrentRow(0);
    if (dlg.exec() != QDialog::Accepted || !list->currentItem()) return;
    const QString stamp = list->currentItem()->data(Qt::UserRole).toString();
    const QString label = list->currentItem()->text();
    if (QMessageBox::question(this, tr("Restore points"),
            Bidi::isolate(tr("Put back the boot settings from %1 and regenerate the boot menu?").arg(label)))
            != QMessageBox::Yes)
        return;
    // Restored BY STAMP, as root, byte-for-byte — not via the text shown here,
    // and also when the current /etc/default/grub is missing or broken.
    setBusy(true);
    m_status->setText(Bidi::isolate(tr("Restoring… generating the boot menu can take a minute.")));
    LoadingOverlay::on(this)->start(tr("Restoring the boot settings"),
        tr("%1 — then the boot menu is generated again.").arg(label));
    auto *w = new QFutureWatcher<CommandResult>(this);
    const QString what = tr("The boot settings from %1 were restored.").arg(label);
    connect(w, &QFutureWatcher<CommandResult>::finished, this, [this, w, what]{
        const CommandResult r = w->result();
        w->deleteLater();
        setBusy(false);
        reload();
        if (!r.ok()) {
            QString line;
            for (const QString &l : r.output.split('\n'))
                if (l.startsWith("gt-stacer:")) line = l.mid(10).trimmed();
            m_status->setText(Bidi::isolate(line.isEmpty() ? FailureText::brief(r) : line));
            FailureText::show(this, tr("Restore points"), line.isEmpty() ? tr("The restore point was not put back.") : line, r);
            return;
        }
        m_status->setText(Bidi::isolate(what));
        QMessageBox::information(this, tr("Restore points"),
            Bidi::isolate(what + "\n\n" + tr("The settings in use before were saved as a new restore point. "
                                              "The restored settings take effect at the next start.")));
    });
    w->setFuture(QtConcurrent::run([stamp]{ return GrubTool::restore(stamp); }));
}

void BootTab::reinstall()
{
    // Fresh facts: the ESP may have been mounted/unmounted, a disk added …
    m_info = GrubTool::info();
    QStringList cmds;
    const QString why = GrubTool::reinstallPlan(m_info, &cmds);
    if (!why.isEmpty()) {
        QString msg;
        if (why == "not-confirmed")
            msg = tr("The firmware did not report GRUB as the boot loader that started this computer "
                     "(or efibootmgr is not installed), so GRUB is not reinstalled — it would become the "
                     "first boot entry and could replace the loader you really use.");
        else if (why == "fedora-uefi")
            msg = tr("On Fedora-family systems with UEFI, GRUB must not be reinstalled with grub2-install. "
                     "Reinstall the packages instead, in a terminal:\n    sudo dnf reinstall shim-* grub2-efi-*");
        else if (why == "suse-uefi")
            msg = tr("On openSUSE / SUSE with UEFI, GRUB is installed through shim by the system's own "
                     "tool, not grub2-install. In a terminal:\n    sudo update-bootloader --reinit");
        else if (why == "secure-boot")
            msg = tr("Secure Boot is on. A plain grub-install would write an unsigned GRUB that the "
                     "firmware refuses to start. Reinstall your distribution's signed GRUB / shim "
                     "packages instead (or sign GRUB with your own keys, e.g. with sbctl).");
        else if (why == "no-esp")
            msg = tr("The EFI system partition is not mounted (looked for /boot/efi, /efi and /boot), so GRUB "
                     "can't be reinstalled safely.");
        else if (why == "no-disk")
            msg = tr("The disk that holds /boot can't be determined safely (%1). GRUB is not reinstalled "
                     "rather than risk writing it to the wrong disk.").arg(m_info.bootDiskWhy);
        else
            msg = tr("This computer does not start with GRUB.");
        QMessageBox::warning(this, tr("Reinstall GRUB"), Bidi::isolate(msg));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Reinstall GRUB"));
    auto *v = new QVBoxLayout(&dlg);
    auto *text = new QLabel(&dlg);
    text->setWordWrap(true);
    text->setTextFormat(Qt::RichText);
    QString where = !m_info.uefi
        ? tr("the start of the disk <b>%1</b>").arg(m_info.bootDisk.toHtmlEscaped())
        : m_info.efiId.isEmpty()
            ? tr("the EFI system partition <b>%1</b> (boot entry named by your distribution)").arg(m_info.esp.toHtmlEscaped())
            : tr("the EFI system partition <b>%1</b> (boot entry “%2”)").arg(m_info.esp.toHtmlEscaped(), m_info.efiId.toHtmlEscaped());
    QString html = tr("<p>GRUB will be written again to %1, then the boot menu is regenerated.</p>"
                      "<p>Use this only when the computer no longer shows the GRUB menu — for example after "
                      "Windows was installed or updated. If GRUB works now, you don't need it.</p>"
                      "<p>These exact commands will run as administrator:</p>").arg(where);
    html += "<pre>" + cmds.join("\n").toHtmlEscaped() + "</pre>";
    if (m_info.uefi)
        html += tr("<p>The firmware currently starts <b>%1</b>. After reinstalling, <b>this</b> system's GRUB "
                   "is the first boot entry. If another Linux system's GRUB starts this computer now, its "
                   "menu is replaced by this one (other systems appear only if “Add other operating "
                   "systems” is on).</p>").arg(("EFI/" + (m_info.bootCurrentId.isEmpty() ? QStringLiteral("?") : m_info.bootCurrentId)).toHtmlEscaped());
    if (m_info.uefi && m_info.secureBoot == 1)   // only Debian/Ubuntu get here with Secure Boot on
        html += tr("<p><b>Secure Boot is on.</b> Debian and Ubuntu's grub-install keeps the signed boot "
                   "chain (shim). Make sure the signed packages are installed (shim-signed, "
                   "grub-efi-amd64-signed).</p>");
    text->setText(Bidi::isolate(html));
    text->setMinimumWidth(520);
    v->addWidget(text);
    auto *ack = new QCheckBox(tr("I understand this changes how the computer starts"), &dlg);
    v->addWidget(ack);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
    QPushButton *go = buttons->addButton(tr("Reinstall"), QDialogButtonBox::DestructiveRole);
    go->setObjectName("dangerButton");
    go->setEnabled(false);
    connect(ack, &QCheckBox::toggled, go, &QPushButton::setEnabled);
    connect(go, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    v->addWidget(buttons);
    if (dlg.exec() != QDialog::Accepted) return;

    setBusy(true);
    m_status->setText(Bidi::isolate(tr("Reinstalling GRUB…")));
    LoadingOverlay::on(this)->start(tr("Reinstalling GRUB"),
        tr("Do not turn the computer off until it is done."));
    const GrubTool::Info info = m_info;
    auto *w = new QFutureWatcher<CommandResult>(this);
    connect(w, &QFutureWatcher<CommandResult>::finished, this, [this, w]{
        const CommandResult r = w->result();
        w->deleteLater();
        setBusy(false);
        reload();
        if (!r.ok()) {
            QString line;
            for (const QString &l : r.output.split('\n'))
                if (l.startsWith("gt-stacer:")) line = l.mid(10).trimmed();
            m_status->setText(Bidi::isolate(line.isEmpty() ? FailureText::brief(r) : line));
            FailureText::show(this, tr("Reinstall GRUB"), line.isEmpty() ? tr("GRUB was not reinstalled.") : line, r);
            return;
        }
        m_status->setText(Bidi::isolate(tr("GRUB was reinstalled and the boot menu regenerated.")));
        QMessageBox::information(this, tr("Reinstall GRUB"), Bidi::isolate(m_status->text()));
    });
    w->setFuture(QtConcurrent::run([info]{ return GrubTool::reinstall(info); }));
}
