#pragma once
#include <QGuiApplication>
#include <QRegularExpression>
#include <QPair>
#include <QString>
#include <QVector>

// Right-to-left text with left-to-right pieces in it (26.12).
//
// An Arabic sentence that contains a command, a path or an English name —
// "اختر وضع RSYNC … (sudo apt install snapper، ثمّ: sudo snapper -c root
// create-config /)" — is scrambled by the Unicode bidi algorithm: the slashes,
// dashes, parentheses and dots around the Latin words are "neutral", so they
// attach to the wrong side and the words come out in the wrong order.
//
// Bidi::isolate() wraps every left-to-right run in a Unicode isolate
// (LRI … PDI), so each command / path / name is laid out as one left-to-right
// block inside the right-to-left sentence. Only invisible marks are added; the
// text itself is unchanged. Edge punctuation is decided carefully: a run keeps
// a leading "/ ~ - . @" (paths, options) and a closing ")" "]" only when it
// also contains the opening one — otherwise the bracket belongs to the Arabic
// sentence and stays outside, where it is mirrored correctly.
// Rich text is supported: tags and entities are left untouched.
// It does nothing when the application is laid out left-to-right.
namespace Bidi {

inline bool strongRtl(QChar c)
{
    const auto d = c.direction();
    return d == QChar::DirR || d == QChar::DirAL || d == QChar::DirAN;
}
inline bool strongLtr(QChar c)
{
    const auto d = c.direction();
    return d == QChar::DirL || d == QChar::DirEN;
}

// One stretch without strong-RTL characters and without unmatched brackets:
// isolate the part from its first to its last left-to-right character.
inline QString isolateRun(const QString &seg)
{
    static const QString leadKeep = QStringLiteral("/~-.@(\"'");
    static const QString trailKeep = QStringLiteral("/)]\"'%");
    int first = -1, last = -1;
    for (int k = 0; k < seg.size(); ++k) if (strongLtr(seg[k])) { if (first < 0) first = k; last = k; }
    if (first < 0) return seg;                                // only neutrals: leave as is
    int s = first, e = last + 1;
    // Glued path/option characters on the left ("/dev", "-c", "~/.cache").
    while (s > 0 && leadKeep.contains(seg[s - 1])) --s;
    // Glued closing marks on the right, and a lone " /" (a path argument).
    for (;;) {
        if (e < seg.size() && trailKeep.contains(seg[e])) { ++e; continue; }
        if (e + 1 < seg.size() && seg[e] == ' ' && seg[e + 1] == '/'
            && (e + 2 >= seg.size() || !seg[e + 2].isLetterOrNumber())) { e += 2; continue; }
        break;
    }
    // A bracket opened inside the run is closed inside it too ("BTRFS
    // (subvol=/)" — not "BTRFS (subvol" with "=/)" left outside): the
    // segment is already free of unmatched brackets, so the partner exists.
    for (int k = s; k < e; ++k) {
        if (seg[k] != '(' && seg[k] != '[') continue;
        const QChar close = seg[k] == '(' ? QChar(')') : QChar(']');
        int depth = 0;
        for (int m = k; m < seg.size(); ++m) {
            if (seg[m] == seg[k]) ++depth;
            else if (seg[m] == close && --depth == 0) { if (m + 1 > e) e = m + 1; break; }
        }
    }
    // Brackets at the edges only in pairs.
    auto count = [&](QChar ch) { int c = 0; for (int k = s; k < e; ++k) if (seg[k] == ch) ++c; return c; };
    while (e > s && (seg[e - 1] == ')' || seg[e - 1] == ']')
           && count(seg[e - 1]) > count(seg[e - 1] == ')' ? QChar('(') : QChar('[')))
        --e;
    while (s < e && (seg[s] == '(' || seg[s] == '[')
           && count(seg[s]) > count(seg[s] == '(' ? QChar(')') : QChar(']')))
        ++s;
    return seg.left(s) + QChar(0x2066) + seg.mid(s, e - s) + QChar(0x2069) + seg.mid(e);
}

// A bracket whose partner is outside the stretch (e.g. "(sudo apt … snapper،
// ثمّ: …)") belongs to the right-to-left sentence: split the stretch there.
inline QString isolateSegment(const QString &seg)
{
    QVector<int> open;                       // positions of "(" / "[" not yet closed
    QVector<bool> unmatched(seg.size(), false);
    for (int k = 0; k < seg.size(); ++k) {
        const QChar c = seg[k];
        if (c == '(' || c == '[') open << k;
        else if (c == ')' || c == ']') {
            const QChar want = c == ')' ? QChar('(') : QChar('[');
            if (!open.isEmpty() && seg[open.last()] == want) open.removeLast();
            else unmatched[k] = true;
        }
    }
    for (int k : open) unmatched[k] = true;
    QString out;
    int from = 0;
    for (int k = 0; k <= seg.size(); ++k) {
        if (k == seg.size() || unmatched[k]) {
            out += isolateRun(seg.mid(from, k - from));
            if (k < seg.size()) out += seg[k];
            from = k + 1;
        }
    }
    return out;
}

inline QString isolateLine(const QString &t)
{
    QString out;
    out.reserve(t.size() + 16);
    int i = 0;
    const int n = t.size();
    bool hasRtl = false, hasLtr = false;
    while (i < n) {
        if (strongRtl(t[i])) { hasRtl = true; out += t[i++]; continue; }
        int j = i;                                // a maximal stretch without strong RTL
        while (j < n && !strongRtl(t[j])) ++j;
        const QString seg = t.mid(i, j - i);
        for (QChar c : seg) if (strongLtr(c)) { hasLtr = true; break; }
        out += isolateSegment(seg);
        i = j;
    }
    // Qt picks each paragraph's direction from its first strong character, so
    // "Timeshift مضبوط على …" became a LEFT-to-right paragraph. A leading
    // right-to-left mark (RLM) makes a mixed Arabic paragraph right-to-left.
    if (hasRtl && hasLtr) out.prepend(QChar(0x200F));
    return out;
}

inline QString isolatePlain(const QString &t)
{
    const QStringList lines = t.split('\n');
    QStringList out;
    out.reserve(lines.size());
    for (const QString &l : lines) out << isolateLine(l);
    return out.join('\n');
}

// Already processed? Our isolates (LRI) are the marker — isolating twice
// would nest them again on every repaint. (Not RLM: translations may contain
// their own RLM marks and must still be processed.)
inline bool isIsolated(const QString &t)
{
    return t.contains(QChar(0x2066));
}

inline QString isolate(const QString &text)
{
    if (text.isEmpty() || !QGuiApplication::isRightToLeft() || isIsolated(text)) return text;
    if (!text.contains('<') && !text.contains('&')) return isolatePlain(text);
    // Rich text: process the text between tags and entities (&lt; …), which
    // are copied untouched. A block tag (<p>, <br>, <li>, <div>, <td>, <h1>…)
    // starts a new paragraph, so the RLM rule is applied per block.
    static const QRegularExpression tag("<[^>]*>|&[#A-Za-z0-9]+;");
    static const QRegularExpression block("^</?(p|br|li|div|td|th|tr|h[1-6]|ul|ol|table|pre)\\b",
                                          QRegularExpression::CaseInsensitiveOption);
    QString out, para;                    // para = current block's text, with tags kept inline
    QVector<QPair<int, QString>> inlineTags;   // (offset in para, tag) to re-insert
    auto flush = [&]{
        // Isolate the plain text of the block, then put inline tags/entities back
        // at their places (they are zero-width for the bidi algorithm).
        QString plain;
        QVector<QPair<int, QString>> at;
        int last = 0;
        for (const auto &it : inlineTags) { plain += para.mid(last, it.first - last); at << qMakePair(plain.size(), it.second); last = it.first; }
        plain += para.mid(last);
        // Isolating changes offsets — do it per piece between tags instead.
        QString res;
        int from = 0;
        bool rtl = false, ltr = false;
        for (QChar c : plain) { if (strongRtl(c)) rtl = true; else if (strongLtr(c)) ltr = true; }
        for (const auto &a : at) {
            res += isolatePlain(plain.mid(from, a.first - from)).remove(QChar(0x200F));
            res += a.second;
            from = a.first;
        }
        res += isolatePlain(plain.mid(from)).remove(QChar(0x200F));
        if (rtl && ltr) res.prepend(QChar(0x200F));
        out += res;
        para.clear();
        inlineTags.clear();
    };
    int pos = 0;
    for (auto it = tag.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        para += text.mid(pos, m.capturedStart() - pos);
        if (block.match(m.captured(0)).hasMatch()) { flush(); out += m.captured(0); }
        else inlineTags << qMakePair(int(para.size()), m.captured(0));
        pos = m.capturedEnd();
    }
    para += text.mid(pos);
    flush();
    return out;
}

} // namespace Bidi
