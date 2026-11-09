#pragma once
#include <QByteArray>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

// A faithful freedesktop.org Desktop Entry (.desktop) reader/writer.
//
// Why not QSettings? Its IniFormat is NOT a desktop-file parser, and using it
// shipped real bugs (26.12):
//   • ';' starts a comment → "Categories=Graphics;2DGraphics;" read as "Graphics"
//   • a quoted Exec is re-joined → `"/x/app.AppImage" %F` became `/x/app.AppImage%F`,
//     hiding every such AppImage from the list
//   • ',' turns a value into a list → a Name with a comma read as empty
//   • writing percent-encodes the group name ("[Desktop%20Entry]")
// This class keeps every group and key in file order, with values RAW (escape
// sequences preserved) so a file can be round-tripped byte-for-byte; value()
// returns the unescaped string.
class DesktopEntry {
public:
    struct Group {
        QString name;
        QVector<QPair<QString, QString>> entries;   // key → raw value, in file order
    };

    static DesktopEntry read(const QString &path);
    static DesktopEntry parse(const QByteArray &data);

    bool isValid() const;            // has a [Desktop Entry] group
    QVector<Group> &groups() { return m_groups; }
    const QVector<Group> &groups() const { return m_groups; }

    // Unescaped value (\s \n \t \r \\ decoded); empty if absent.
    QString value(const QString &key, const QString &group = QStringLiteral("Desktop Entry")) const;
    bool    contains(const QString &key, const QString &group = QStringLiteral("Desktop Entry")) const;
    // Localized lookup: Key[ll_CC], then Key[ll], then Key. `locale` like "ar_MA".
    QString localized(const QString &key, const QString &locale,
                      const QString &group = QStringLiteral("Desktop Entry")) const;

    // Set a key to an (unescaped) value — escaped on write. Appends if absent.
    void set(const QString &key, const QString &value,
             const QString &group = QStringLiteral("Desktop Entry"));
    // Remove a key and all its localized variants (Key, Key[xx]).
    void removeKey(const QString &key, const QString &group = QStringLiteral("Desktop Entry"));

    QByteArray serialize() const;

    // ── Exec helpers (spec: "The Exec key") ──
    // Split an (already unescaped) Exec value into argv, honouring double quotes
    // and the \" \` \$ \\ escapes inside them.
    static QStringList splitExec(const QString &exec);
    // Quote one argument for an Exec line (only when needed).
    static QString quoteExecArg(const QString &arg);
    // Index of the real program in an Exec argv — skips `env`, VAR=value
    // assignments and `sh -c`-less interpreter prefixes. -1 if none.
    static int programIndex(const QStringList &argv);

    static QString escapeValue(const QString &v);
    static QString unescapeValue(const QString &raw);

private:
    const Group *group(const QString &name) const;
    Group *group(const QString &name);
    QVector<Group> m_groups;
};
