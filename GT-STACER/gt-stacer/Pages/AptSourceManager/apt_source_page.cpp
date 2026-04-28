#include "apt_source_page.h"
#include "ui_apt_source_page.h"
#include "../../Managers/tool_manager.h"
#include "../../Dialogs/edit_source_dialog.h"
#include "../../../gt-stacer-core/Tools/apt_source_tool.h"
#include "../../../gt-stacer-core/Tools/package_tool.h"
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QToolTip>
#include <QHelpEvent>

// ── Custom delegate: shows full text in tooltip on hover ──────────────
#include <QStyledItemDelegate>
#include <QPainter>

class FullTextDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool helpEvent(QHelpEvent *e, QAbstractItemView *view,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override {
        if (e->type() == QEvent::ToolTip) {
            QToolTip::showText(e->globalPos(),
                index.data(Qt::DisplayRole).toString(), view);
            return true;
        }
        return QStyledItemDelegate::helpEvent(e, view, option, index);
    }
};

AptSourcePage::AptSourcePage(QWidget *parent)
    : QWidget(parent), ui(new Ui::AptSourcePage)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 5, this);
    m_model->setHorizontalHeaderLabels({
        tr("Type"), tr("URI"), tr("Suite"), tr("Components"), tr("Enabled")});

    ui->sourceTable->setModel(m_model);
    // Stretch URI column
    ui->sourceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->sourceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->sourceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->sourceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->sourceTable->setItemDelegate(new FullTextDelegate(this));
    ui->sourceTable->setMouseTracking(true);
    ui->sourceTable->setSortingEnabled(true);
    ui->sourceTable->verticalHeader()->hide();

    // Double-click to edit
    connect(ui->sourceTable, &QTableView::doubleClicked, this, [this](const QModelIndex &idx){
        if (!idx.isValid()) return;
        int row = idx.row();
        AptSource src;
        src.type       = m_model->item(row, 0)->text();
        src.uri        = m_model->item(row, 1)->text();
        src.suite      = m_model->item(row, 2)->text();
        src.components = m_model->item(row, 3)->text();
        src.enabled    = m_model->item(row, 4)->data(Qt::UserRole + 1).toBool();
        src.filePath   = m_model->item(row, 4)->data(Qt::UserRole).toString();

        EditSourceDialog dlg(src, this);
        if (dlg.exec() == QDialog::Accepted) {
            auto result = dlg.result();
            // toggle enabled state if changed
            if (result.enabled != src.enabled)
                AptSourceTool::setEnabled(src.filePath, result.enabled);
            refresh();
        }
    });

    connect(ui->refreshButton, &QPushButton::clicked, this, &AptSourcePage::refresh);
    connect(ui->addButton,     &QPushButton::clicked, this, &AptSourcePage::addSource);
    connect(ui->removeButton,  &QPushButton::clicked, this, &AptSourcePage::removeSource);
    connect(ui->toggleButton,  &QPushButton::clicked, this, &AptSourcePage::toggleSource);

    if (!PackageTool::has(PkgMgr::APT)) {
        ui->addButton->setEnabled(false);
        ui->addButton->setToolTip(tr("APT is not available on this system"));
        ui->toggleButton->setEnabled(false);
        ui->removeButton->setEnabled(false);
    }

    refresh();
}

AptSourcePage::~AptSourcePage() { delete ui; }

void AptSourcePage::refresh()
{
    auto sources = AptSourceTool::sources();
    m_model->setRowCount(0);
    for (const auto &s : sources) {
        QList<QStandardItem*> row;
        auto mkItem = [](const QString &txt, bool enabled) {
            auto *i = new QStandardItem(txt);
            i->setToolTip(txt);
            if (!enabled) i->setForeground(QColor("#6c7086"));
            return i;
        };
        row << mkItem(s.type, s.enabled)
            << mkItem(s.uri, s.enabled)
            << mkItem(s.suite, s.enabled)
            << mkItem(s.components, s.enabled)
            << mkItem(s.enabled ? tr("✓ Enabled") : tr("✗ Disabled"), s.enabled);
        row[4]->setData(s.filePath,  Qt::UserRole);
        row[4]->setData(s.enabled,   Qt::UserRole + 1);
        m_model->appendRow(row);
    }
    ui->sourceTable->resizeColumnToContents(0);
    ui->sourceTable->resizeColumnToContents(2);
    ui->sourceTable->resizeColumnToContents(4);
}

void AptSourcePage::addSource()
{
    EditSourceDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    auto s = dlg.result();
    if (s.uri.isEmpty() || s.suite.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid"), tr("URI and Suite are required."));
        return;
    }
    if (AptSourceTool::add(s)) refresh();
    else QMessageBox::warning(this, tr("Error"), tr("Could not add source."));
}

void AptSourcePage::removeSource()
{
    auto idx = ui->sourceTable->currentIndex();
    if (!idx.isValid()) return;
    QString path = m_model->item(idx.row(), 4)->data(Qt::UserRole).toString();
    auto ans = QMessageBox::question(this, tr("Remove Source"),
        tr("Remove this APT source?\n%1").arg(
            m_model->item(idx.row(), 1)->text()));
    if (ans != QMessageBox::Yes) return;
    if (AptSourceTool::remove(path)) refresh();
}

void AptSourcePage::toggleSource()
{
    auto idx = ui->sourceTable->currentIndex();
    if (!idx.isValid()) return;
    QString path    = m_model->item(idx.row(), 4)->data(Qt::UserRole).toString();
    bool    enabled = m_model->item(idx.row(), 4)->data(Qt::UserRole + 1).toBool();
    if (AptSourceTool::setEnabled(path, !enabled)) refresh();
}

void AptSourcePage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
