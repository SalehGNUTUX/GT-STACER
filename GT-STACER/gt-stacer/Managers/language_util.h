#pragma once
#include <QIcon>
#include <QString>
#include <QVector>

class QComboBox;

// Shared language list + flag rendering + combo population, used by the Settings
// page and the Welcome / What's-new dialogs so they all offer the same picker.
namespace LangUtil {

struct Entry {
    QString code;   // "en", "ar", …
    QString label;  // native name
    QString flag;   // emoji
};

const QVector<Entry> &languages();

// Render a flag emoji as an icon via a colour-emoji font (so flags show on KDE,
// not only GNOME). Null icon when no emoji font exists — caller falls back to text.
QIcon flagIcon(const QString &emoji);

// Fill `combo` with an "Auto (system language)" entry first, then every language,
// each carrying its code in Qt::UserRole. Preserves the current selection (by
// code) across a refresh. `autoLabel` is passed in so it can be translated by the
// caller's context.
void fillCombo(QComboBox *combo, const QString &autoLabel);

} // namespace LangUtil
