#include "startup_add_dialog.h"
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr int ROLE_ENTRY = Qt::UserRole + 1;

QIcon iconForEntry(const StartupEntry &e)
{
    // Icon= can be an icon-theme name OR an absolute file path. Try both.
    if (e.icon.isEmpty()) return QIcon::fromTheme("application-x-executable");
    if (e.icon.startsWith('/')) {
        QIcon ic(e.icon);
        if (!ic.isNull()) return ic;
    }
    QIcon ic = QIcon::fromTheme(e.icon);
    if (!ic.isNull()) return ic;
    return QIcon::fromTheme("application-x-executable");
}
}

StartupAddDialog::StartupAddDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Add Startup Entry"));
    setMinimumSize(540, 460);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 12);

    m_tabs = new QTabWidget;
    buildSystemTab();
    buildManualTab();
    root->addWidget(m_tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *addBtn = new QPushButton(tr("Add"));
    addBtn->setObjectName("primaryButton");
    addBtn->setDefault(true);
    buttons->addButton(addBtn, QDialogButtonBox::AcceptRole);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        if (m_tabs->currentIndex() == 0) onAddFromSystem();
        else                              onAddManual();
    });
}

void StartupAddDialog::buildSystemTab()
{
    auto *page = new QWidget;
    auto *col  = new QVBoxLayout(page);
    col->setContentsMargins(8, 8, 8, 8);
    col->setSpacing(8);

    m_systemSearch = new QLineEdit;
    m_systemSearch->setPlaceholderText(tr("Search installed applications…"));
    m_systemSearch->setClearButtonEnabled(true);
    col->addWidget(m_systemSearch);

    m_systemModel = new QStandardItemModel(this);
    const auto apps = StartupTool::systemApplications();
    for (const auto &e : apps) {
        auto *it = new QStandardItem(iconForEntry(e), e.name);
        QString tip = e.comment.isEmpty() ? e.exec : (e.comment + "\n\n" + e.exec);
        it->setToolTip(tip);
        // Store the entry as variant data (just the bits we need for autostart).
        QVariantMap m;
        m["name"] = e.name; m["exec"] = e.exec;
        m["comment"] = e.comment; m["icon"] = e.icon;
        it->setData(m, ROLE_ENTRY);
        m_systemModel->appendRow(it);
    }
    m_systemModel->sort(0);

    m_systemProxy = new QSortFilterProxyModel(this);
    m_systemProxy->setSourceModel(m_systemModel);
    m_systemProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_systemList = new QListView;
    m_systemList->setModel(m_systemProxy);
    m_systemList->setIconSize({28, 28});
    m_systemList->setUniformItemSizes(true);
    m_systemList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    col->addWidget(m_systemList, 1);

    connect(m_systemSearch, &QLineEdit::textChanged,
            m_systemProxy, &QSortFilterProxyModel::setFilterFixedString);
    // Double-click also accepts the choice.
    connect(m_systemList, &QListView::doubleClicked,
            this, [this](const QModelIndex &) { onAddFromSystem(); });

    m_tabs->addTab(page, tr("From System Apps"));
}

void StartupAddDialog::buildManualTab()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(8, 12, 8, 8);
    form->setSpacing(10);

    m_manualName    = new QLineEdit;
    m_manualName->setPlaceholderText(tr("My Script"));
    m_manualExec    = new QLineEdit;
    m_manualExec->setPlaceholderText(tr("/usr/local/bin/my-script.sh"));
    m_manualComment = new QLineEdit;
    m_manualIcon    = new QLineEdit;
    m_manualIcon->setPlaceholderText(tr("icon-theme name or /path/to/icon.png"));

    auto *iconRow = new QHBoxLayout;
    iconRow->addWidget(m_manualIcon, 1);
    auto *browseIconBtn = new QPushButton(tr("Browse…"));
    iconRow->addWidget(browseIconBtn);
    auto *iconHost = new QWidget; iconHost->setLayout(iconRow);
    iconRow->setContentsMargins(0, 0, 0, 0);

    auto *execRow = new QHBoxLayout;
    execRow->addWidget(m_manualExec, 1);
    auto *browseExecBtn = new QPushButton(tr("Browse…"));
    execRow->addWidget(browseExecBtn);
    auto *execHost = new QWidget; execHost->setLayout(execRow);
    execRow->setContentsMargins(0, 0, 0, 0);

    form->addRow(tr("Name:"),    m_manualName);
    form->addRow(tr("Command:"), execHost);
    form->addRow(tr("Comment:"), m_manualComment);
    form->addRow(tr("Icon:"),    iconHost);

    connect(browseExecBtn, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, tr("Pick an executable"),
            QDir::homePath());
        if (!f.isEmpty()) m_manualExec->setText(f);
    });
    connect(browseIconBtn, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, tr("Pick an icon"),
            QDir::homePath(), tr("Image files (*.png *.svg *.xpm)"));
        if (!f.isEmpty()) m_manualIcon->setText(f);
    });

    m_tabs->addTab(page, tr("Manual Entry"));
}

void StartupAddDialog::onAddFromSystem()
{
    auto idx = m_systemList->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, tr("Pick an application"),
            tr("Select an application from the list first."));
        return;
    }
    const QVariantMap m = idx.data(ROLE_ENTRY).toMap();
    m_result.name    = m.value("name").toString();
    m_result.exec    = m.value("exec").toString();
    m_result.comment = m.value("comment").toString();
    m_result.icon    = m.value("icon").toString();
    m_result.enabled = true;
    accept();
}

void StartupAddDialog::onAddManual()
{
    QString name = m_manualName->text().trimmed();
    QString exec = m_manualExec->text().trimmed();
    if (name.isEmpty() || exec.isEmpty()) {
        QMessageBox::warning(this, tr("Missing fields"),
            tr("Name and Command are required."));
        return;
    }
    m_result.name    = name;
    m_result.exec    = exec;
    m_result.comment = m_manualComment->text().trimmed();
    m_result.icon    = m_manualIcon->text().trimmed();
    m_result.enabled = true;
    accept();
}
