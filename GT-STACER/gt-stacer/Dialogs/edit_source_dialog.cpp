#include "edit_source_dialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>

EditSourceDialog::EditSourceDialog(QWidget *parent)
    : QDialog(parent) { buildUi(); }

EditSourceDialog::EditSourceDialog(const AptSource &src, QWidget *parent)
    : QDialog(parent) { buildUi(src); }

void EditSourceDialog::buildUi(const AptSource &src)
{
    setWindowTitle(src.uri.isEmpty() ? tr("Add APT Source") : tr("Edit APT Source"));
    setMinimumWidth(480);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);

    // Info label
    auto *info = new QLabel(tr("APT source lines define where packages are downloaded from.\n"
                               "Format: deb <URI> <suite> <components>"));
    info->setWordWrap(true);
    info->setObjectName("infoKey");
    root->addWidget(info);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_type = new QComboBox;
    m_type->addItems({"deb", "deb-src"});
    if (!src.type.isEmpty()) m_type->setCurrentText(src.type);
    form->addRow(tr("Type:"), m_type);

    m_uri = new QLineEdit(src.uri);
    m_uri->setPlaceholderText("https://example.com/ubuntu");
    form->addRow(tr("URI:"), m_uri);

    m_suite = new QLineEdit(src.suite);
    m_suite->setPlaceholderText("noble  /  jammy  /  stable");
    form->addRow(tr("Suite:"), m_suite);

    m_components = new QLineEdit(src.components);
    m_components->setPlaceholderText("main restricted universe multiverse");
    form->addRow(tr("Components:"), m_components);

    m_enabled = new QCheckBox(tr("Enabled"));
    m_enabled->setChecked(src.uri.isEmpty() ? true : src.enabled);
    form->addRow("", m_enabled);

    root->addLayout(form);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);
}

AptSource EditSourceDialog::result() const
{
    AptSource s;
    s.type       = m_type->currentText();
    s.uri        = m_uri->text().trimmed();
    s.suite      = m_suite->text().trimmed();
    s.components = m_components->text().trimmed();
    s.enabled    = m_enabled->isChecked();
    return s;
}
