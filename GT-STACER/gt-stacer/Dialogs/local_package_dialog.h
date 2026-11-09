#pragma once
#include <QDialog>
#include <QStringList>
#include "../../gt-stacer-core/Tools/local_package_tool.h"

class QTreeWidget;
class QCheckBox;
class QLabel;

// Shows what a local package file is, whether this system can install it, and
// (for .deb / .rpm / pacman packages) each dependency with its state:
//   • required ones are listed for information — the package manager installs
//     the missing ones automatically (skipping a required dependency would leave
//     the package broken, so they are not optional);
//   • optional ones (Recommends / Suggests / pacman optdepends) can be ticked —
//     all, some, or none — and only the ticked ones are installed.
// Accept = install; the caller reads selectedOptional().
class LocalPackageDialog : public QDialog {
    Q_OBJECT
public:
    explicit LocalPackageDialog(const LocalPackageInfo &info, QWidget *parent = nullptr);

    QStringList selectedOptional() const;

private:
    void fillDependencies();
    void updateVisibility();
    static QString translatedKind(LocalPkgKind k);

    LocalPackageInfo m_info;
    QTreeWidget *m_deps         = nullptr;
    QCheckBox   *m_showSatisfied = nullptr;
    QCheckBox   *m_allOptional   = nullptr;
    QLabel      *m_depSummary    = nullptr;
};
