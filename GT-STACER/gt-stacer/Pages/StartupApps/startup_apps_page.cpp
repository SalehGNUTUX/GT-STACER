#include "startup_apps_page.h"
#include "../../Dialogs/startup_add_dialog.h"
#include "../../Managers/theme.h"
#include "../../../gt-stacer-core/Tools/startup_tool.h"
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
// Renders an icon-theme name / absolute path / fallback to a generic
// application glyph. Pixmap is returned at the requested size.
QPixmap iconPixmapFor(const QString &iconKey, int size)
{
    QIcon ic;
    if (!iconKey.isEmpty()) {
        if (iconKey.startsWith('/'))                  ic = QIcon(iconKey);
        if (ic.isNull() || ic.actualSize({size,size}).isEmpty())
            ic = QIcon::fromTheme(iconKey);
    }
    if (ic.isNull())
        ic = QIcon::fromTheme("application-x-executable");
    if (ic.isNull()) {
        // Hand-painted fallback square so we never show an empty slot.
        QPixmap pm(size, size); pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Theme::surface());
        p.setPen(Theme::overlay());
        p.drawRoundedRect(1, 1, size - 2, size - 2, 6, 6);
        QFont f = p.font(); f.setBold(true); f.setPixelSize(size / 2);
        p.setFont(f); p.setPen(Theme::subtext());
        p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, "?");
        return pm;
    }
    return ic.pixmap(size, size);
}
} // namespace

StartupAppsPage::StartupAppsPage(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Startup Applications"));
    title->setObjectName("pageTitle");
    root->addWidget(title);

    // Header row: search + add + refresh + count.
    auto *header = new QHBoxLayout;
    header->setSpacing(8);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    header->addWidget(m_search, 1);

    auto *addBtn = new QPushButton(tr("Add…"));
    addBtn->setObjectName("primaryButton");
    header->addWidget(addBtn);

    auto *refreshBtn = new QPushButton(tr("Refresh"));
    header->addWidget(refreshBtn);

    root->addLayout(header);

    // Scrollable area containing the rows.
    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto *host = new QWidget;
    m_rowsBox = new QVBoxLayout(host);
    m_rowsBox->setContentsMargins(0, 0, 0, 0);
    m_rowsBox->setSpacing(8);
    m_emptyHint = new QLabel(tr("No autostart entries yet. Click \"Add…\" to create one."));
    m_emptyHint->setObjectName("emptyHint");
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_rowsBox->addWidget(m_emptyHint);
    m_rowsBox->addStretch();
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    m_countLabel = new QLabel;
    m_countLabel->setObjectName("countLabel");
    root->addWidget(m_countLabel);

    connect(addBtn,     &QPushButton::clicked, this, &StartupAppsPage::openAddDialog);
    connect(refreshBtn, &QPushButton::clicked, this, &StartupAppsPage::refresh);
    connect(m_search,   &QLineEdit::textChanged, this, &StartupAppsPage::onSearchChanged);

    refresh();
}

StartupAppsPage::~StartupAppsPage() = default;

void StartupAppsPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
}

void StartupAppsPage::clearRows()
{
    // Remove every previously-added row widget, keeping the empty hint + stretch.
    QList<QWidget*> rows;
    for (int i = 0; i < m_rowsBox->count(); ++i) {
        auto *w = m_rowsBox->itemAt(i)->widget();
        if (w && w != m_emptyHint) rows.append(w);
    }
    for (auto *w : rows) { m_rowsBox->removeWidget(w); w->deleteLater(); }
}

