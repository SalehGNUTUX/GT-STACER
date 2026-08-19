#pragma once
#include <QDialog>

class QComboBox;
class QLabel;
class QVBoxLayout;
class QPushButton;

// Shown once after the app is updated to a new version (and re-openable from
// Settings). Lists the current release's highlights in the app language, with an
// in-dialog language picker. The highlight text is baked in and must be refreshed
// each release — see highlights() in the .cpp.
class WhatsNewDialog : public QDialog {
    Q_OBJECT
public:
    explicit WhatsNewDialog(QWidget *parent = nullptr);

private:
    void retranslate();

    QComboBox   *m_lang     = nullptr;
    QLabel      *m_title    = nullptr;
    QLabel      *m_sub      = nullptr;
    QLabel      *m_link     = nullptr;
    QPushButton *m_ok       = nullptr;
    QVBoxLayout *m_listLay  = nullptr;   // holds the highlight rows
};
