#include "desktop_entry.h"
#include <QFile>
#include <QRegularExpression>

DesktopEntry DesktopEntry::read(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    // Desktop files are small; cap the read so a hostile/odd file can't bloat us.
    return parse(f.read(1024 * 1024));
}

DesktopEntry DesktopEntry::parse(const QByteArray &data)
{
    DesktopEntry e;
    Group *cur = nullptr;
    const QString text = QString::fromUtf8(data);
    for (QString line : text.split('\n')) {
        if (line.endsWith('\r')) line.chop(1);
        const QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith('#')) continue;          // comments are dropped
        if (t.startsWith('[') && t.endsWith(']')) {
            e.m_groups.append({t.mid(1, t.size() - 2), {}});
            cur = &e.m_groups.last();
            continue;
        }
        if (!cur) continue;                                       // key before any group
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        // Spec: whitespace around '=' is ignored; the value is everything after.
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();
        if (key.isEmpty()) continue;
        cur->entries.append({key, val});
    }
    return e;
}

bool DesktopEntry::isValid() const { return group(QStringLiteral("Desktop Entry")) != nullptr; }

const DesktopEntry::Group *DesktopEntry::group(const QString &name) const
{
    for (const auto &g : m_groups) if (g.name == name) return &g;
    return nullptr;
}

DesktopEntry::Group *DesktopEntry::group(const QString &name)
{
    for (auto &g : m_groups) if (g.name == name) return &g;
    return nullptr;
}

bool DesktopEntry::contains(const QString &key, const QString &grp) const
{
    if (const Group *g = group(grp))
        for (const auto &kv : g->entries) if (kv.first == key) return true;
    return false;
}

QString DesktopEntry::value(const QString &key, const QString &grp) const
{
    if (const Group *g = group(grp))
        for (const auto &kv : g->entries) if (kv.first == key) return unescapeValue(kv.second);
    return {};
}

QString DesktopEntry::localized(const QString &key, const QString &locale, const QString &grp) const
{
    if (!locale.isEmpty()) {
        QString loc = locale.section('.', 0, 0).section('@', 0, 0);   // drop .UTF-8 / @mod
        if (contains(key + '[' + loc + ']', grp)) return value(key + '[' + loc + ']', grp);
        const QString lang = loc.section('_', 0, 0);
        if (contains(key + '[' + lang + ']', grp)) return value(key + '[' + lang + ']', grp);
    }
    return value(key, grp);
}

void DesktopEntry::set(const QString &key, const QString &v, const QString &grp)
{
    Group *g = group(grp);
    if (!g) { m_groups.append({grp, {}}); g = &m_groups.last(); }
    for (auto &kv : g->entries)
        if (kv.first == key) { kv.second = escapeValue(v); return; }
    g->entries.append({key, escapeValue(v)});
}

void DesktopEntry::removeKey(const QString &key, const QString &grp)
{
    Group *g = group(grp);
    if (!g) return;
    const QString loc = key + '[';
    for (int i = g->entries.size() - 1; i >= 0; --i) {
        const QString &k = g->entries[i].first;
        if (k == key || k.startsWith(loc)) g->entries.removeAt(i);
    }
}

QByteArray DesktopEntry::serialize() const
{
    QString out;
    for (int i = 0; i < m_groups.size(); ++i) {
        if (i) out += '\n';
        out += '[' + m_groups[i].name + "]\n";
        for (const auto &kv : m_groups[i].entries)
            out += kv.first + '=' + kv.second + '\n';
    }
    return out.toUtf8();
}

QString DesktopEntry::escapeValue(const QString &v)
{
    QString r;
    r.reserve(v.size());
    for (const QChar c : v) {
        if      (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\t') r += "\\t";
        else if (c == '\r') r += "\\r";
        else r += c;
    }
    // A leading space would be trimmed by readers — encode it.
    if (r.startsWith(' ')) r.replace(0, 1, "\\s");
    return r;
}

QString DesktopEntry::unescapeValue(const QString &raw)
{
    QString r;
    r.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        const QChar c = raw[i];
        if (c == '\\' && i + 1 < raw.size()) {
            const QChar n = raw[++i];
            if      (n == 's')  r += ' ';
            else if (n == 'n')  r += '\n';
            else if (n == 't')  r += '\t';
            else if (n == 'r')  r += '\r';
            else if (n == '\\') r += '\\';
            else { r += '\\'; r += n; }       // e.g. "\;" in lists — keep as-is
        } else {
            r += c;
        }
    }
    return r;
}

QStringList DesktopEntry::splitExec(const QString &exec)
{
    QStringList argv;
    QString cur;
    bool inQuotes = false, have = false;
    for (int i = 0; i < exec.size(); ++i) {
        const QChar c = exec[i];
        if (inQuotes) {
            if (c == '\\' && i + 1 < exec.size()) {
                const QChar n = exec[i + 1];
                if (n == '"' || n == '`' || n == '$' || n == '\\') { cur += n; ++i; continue; }
            }
            if (c == '"') { inQuotes = false; continue; }
            cur += c;
        } else if (c == '"') {
            inQuotes = true; have = true;
        } else if (c == ' ' || c == '\t') {
            if (have || !cur.isEmpty()) { argv << cur; cur.clear(); have = false; }
        } else {
            cur += c;
        }
    }
    if (have || !cur.isEmpty()) argv << cur;
    return argv;
}

QString DesktopEntry::quoteExecArg(const QString &arg)
{
    // Field codes (%U, %f …) must stay bare.
    static const QRegularExpression fieldCode("^%[fFuUdDnNickvm]$");
    if (fieldCode.match(arg).hasMatch()) return arg;
    static const QRegularExpression reserved("[\\s\"'\\\\><~|&;$*?#()`]");
    if (!arg.isEmpty() && !reserved.match(arg).hasMatch()) return arg;
    QString q = arg;
    q.replace('\\', "\\\\").replace('"', "\\\"").replace('`', "\\`").replace('$', "\\$");
    return '"' + q + '"';
}

int DesktopEntry::programIndex(const QStringList &argv)
{
    static const QRegularExpression assign("^[A-Za-z_][A-Za-z0-9_]*=");
    for (int i = 0; i < argv.size(); ++i) {
        const QString &t = argv[i];
        if (t == "env" || t == "/usr/bin/env" || t == "/bin/env") continue;
        if (assign.match(t).hasMatch()) continue;               // VAR=value
        if (t.startsWith('%')) continue;
        return i;
    }
    return -1;
}
