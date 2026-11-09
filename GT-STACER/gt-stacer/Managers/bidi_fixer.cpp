#include "bidi_fixer.h"
#include "../Widgets/bidi_text.h"
#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>

namespace {

// Worth processing: mixed right-to-left + left-to-right, not yet isolated.
bool needsFix(const QString &t)
{
    if (t.isEmpty() || Bidi::isIsolated(t)) return false;
    bool rtl = false, ltr = false;
    for (QChar c : t) {
        if (Bidi::strongRtl(c)) rtl = true;
        else if (Bidi::strongLtr(c)) ltr = true;
        if (rtl && ltr) return true;
    }
    return false;
}

} // namespace

void BidiFixer::install()
{
    static BidiFixer *fixer = nullptr;
    if (fixer) return;
    fixer = new BidiFixer(qApp);
    qApp->installEventFilter(fixer);
}

bool BidiFixer::eventFilter(QObject *watched, QEvent *event)
{
    // Cheap first: only paint events of widgets, only in a right-to-left UI.
    if (event->type() != QEvent::Paint || !QGuiApplication::isRightToLeft())
        return false;
    if (auto *l = qobject_cast<QLabel *>(watched)) {
        if (needsFix(l->text())) l->setText(Bidi::isolate(l->text()));
    } else if (auto *b = qobject_cast<QAbstractButton *>(watched)) {
        if (needsFix(b->text())) b->setText(Bidi::isolate(b->text()));
    } else if (auto *g = qobject_cast<QGroupBox *>(watched)) {
        if (needsFix(g->title())) g->setTitle(Bidi::isolate(g->title()));
    }
    return false;                       // the widget still paints itself
}
