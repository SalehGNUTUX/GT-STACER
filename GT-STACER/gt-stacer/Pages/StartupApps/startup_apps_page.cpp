#include "startup_apps_page.h"
#include "../../Dialogs/startup_add_dialog.h"
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
        p.setBrush(QColor(0x31, 0x32, 0x44));
        p.setPen(QColor(0x45, 0x47, 0x5a));
        p.drawRoundedRect(1, 1, size - 2, size - 2, 6, 6);
        QFont f = p.font(); f.setBold(true); f.setPixelSize(size / 2);
        p.setFont(f); p.setPen(QColor(0xa6, 0xad, 0xc8));
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
    title->setStyleSheet("font-size:22px;font-weight:bold;color:#cdd6f4;");
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
    m_emptyHint->setStyleSheet("color:#6c7086;padding:24px;");
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_rowsBox->addWidget(m_emptyHint);
    m_rowsBox->addStretch();
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    m_countLabel = new QLabel;
    m_countLabel->setStyleSheet("color:#6c7086;");
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
    row->setStyleSheet(
        "QFrame#startupRow { background:#181825; border:1px solid #313244; "
        "border-radius:10px; padding:8px; }"
        "QFrame#startupRow:hover { background:#1f2238; }");
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
    nameLbl->setStyleSheet("color:#cdd6f4;font-weight:600;");
    auto *execLbl = new QLabel(entry.exec);
    execLbl->setStyleSheet("color:#6c7086;font-family:Monospace;font-size:11px;");
    execLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textCol->addWidget(nameLbl);
    textCol->addWidget(execLbl);
    h->addLayout(textCol, 1);

    // Toggle ON/OFF: QPushButton + checkable, colored by state.
    auto *toggle = new QPushButton;
    toggle->setCheckable(true);
    toggle->setChecked(entry.enabled);
    toggle->setMinimumWidth(78);
    auto applyToggleStyle = [toggle]() {
        if (toggle->isChecked()) {
            toggle->setText(tr("ON"));
            toggle->setStyleSheet(
                "background:#a6e3a1;color:#11111b;font-weight:bold;"
                "border-radius:6px;padding:6px 14px;");
        } else {
            toggle->setText(tr("OFF"));
            toggle->setStyleSheet(
                "background:#45475a;color:#cdd6f4;"
                "border-radius:6px;padding:6px 14px;");
        }
    };
    applyToggleStyle();
    const QString path = entry.filePath;
    connect(toggle, &QPushButton::toggled, this, [path, applyToggleStyle, toggle](bool on) {
        if (on) StartupTool::enable(path);
        else    StartupTool::disable(path);
        applyToggleStyle();
    });
    h->addWidget(toggle);

    // Remove button (trash icon)
    auto *rmBtn = new QToolButton;
    rmBtn->setText("✕");
    rmBtn->setToolTip(tr("Remove from autostart"));
    rmBtn->setStyleSheet(
        "QToolButton { color:#f38ba8; background:transparent; "
        "border:1px solid #45475a; border-radius:6px; padding:4px 10px; }"
        "QToolButton:hover { background:#311b22; }");
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
