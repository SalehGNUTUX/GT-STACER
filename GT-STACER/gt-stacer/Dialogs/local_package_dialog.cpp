#include "local_package_dialog.h"
#include "../Managers/theme.h"
#include "../../gt-stacer-core/Utils/format_util.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <functional>

namespace {

// The core library carries no translations; its messages are fixed English
// templates. Map each one to a translated string here (fallback: as-is).
QString trCore(const QString &msg)
{
    struct Rule { const char *rx; std::function<QString(const QRegularExpressionMatch &)> f; };
    static const QVector<Rule> rules = {
        {"^The file can't be read\\.$", [](auto &){ return LocalPackageDialog::tr("The file can't be read."); }},
        {"^This file type is not a package GT-STACER can install\\.$",
            [](auto &){ return LocalPackageDialog::tr("This file type is not a package GT-STACER can install."); }},
        {"^(.+) can't be installed on (.+): it is meant for a different family of distributions \\(no matching package manager here\\)\\.$",
            [](auto &m){ return LocalPackageDialog::tr("This package type can't be installed on %1: it is meant for a "
                                                       "different family of distributions (no matching package manager here).")
                                .arg(m.captured(2)); }},
        {"^This package is built for (.+), but this system is (.+)\\.$",
            [](auto &m){ return LocalPackageDialog::tr("This package is built for %1, but this system is %2.")
                                .arg(m.captured(1), m.captured(2)); }},
        {"^This file is not a valid (Debian|RPM|pacman) package",
            [](auto &){ return LocalPackageDialog::tr("This file is not a valid package — it could not be read."); }},
        {"^Reading \\.rpm packages needs the rpm tool",
            [](auto &){ return LocalPackageDialog::tr("Reading .rpm packages needs the rpm tool, which is not installed."); }},
        {"^Not a type-2 AppImage",
            [](auto &){ return LocalPackageDialog::tr("This is not a type-2 AppImage, so it can't be read safely without running it."); }},
        {"^unsquashfs",
            [](auto &){ return LocalPackageDialog::tr("Reading AppImages needs unsquashfs (package squashfs-tools)."); }},
        {"^A copy was staged",
            [](auto &){ return LocalPackageDialog::tr("A copy was placed in ~/.cache/gt-stacer-tmp so the host package manager can read it."); }},
        {"^(\\d+) required dependenc",
            [](auto &m){ return LocalPackageDialog::tr("%1 required dependencies were not found in your repositories — "
                                                       "the installation will probably fail.").arg(m.captured(1)); }},
        {"^Missing required dependencies are downloaded",
            [](auto &){ return LocalPackageDialog::tr("Missing required dependencies are downloaded from your repositories during installation."); }},
        {"^A local snap is not signed",
            [](auto &){ return LocalPackageDialog::tr("A local snap is not signed by the Snap Store: it is installed with --dangerous and will not update automatically."); }},
        {"^The application and any runtime",
            [](auto &){ return LocalPackageDialog::tr("The application and any runtime it needs are downloaded from its Flatpak remote."); }},
        {"^If the bundle needs a runtime",
            [](auto &){ return LocalPackageDialog::tr("If the bundle needs a runtime you don't have, Flatpak downloads it from a configured remote (e.g. Flathub)."); }},
        {"^A local \\.apk is installed",
            [](auto &){ return LocalPackageDialog::tr("A local .apk is installed with --allow-untrusted (its signature is not checked)."); }},
        {"^AppImages are integrated for your user only \\(no password\\): placed in (.+) and added",
            [](auto &m){ return LocalPackageDialog::tr("AppImages are integrated for your user only (no password): placed in %1 and added to the application menu.")
                                .arg(m.captured(1)); }},
    };
    for (const Rule &r : rules) {
        const auto m = QRegularExpression(r.rx).match(msg);
        if (m.hasMatch()) return r.f(m);
    }
    return msg;
}

enum Col { ColName = 0, ColState = 1, ColKind = 2 };
constexpr int RoleName = Qt::UserRole;          // the name to install
constexpr int RoleSelectable = Qt::UserRole + 1;
constexpr int RoleSatisfied = Qt::UserRole + 2;

} // namespace

QString LocalPackageDialog::translatedKind(LocalPkgKind k)
{
    switch (k) {
    case LocalPkgKind::Deb:           return tr("Debian package (.deb)");
    case LocalPkgKind::Rpm:           return tr("RPM package (.rpm)");
    case LocalPkgKind::Pacman:        return tr("Arch package (.pkg.tar.zst)");
    case LocalPkgKind::Apk:           return tr("Alpine package (.apk)");
    case LocalPkgKind::Eopkg:         return tr("Solus package (.eopkg)");
    case LocalPkgKind::FlatpakBundle: return tr("Flatpak bundle (.flatpak)");
    case LocalPkgKind::FlatpakRef:    return tr("Flatpak reference (.flatpakref)");
    case LocalPkgKind::Snap:          return tr("Snap package (.snap)");
    case LocalPkgKind::AppImage:      return tr("AppImage");
    default:                          return tr("Unknown file type");
    }
}

