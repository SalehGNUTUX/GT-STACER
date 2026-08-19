#include "language_util.h"
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>

namespace LangUtil {

const QVector<Entry> &languages()
{
    static const QVector<Entry> kLangs = {
        {"en",    "English",       "🇬🇧"},
        {"ar",    "العربية",       "🇲🇦"},   // ar_MA — Western numerals, RTL
        {"de",    "Deutsch",       "🇩🇪"},
        {"fr",    "Français",      "🇫🇷"},
        {"hi",    "हिन्दी",         "🇮🇳"},
        {"it",    "Italiano",      "🇮🇹"},
        {"kn",    "ಕನ್ನಡ",         "🇮🇳"},
        {"ml",    "മലയാളം",        "🇮🇳"},
        {"nl",    "Nederlands",    "🇳🇱"},
        {"oc",    "Occitan",       "🏳️"},
        {"pl",    "Polski",        "🇵🇱"},
        {"pt",    "Português",     "🇧🇷"},
        {"ru",    "Русский",       "🇷🇺"},
        {"sv",    "Svenska",       "🇸🇪"},
        {"tr",    "Türkçe",        "🇹🇷"},
        {"uk",    "Українська",    "🇺🇦"},
        {"vi",    "Tiếng Việt",    "🇻🇳"},
        {"zh_CN", "简体中文",       "🇨🇳"},
        {"zh_TW", "繁體中文",       "🇹🇼"},
    };
    return kLangs;
}

static QString pickEmojiFont()
{
    static const QString chosen = [] {
        const QStringList prefer = {"Noto Color Emoji", "Twemoji", "EmojiOne Color",
                                    "Segoe UI Emoji", "Apple Color Emoji"};
        const QStringList fams = QFontDatabase::families();
        for (const QString &p : prefer)
            if (fams.contains(p)) return p;
        for (const QString &f : fams)
            if (f.contains("Emoji", Qt::CaseInsensitive)) return f;
        return QString();
    }();
    return chosen;
}

QIcon flagIcon(const QString &emoji)
{
    const QString font = pickEmojiFont();
    if (font.isEmpty()) return QIcon();
    const int w = 24, h = 18, scale = 2;
    QPixmap pm(w * scale, h * scale);
    pm.setDevicePixelRatio(scale);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f(font);
    f.setPixelSize(h);
    p.setFont(f);
    p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, emoji);
    p.end();
    return QIcon(pm);
}

void fillCombo(QComboBox *combo, const QString &autoLabel)
{
    const QString keep = combo->count() ? combo->currentData().toString() : QString("auto");
    const QSignalBlocker block(combo);
    combo->clear();
    combo->setIconSize(QSize(24, 18));

    const QIcon globe = flagIcon(QString::fromUtf8("\xF0\x9F\x8C\x90")); // 🌐
    if (globe.isNull()) combo->addItem(autoLabel, "auto");
    else                combo->addItem(globe, autoLabel, "auto");

    for (const Entry &lang : languages()) {
        const QIcon ic = flagIcon(lang.flag);
        if (ic.isNull()) combo->addItem(lang.flag + "  " + lang.label, lang.code);
        else             combo->addItem(ic, lang.label, lang.code);
    }

    const int idx = combo->findData(keep);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

} // namespace LangUtil
