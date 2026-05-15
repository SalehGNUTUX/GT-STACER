#pragma once
#include <QDialog>

// Asked when the user clicks Quit in the tray menu. Mirrors the prompt used
// by Stacer 1.x: "continue running in the tray?" with a remember-my-choice
// checkbox.
//
// Returns:
//   QDialog::Accepted → user picked "Continue" (keep running in tray)
//   QDialog::Rejected → user picked "Quit"
//
// If the user previously ticked "Don't ask again", call shouldShow() first;
// when it returns false, the caller should skip the dialog and act on the
// stored preference directly.
class QuitConfirmDialog : public QDialog {
    Q_OBJECT
public:
    explicit QuitConfirmDialog(QWidget *parent = nullptr);

    // True if the dialog is suppressed by the user's earlier "don't ask".
    static bool shouldShow();

    // Default action when the dialog is suppressed.
    // "continue" or "quit".
    static QString rememberedAction();

private:
    void onContinue();
    void onQuit();

    class QCheckBox *m_dontAsk = nullptr;
};