LocalPackageDialog::LocalPackageDialog(const LocalPackageInfo &info, QWidget *parent)
    : QDialog(parent), m_info(info)
{
    setWindowTitle(tr("Install local package"));
    setMinimumSize(620, 480);
    auto *v = new QVBoxLayout(this);
    v->setSpacing(10);

    auto *name = new QLabel(m_info.name.isEmpty() ? QFileInfo(m_info.path).fileName() : m_info.name);
    name->setObjectName("pkgName");
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(name);
    if (!m_info.summary.isEmpty()) {
        auto *sum = new QLabel(m_info.summary);
        sum->setObjectName("introText");
        sum->setWordWrap(true);
        v->addWidget(sum);
    }

    // ── Facts ──
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeading | Qt::AlignVCenter);
    auto row = [&](const QString &label, const QString &value) {
        if (value.isEmpty()) return;
        auto *k = new QLabel(label);   k->setObjectName("infoKey");
        auto *val = new QLabel(value); val->setObjectName("infoValue");
        val->setWordWrap(true);
        val->setTextInteractionFlags(Qt::TextBrowserInteraction);
        val->setOpenExternalLinks(true);
        form->addRow(k, val);
    };
    row(tr("Type"), translatedKind(m_info.kind));
    row(tr("Version"), m_info.version);
    row(tr("Architecture"), m_info.arch);
    if (!m_info.installedVersion.isEmpty())
        row(tr("Installed now"), m_info.installedVersion == m_info.version
            ? tr("%1 (the same version — it will be reinstalled)").arg(m_info.installedVersion)
            : tr("%1 (it will be replaced by %2)").arg(m_info.installedVersion, m_info.version));
    row(tr("File size"), m_info.fileSize > 0 ? FormatUtil::formatBytes(m_info.fileSize) : QString());
    row(tr("Installed size"), m_info.installedSize > 0 ? FormatUtil::formatBytes(m_info.installedSize) : QString());
    row(tr("Maintainer"), m_info.maintainer.toHtmlEscaped());
    if (m_info.homepage.startsWith("http"))
        row(tr("Homepage"), QString("<a href=\"%1\">%1</a>").arg(m_info.homepage.toHtmlEscaped()));
    row(tr("License"), m_info.license);
    row(tr("File"), m_info.path.toHtmlEscaped());
    v->addLayout(form);

    if (!m_info.description.isEmpty()) {
        auto *desc = new QPlainTextEdit(m_info.description);
        desc->setReadOnly(true);
        desc->setMaximumHeight(90);
        v->addWidget(desc);
    }

    // ── Blocking problem / notes ──
    if (!m_info.problem.isEmpty()) {
        auto *p = new QLabel(trCore(m_info.problem));
        p->setObjectName("pkgProblem");
        p->setWordWrap(true);
        v->addWidget(p);
    }
    for (const QString &w : m_info.warnings) {
        auto *l = new QLabel(trCore(w));
        l->setObjectName("pkgWarning");
        l->setWordWrap(true);
        v->addWidget(l);
    }

    // ── Dependencies ──
    if (m_info.installable && m_info.depsChecked && !m_info.deps.isEmpty()) {
        auto *hdr = new QLabel(tr("Dependencies"));
        hdr->setObjectName("netTitle");
        v->addWidget(hdr);
        m_depSummary = new QLabel;
        m_depSummary->setWordWrap(true);
        m_depSummary->setObjectName("infoValue");
        v->addWidget(m_depSummary);

        m_deps = new QTreeWidget;
        m_deps->setColumnCount(3);
        m_deps->setHeaderLabels({tr("Dependency"), tr("Status"), tr("Kind")});
        m_deps->setRootIsDecorated(false);
        m_deps->setAlternatingRowColors(true);
        m_deps->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
        m_deps->header()->setSectionResizeMode(ColState, QHeaderView::ResizeToContents);
        m_deps->header()->setSectionResizeMode(ColKind, QHeaderView::ResizeToContents);
        v->addWidget(m_deps, 1);

        auto *opts = new QHBoxLayout;
        m_allOptional = new QCheckBox(tr("Select all optional dependencies"));
        m_showSatisfied = new QCheckBox(tr("Show already-installed dependencies"));
        opts->addWidget(m_allOptional);
        opts->addStretch(1);
        opts->addWidget(m_showSatisfied);
        v->addLayout(opts);

        fillDependencies();
        connect(m_showSatisfied, &QCheckBox::toggled, this, &LocalPackageDialog::updateVisibility);
        connect(m_allOptional, &QCheckBox::toggled, this, [this](bool on) {
            for (int i = 0; i < m_deps->topLevelItemCount(); ++i) {
                auto *it = m_deps->topLevelItem(i);
                if (it->data(ColName, RoleSelectable).toBool())
                    it->setCheckState(ColName, on ? Qt::Checked : Qt::Unchecked);
            }
        });
    } else if (m_info.installable && !m_info.depsChecked
               && (m_info.kind == LocalPkgKind::Deb || m_info.kind == LocalPkgKind::Rpm
                   || m_info.kind == LocalPkgKind::Pacman)) {
        auto *l = new QLabel(tr("Dependencies could not be checked — the package manager will resolve them during installation."));
        l->setObjectName("pkgWarning");
        l->setWordWrap(true);
        v->addWidget(l);
    } else {
        v->addStretch(1);
    }

    // ── Buttons ──
    auto *bb = new QDialogButtonBox;
    auto *install = bb->addButton(m_info.kind == LocalPkgKind::AppImage ? tr("Integrate") : tr("Install"),
                                  QDialogButtonBox::AcceptRole);
    install->setObjectName("primaryButton");
    install->setEnabled(m_info.installable);
    if (!m_info.installable) install->setToolTip(tr("This package can't be installed on this system."));
    bb->addButton(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);
}

