#include "system_cleaner_page.h"
#include "ui_system_cleaner_page.h"
#include "../../Widgets/loading_overlay.h"
#include "../../../gt-stacer-core/Utils/file_util.h"
#include "../../../gt-stacer-core/Utils/format_util.h"
#include "../../../gt-stacer-core/Tools/package_tool.h"
#include <QStandardPaths>
#include <QDir>
#include <QHeaderView>
#include <QResizeEvent>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

SystemCleanerPage::SystemCleanerPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::SystemCleanerPage)
{
    ui->setupUi(this);

    m_overlay = new LoadingOverlay(this);

    QString home = QDir::homePath();
    m_categories = {
        {tr("Crash Reports"),   "/var/crash",              true,  tr("System crash logs")},
        {tr("System Logs"),     "/var/log",                false, tr("Log files in /var/log")},
        {tr("App Cache"),       home + "/.cache",          false, tr("Application cache files")},
        {tr("Thumbnails"),      home + "/.cache/thumbnails",true, tr("Image thumbnail cache")},
        {tr("Trash"),           home + "/.local/share/Trash/files", true, tr("Trash bin contents")},
        {tr("APT Cache"),       "/var/cache/apt/archives", false, tr("Downloaded .deb packages")},
        {tr("Old Kernels"),     "",                        false, tr("Remove unused kernel packages")},
    };

    m_model = new QStandardItemModel(0, 3, this);
    m_model->setHorizontalHeaderLabels({tr("Category"), tr("Size"), tr("Description")});
    ui->cleanerTable->setModel(m_model);
    ui->cleanerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->cleanerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // أعمدة: الفئة (ثابتة) · الحجم (ثابتة) · الوصف (ممتدة)
    auto *hdr = ui->cleanerTable->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->cleanerTable->verticalHeader()->hide();

    connect(ui->scanButton,  &QPushButton::clicked, this, &SystemCleanerPage::scan);
    connect(ui->cleanButton, &QPushButton::clicked, this, &SystemCleanerPage::clean);
    ui->cleanButton->setEnabled(false);
}

SystemCleanerPage::~SystemCleanerPage() { delete ui; }

void SystemCleanerPage::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_overlay) m_overlay->resize(size());
}

void SystemCleanerPage::scan()
{
    ui->scanButton->setEnabled(false);
    ui->cleanButton->setEnabled(false);
    ui->statusLabel->setText(tr("Scanning..."));
    m_model->setRowCount(0);
    m_overlay->start(tr("Scanning your system..."));

    // فحص في خلفية منفصلة لعدم تجميد الواجهة
    auto *watcher = new QFutureWatcher<QVector<QPair<qint64,int>>>(this);
    connect(watcher, &QFutureWatcher<QVector<QPair<qint64,int>>>::finished, this, [this, watcher]{
        watcher->deleteLater();
        m_overlay->stop();

        auto results = watcher->result();
        qint64 total = 0;
        for (int i = 0; i < results.size(); ++i) {
            qint64 size = results[i].first;
            total += size;
            QList<QStandardItem*> row;
            auto *nameItem = new QStandardItem(m_categories[i].name);
            auto *sizeItem = new QStandardItem(FormatUtil::formatBytes(size));
            auto *descItem = new QStandardItem(m_categories[i].description);
            // لون مختلف حسب الحجم
            if (size > 500LL * 1024 * 1024)
                sizeItem->setForeground(QColor("#f38ba8"));
            else if (size > 50LL * 1024 * 1024)
                sizeItem->setForeground(QColor("#f9e2af"));
            row << nameItem << sizeItem << descItem;
            m_model->appendRow(row);
        }
        ui->statusLabel->setText(tr("Found %1 to clean").arg(FormatUtil::formatBytes(total)));
        ui->cleanButton->setEnabled(true);
        ui->scanButton->setEnabled(true);
    });

    auto categories = m_categories;
    watcher->setFuture(QtConcurrent::run([categories]() {
        QVector<QPair<qint64,int>> results;
        for (int i = 0; i < categories.size(); ++i)
            results << qMakePair(categories[i].path.isEmpty() ? 0LL
                                 : FileUtil::dirSize(categories[i].path), i);
        return results;
    }));
}

void SystemCleanerPage::cleanCategory(const CleanCategory &cat)
{
    if (cat.path.isEmpty()) return;
    auto files = FileUtil::dirFiles(cat.path, cat.recursive);
    for (const auto &f : files)
        FileUtil::removeFile(f);
}

void SystemCleanerPage::clean()
{
    ui->cleanButton->setEnabled(false);
    ui->statusLabel->setText(tr("Cleaning..."));
    m_overlay->start(tr("Cleaning your system..."));

    auto selected = ui->cleanerTable->selectionModel()->selectedRows();
    QVector<CleanCategory> toClean;
    if (selected.isEmpty()) {
        toClean = m_categories;
    } else {
        for (const auto &idx : selected)
            toClean << m_categories[idx.row()];
    }

    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]{
        watcher->deleteLater();
        PackageTool::cleanCache();
        m_overlay->stop();
        ui->statusLabel->setText(tr("Cleaning complete!"));
        scan();
    });

    watcher->setFuture(QtConcurrent::run([this, toClean]() {
        for (const auto &cat : toClean)
            const_cast<SystemCleanerPage*>(this)->cleanCategory(cat);
    }));
}

qint64 SystemCleanerPage::calcCategorySize(const CleanCategory &cat)
{
    return FileUtil::dirSize(cat.path);
}

void SystemCleanerPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && ui)
        ui->retranslateUi(this);
    QWidget::changeEvent(event);
}
