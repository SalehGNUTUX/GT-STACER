#pragma once
#include <QTableView>
#include <QHeaderView>
#include <QObject>
#include <QEvent>

// Column-resize helper. Qt's QHeaderView::Stretch makes a column fill the width
// but then it CANNOT be dragged, and the drag handle on its boundary goes dead —
// which is why resizing felt broken. This keeps `primaryCol` filling any spare
// width (like Stretch) while every column, including that one, stays Interactive
// and freely resizable, so all boundaries have a real, grabbable handle.
class StretchColumnFiller : public QObject {
public:
    StretchColumnFiller(QTableView *table, int primaryCol)
        : QObject(table), m_table(table), m_col(primaryCol)
    {
        auto *hh = m_table->horizontalHeader();
        hh->setSectionResizeMode(QHeaderView::Interactive);
        hh->setStretchLastSection(false);
        hh->setMinimumSectionSize(70);
        hh->setHighlightSections(false);
        m_table->viewport()->installEventFilter(this);
        // When the user drags any OTHER column, re-fill the primary one so the
        // table keeps spanning the width (no gap, no premature scrollbar).
        connect(hh, &QHeaderView::sectionResized, this,
                [this](int idx, int, int){ if (idx != m_col) fill(); });
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::Resize) fill();
        return QObject::eventFilter(o, e);
    }

private:
    void fill() {
        if (m_busy) return;
        m_busy = true;
        auto *hh = m_table->horizontalHeader();
        int others = 0;
        for (int i = 0; i < hh->count(); ++i)
            if (i != m_col && !hh->isSectionHidden(i)) others += hh->sectionSize(i);
        const int avail = m_table->viewport()->width() - others;
        if (avail > hh->minimumSectionSize()) hh->resizeSection(m_col, avail);
        m_busy = false;
    }

    QTableView *m_table;
    int  m_col;
    bool m_busy = false;
};

// Configure a table so `primaryCol` fills the width yet all columns stay
// user-resizable with visible, grabbable drag handles.
inline void setupResizableTable(QTableView *table, int primaryCol = 0)
{
    new StretchColumnFiller(table, primaryCol);
}