void LocalPackageDialog::fillDependencies()
{
    int req = 0, reqMissing = 0, unavailable = 0, optional = 0, optInstallable = 0, satisfied = 0;
    for (const LocalDependency &d : m_info.deps) {
        auto *it = new QTreeWidgetItem;
        it->setText(ColName, d.declared.isEmpty() ? d.name : d.declared);
        it->setToolTip(ColName, d.note.isEmpty() || d.note == "recommended" || d.note == "suggested"
                                    ? d.declared : d.declared + "\n" + d.note);
        it->setData(ColName, RoleName, d.name);

        QString state;
        QColor color = Theme::text();
        switch (d.state) {
        case LocalDependency::State::Satisfied:   state = tr("Installed");                 color = Theme::green();  ++satisfied; break;
        case LocalDependency::State::Installable: state = tr("Will be installed");         color = Theme::blue();   break;
        case LocalDependency::State::Unavailable: state = tr("Not in your repositories");  color = Theme::red();    ++unavailable; break;
        case LocalDependency::State::Missing:     state = tr("Missing — resolved during install"); color = Theme::yellow(); break;
        }
        if (!d.required && d.state != LocalDependency::State::Satisfied) {
            // Optional: only "will be installed" ones are offered as a choice.
            state = d.state == LocalDependency::State::Installable ? tr("Available")
                  : d.state == LocalDependency::State::Missing     ? tr("Not installed")
                                                                   : state;
        }
        it->setText(ColState, state);
        it->setForeground(ColState, color);
        it->setData(ColName, RoleSatisfied, d.state == LocalDependency::State::Satisfied);

        QString kind = d.required ? tr("Required")
                     : d.note == "recommended" ? tr("Recommended")
                     : d.note == "suggested"   ? tr("Suggested")
                                               : tr("Optional");
        it->setText(ColKind, kind);

        if (d.required) {
            ++req;
            if (d.state != LocalDependency::State::Satisfied) {
                ++reqMissing;
                // Shown ticked and locked: the package can't work without it, and
                // the package manager installs it automatically.
                it->setCheckState(ColName, Qt::Checked);
                it->setFlags(it->flags() & ~Qt::ItemIsUserCheckable);   // visible, not toggleable
                it->setToolTip(ColState, tr("Required — installed automatically by the package manager."));
            }
        } else {
            ++optional;
            const bool selectable = !d.name.isEmpty() && d.state != LocalDependency::State::Satisfied
                                    && d.state != LocalDependency::State::Unavailable;
            it->setData(ColName, RoleSelectable, selectable);
            if (selectable) {
                ++optInstallable;
                it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
                // Same default as the package manager: recommended on, suggested off.
                it->setCheckState(ColName, d.note == "recommended" ? Qt::Checked : Qt::Unchecked);
            }
        }
        m_deps->addTopLevelItem(it);
    }

    QStringList parts;
    parts << tr("%1 required (%2 to install)").arg(req).arg(reqMissing);
    parts << tr("%1 optional (%2 can be added)").arg(optional).arg(optInstallable);
    if (satisfied) parts << tr("%1 already installed").arg(satisfied);
    if (unavailable) parts << tr("%1 not available").arg(unavailable);
    m_depSummary->setText(parts.join(QStringLiteral(" · ")) + "\n"
        + tr("Required dependencies are installed automatically. Tick the optional ones you also want."));
    m_allOptional->setEnabled(optInstallable > 0);
    // Few deps → show everything; otherwise hide what's already installed.
    m_showSatisfied->setChecked(m_info.deps.size() <= 8);
    updateVisibility();
}

void LocalPackageDialog::updateVisibility()
{
    const bool show = m_showSatisfied->isChecked();
    for (int i = 0; i < m_deps->topLevelItemCount(); ++i) {
        auto *it = m_deps->topLevelItem(i);
        it->setHidden(!show && it->data(ColName, RoleSatisfied).toBool());
    }
}

QStringList LocalPackageDialog::selectedOptional() const
{
    QStringList out;
    if (!m_deps) return out;
    for (int i = 0; i < m_deps->topLevelItemCount(); ++i) {
        auto *it = m_deps->topLevelItem(i);
        if (it->data(ColName, RoleSelectable).toBool() && it->checkState(ColName) == Qt::Checked)
            out << it->data(ColName, RoleName).toString();
    }
    return out;
}
