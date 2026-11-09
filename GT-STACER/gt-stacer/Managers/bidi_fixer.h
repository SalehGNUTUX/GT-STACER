#pragma once
#include <QObject>

// App-wide fix for Arabic text that contains commands, paths or English names
// (26.12). Installed once on the QApplication: just before any QLabel (this
// includes message boxes and tooltips), button, check box or group-box title
// is painted, its text is passed through Bidi::isolate() — so no page has to
// remember to do it. Only when the UI is right-to-left and the text mixes
// right-to-left and left-to-right letters; pure English text (and its "&"
// shortcuts) is never touched. Processed text carries an isolate mark, so it
// is handled once, not on every repaint.
class BidiFixer : public QObject {
    Q_OBJECT
public:
    explicit BidiFixer(QObject *parent = nullptr) : QObject(parent) {}
    static void install();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
