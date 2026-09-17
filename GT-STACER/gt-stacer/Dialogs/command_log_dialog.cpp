#include "command_log_dialog.h"
#include "../../gt-stacer-core/Utils/command_util.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QScrollBar>

CommandLogDialog::CommandLogDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    resize(720, 440);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *head = new QLabel(title);
    head->setObjectName("pageTitle");
    root->addWidget(head);

    m_status = new QLabel(tr("Preparing…"));
    m_status->setObjectName("introText");
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    // Log output is left-to-right regardless of UI language.
    m_log->setLayoutDirection(Qt::LeftToRight);
    root->addWidget(m_log, 1);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 0);        // busy/indeterminate while a step runs
    m_bar->setTextVisible(false);
    root->addWidget(m_bar);

    auto *btnRow = new QHBoxLayout;
    m_stop = new QPushButton(tr("Stop"));
    m_stop->setObjectName("dangerButton");
    m_close = new QPushButton(tr("Close"));
    m_close->setObjectName("primaryButton");
    m_close->setEnabled(false);
    btnRow->addWidget(m_stop);
    btnRow->addStretch();
    btnRow->addWidget(m_close);
    root->addLayout(btnRow);

    connect(m_stop,  &QPushButton::clicked, this, &CommandLogDialog::stopRequested);
    connect(m_close, &QPushButton::clicked, this, &QDialog::accept);
}

void CommandLogDialog::addStep(const QString &label, const QStringList &argv)
{
    m_steps.append({label, argv});
}

void CommandLogDialog::run()
{
    if (m_running || m_steps.isEmpty()) { if (m_steps.isEmpty()) finishAll(false); return; }
    m_running = true;
    startNext();
}

void CommandLogDialog::appendLine(const QString &text)
{
    m_log->appendPlainText(text);
    if (auto *sb = m_log->verticalScrollBar()) sb->setValue(sb->maximum());
}

void CommandLogDialog::startNext()
{
    if (m_cancelled) { finishAll(false); return; }
    if (m_idx >= m_steps.size()) { finishAll(m_allOk); return; }

    const Step step = m_steps.at(m_idx);
    m_status->setText(step.label);
    if (m_steps.size() > 1)
        appendLine(QString("\n==> [%1/%2] %3").arg(m_idx + 1).arg(m_steps.size()).arg(step.label));
    else
        appendLine(QString("==> %1").arg(step.label));

    if (step.argv.isEmpty()) {           // nothing to run for this step — skip
        appendLine(tr("  (skipped)"));
        ++m_idx;
        startNext();
        return;
    }

    QString prog = step.argv.first();
    QStringList args = step.argv.mid(1);
    CommandUtil::wrapForHost(prog, args);   // no-op outside Flatpak
    appendLine("$ " + prog + " " + args.join(' '));

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]{
        const QString out = QString::fromUtf8(m_proc->readAllStandardOutput());
        for (const QString &line : out.split('\n')) {
            if (line.isEmpty()) continue;
            appendLine(line);
        }
    });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus st) {
        const bool ok = (st == QProcess::NormalExit && code == 0);
        if (!ok) {
            m_allOk = false;
            if (m_cancelled) appendLine(tr("  ⏹ Stopped."));
            else appendLine(tr("  ✗ Command failed (exit %1).").arg(code));
        } else {
            appendLine(tr("  ✓ Done."));
        }
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_cancelled) { finishAll(false); return; }
        // Stop the whole batch on the first failure so a broken step doesn't
        // cascade (e.g. a failed upgrade shouldn't trigger the next one).
        if (!ok) { finishAll(false); return; }
        ++m_idx;
        startNext();
    });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError){
        if (m_proc) appendLine(tr("  ✗ %1").arg(m_proc->errorString()));
    });

    m_proc->start(prog, args);
}

void CommandLogDialog::stopRequested()
{
    if (!m_running || m_cancelled) { reject(); return; }
    m_cancelled = true;
    m_status->setText(tr("Stopping…"));
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        appendLine(tr("  ⏹ Stopping… (a privileged step already running as root may "
                      "finish in the background)"));
        m_proc->terminate();
        if (!m_proc->waitForFinished(2000)) m_proc->kill();
    } else {
        finishAll(false);
    }
}

void CommandLogDialog::finishAll(bool ok)
{
    m_running = false;
    m_bar->setRange(0, 1);
    m_bar->setValue(1);
    m_stop->setEnabled(false);
    m_close->setEnabled(true);
    m_close->setDefault(true);
    m_close->setFocus();
    if (m_cancelled)   m_status->setText(tr("Stopped."));
    else if (ok)       m_status->setText(tr("✓ Finished successfully."));
    else               m_status->setText(tr("✗ Finished with errors — see the log above."));
    emit completed(ok);
}

void CommandLogDialog::closeEvent(QCloseEvent *e)
{
    // Don't let the window close mid-operation; require Stop first.
    if (m_running && !m_cancelled) { stopRequested(); e->ignore(); return; }
    QDialog::closeEvent(e);
}