void StartupAppsPage::appendRow(const StartupEntry &entry)
{
    auto *row = new QFrame;
    row->setObjectName("startupRow");
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(10, 8, 10, 8);
    h->setSpacing(12);

    auto *iconLbl = new QLabel;
    iconLbl->setPixmap(iconPixmapFor(entry.icon, 32));
    iconLbl->setFixedSize(36, 36);
    iconLbl->setAlignment(Qt::AlignCenter);
    h->addWidget(iconLbl);

    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(2);
    auto *nameLbl = new QLabel(entry.name.isEmpty() ? QFileInfo(entry.filePath).baseName() : entry.name);
    nameLbl->setObjectName("startupName");
    auto *execLbl = new QLabel(entry.exec);
    execLbl->setObjectName("startupExec");
    execLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textCol->addWidget(nameLbl);
    textCol->addWidget(execLbl);
    if (entry.delaySeconds > 0) {
        auto *delayLbl = new QLabel(tr("⏱ delayed %1 s after login").arg(entry.delaySeconds));
        delayLbl->setObjectName("startupDelay");
        textCol->addWidget(delayLbl);
    }
    h->addLayout(textCol, 1);

    // Toggle ON/OFF: QPushButton + checkable, colored by state.
    auto *toggle = new QPushButton;
    toggle->setObjectName("startupToggle");
    toggle->setCheckable(true);
    toggle->setChecked(entry.enabled);
    toggle->setMinimumWidth(78);
    // Styling lives in QSS keyed on the dynamic "on" property so it follows the
    // active theme; we only flip the property + text here and re-polish.
    auto applyToggleStyle = [toggle]() {
        const bool on = toggle->isChecked();
        toggle->setText(on ? tr("ON") : tr("OFF"));
        toggle->setProperty("on", on);
        toggle->style()->unpolish(toggle);
        toggle->style()->polish(toggle);
    };
    applyToggleStyle();
    const QString path = entry.filePath;
    connect(toggle, &QPushButton::toggled, this, [path, applyToggleStyle, toggle](bool on) {
        if (on) StartupTool::enable(path);
        else    StartupTool::disable(path);
        applyToggleStyle();
    });
    h->addWidget(toggle);

    // Edit button — reopens the add dialog pre-filled so the delay and other
    // fields of an already-configured entry can be changed.
    auto *editBtn = new QToolButton;
    editBtn->setText("✎");
    editBtn->setObjectName("startupEdit");
    editBtn->setToolTip(tr("Edit this entry"));
    connect(editBtn, &QToolButton::clicked, this, [this, entry]() {
        openEditDialog(entry);
    });
    h->addWidget(editBtn);

    // Remove button (trash icon)
    auto *rmBtn = new QToolButton;
    rmBtn->setText("✕");
    rmBtn->setObjectName("startupRemove");
    rmBtn->setToolTip(tr("Remove from autostart"));
    connect(rmBtn, &QToolButton::clicked, this, [this, path, entry]() {
        int yes = QMessageBox::question(this, tr("Remove"),
            tr("Remove '%1' from autostart?").arg(entry.name));
        if (yes != QMessageBox::Yes) return;
        StartupTool::remove(path);
        refresh();
    });
    h->addWidget(rmBtn);

    // Insert above the empty hint and stretch.
    int insertIdx = m_rowsBox->count() - 2;
    if (insertIdx < 0) insertIdx = 0;
    m_rowsBox->insertWidget(insertIdx, row);
}

void StartupAppsPage::refresh()
{
    clearRows();
    const auto entries = StartupTool::entries();
    const QString filter = m_search ? m_search->text().trimmed() : QString();

    int shown = 0;
    for (const auto &e : entries) {
        if (!filter.isEmpty()
            && !e.name.contains(filter, Qt::CaseInsensitive)
            && !e.exec.contains(filter, Qt::CaseInsensitive)) continue;
        appendRow(e);
        ++shown;
    }
    m_emptyHint->setVisible(shown == 0);
    m_countLabel->setText(tr("%1 autostart entries · %2 visible")
        .arg(entries.size()).arg(shown));
}

void StartupAppsPage::onSearchChanged(const QString &)
{
    refresh();
}

void StartupAppsPage::openAddDialog()
{
    StartupAddDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    if (!StartupTool::add(dlg.result())) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not write the autostart entry."));
        return;
    }
    refresh();
}

void StartupAppsPage::openEditDialog(const StartupEntry &entry)
{
    StartupAddDialog dlg(this);
    dlg.loadForEdit(entry);
    if (dlg.exec() != QDialog::Accepted) return;

    StartupEntry updated = dlg.result();
    updated.enabled = entry.enabled;   // preserve the ON/OFF state across an edit
    // Remove the old file first — renaming the entry changes the derived
    // filename, so overwriting alone would leave a stale duplicate behind.
    StartupTool::remove(entry.filePath);
    if (!StartupTool::add(updated)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not update the autostart entry."));
    }
    refresh();
}
