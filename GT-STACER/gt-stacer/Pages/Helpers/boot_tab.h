#pragma once
#include <QWidget>
#include <QMap>
#include "../../../gt-stacer-core/Tools/grub_tool.h"

class QLabel;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QGroupBox;

// Helpers → "Boot (GRUB)" (26.12): the native replacement for GT-GMT.
// Shows how this computer boots, edits the GRUB settings safely (only the
// keys changed; validated; previewed), and runs every change as one
// transaction with an automatic restore point and rollback — see GrubTool.
// Other boot loaders (systemd-boot, rEFInd) are shown, not edited.
class BootTab : public QWidget {
    Q_OBJECT
public:
    explicit BootTab(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslate();
    void reload();                         // re-read Info + Settings into the form
    QMap<QString, QString> pendingChanges() const;
    void loadMenuEntries();
    void installTheme();
    void applyChanges();
    void regenerate();
    void showRestorePoints();
    void reinstall();
    void runApply(const QString &newText, const QString &what,
                  const QString &themeDir = QString(), const QString &themeName = QString(),
                  bool updateBls = false);
    void setBusy(bool busy);
    void updateOverrides();

    GrubTool::Info     m_info;
    GrubTool::Settings m_settings;
    bool               m_busy = false;
    bool               m_editable = false;   // GRUB in use, grub-mkconfig present AND /etc/default/grub was read

    QLabel      *m_intro       = nullptr;
    QGroupBox   *m_sysBox      = nullptr;
    QLabel      *m_sysText     = nullptr;
    QGroupBox   *m_setBox      = nullptr;
    QLabel      *m_lblTimeout = nullptr, *m_lblStyle = nullptr, *m_lblDefault = nullptr,
                *m_lblRes = nullptr, *m_lblParams = nullptr, *m_lblTheme = nullptr;
    QSpinBox    *m_timeout     = nullptr;
    QComboBox   *m_style       = nullptr;
    QComboBox   *m_default     = nullptr;
    QPushButton *m_loadEntries = nullptr;
    QCheckBox   *m_osProber    = nullptr;
    QCheckBox   *m_recovery    = nullptr;
    QComboBox   *m_gfx         = nullptr;
    QLineEdit   *m_params      = nullptr;
    QComboBox   *m_theme       = nullptr;
    QPushButton *m_installTheme = nullptr;
    QLabel      *m_overrides   = nullptr;
    QPushButton *m_applyBtn    = nullptr;
    QPushButton *m_revertBtn   = nullptr;
    QGroupBox   *m_maintBox    = nullptr;
    QPushButton *m_regenBtn    = nullptr;
    QPushButton *m_restoreBtn  = nullptr;
    QPushButton *m_reinstallBtn = nullptr;
    QLabel      *m_regenHint   = nullptr;
    QLabel      *m_status      = nullptr;

    // A theme picked with "Install theme…" waiting to be applied.
    QString m_stagedThemeDir;
    QString m_stagedThemeName;
};
