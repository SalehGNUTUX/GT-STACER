#pragma once
#include <QDialog>
#include <QVector>
#include <QStringList>
#include <QProcess>

class QPlainTextEdit;
class QProgressBar;
class QLabel;
class QPushButton;

// A live progress window for long-running package operations (install / upgrade
// / remove). It runs one or more commands in sequence, streaming their combined
// stdout+stderr into a scrolling log, and offers a real Stop button — the missing
// piece the old spinner overlay had no room for.
//
// Each step's argv is {program, arg…} exactly as PackageTool's *Command()
// builders return it (pkexec already prepended when root is needed), so no shell
// is involved. Inside Flatpak the argv is transparently host-wrapped.
//
// Cancellation note: Stop kills the process we spawned. A user-level command
// (flatpak/snap/pip…) dies immediately; for a pkexec command, Stop cancels before
// authorization and stops the UI wait, but a privileged step already running as
// root may finish in the background — the log says so.
class CommandLogDialog : public QDialog {
    Q_OBJECT
public:
    explicit CommandLogDialog(const QString &title, QWidget *parent = nullptr);

    // Queue a step. `label` is shown above its output block; `argv` is
    // {program, args…}. Empty argv steps are skipped (with a note).
    void addStep(const QString &label, const QStringList &argv);
    // Begin running the queued steps. Safe to call once, after addStep()s.
    void run();

    bool succeeded() const { return m_allOk && !m_cancelled; }

signals:
    void completed(bool ok);

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void startNext();
    void finishAll(bool ok);
    void appendLine(const QString &text);
    void stopRequested();

    struct Step { QString label; QStringList argv; };
    QVector<Step> m_steps;
    int      m_idx       = 0;
    bool     m_allOk     = true;
    bool     m_cancelled = false;
    bool     m_running   = false;
    QProcess *m_proc     = nullptr;

    QPlainTextEdit *m_log    = nullptr;
    QProgressBar   *m_bar    = nullptr;
    QLabel         *m_status = nullptr;
    QPushButton    *m_stop   = nullptr;
    QPushButton    *m_close  = nullptr;
};
