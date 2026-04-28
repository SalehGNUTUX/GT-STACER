#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include "../../gt-stacer-core/Tools/apt_source_tool.h"

class EditSourceDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditSourceDialog(QWidget *parent = nullptr);
    explicit EditSourceDialog(const AptSource &source, QWidget *parent = nullptr);

    AptSource result() const;

private:
    void buildUi(const AptSource &src = {});

    QComboBox *m_type       = nullptr;
    QLineEdit *m_uri        = nullptr;
    QLineEdit *m_suite      = nullptr;
    QLineEdit *m_components = nullptr;
    QCheckBox *m_enabled    = nullptr;
};
