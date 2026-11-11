#pragma once

#include <QPointer>
#include <QTimer>
#include <functional>

#include "Glass.h"

class QPushButton;
class UndoMessage;

// The undo bar (K1): "Deleted everything recorded for ...  [Undo]  [Close]".
// - Glass at elevation 128, bottom centre, 16 px from the bottom, floating over the page,
//   which never moves for it (Chris, 2026-11-10). space() is how much of the window's bottom
//   it covers; spaceChanged() tells the window, which keeps the focus clear of it (SC5).
// - It floats over the whole window, so it also shows on Settings (where all the data can be
//   erased). There it is drawn solid: the glass only has copies of the normal screen.
// - Shows for 10 s and holds still while the mouse is over it. Narrator reads the message
//   without moving the focus.
// - The last action stays undoable with Ctrl+Z (and Ctrl+K's "Undo ...") after the bar
//   hides, until the next change or until the app closes.
// - In a narrow window the message wraps to two lines beside the buttons, and only when
//   that doesn't fit do the buttons go under it (RZ5).
class UndoBar : public GlassGroup
{
    Q_OBJECT

public:
    // `host`: the widget it floats in (the window's whole area).
    UndoBar(GlassBackdrop *backdrop, QWidget *host);

    void showMessage(const QString &text, std::function<void()> undo);
    bool undo();                       // false if there is nothing to undo
    void dismiss();                    // hides it; the action stays undoable
    void forget();                     // something else changed: nothing left to undo
    bool canUndo() const { return bool(m_undoAction); }
    QString undoLabel() const { return m_undoLabel; }   // for Ctrl+K: "Undo: <message>"

    // K1: how much of the window's bottom the bar covers: its height plus 16 while it shows,
    // 0 otherwise.
    int space() const;
    void place();                      // bottom centre of the host

signals:
    void spaceChanged(int space);

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int widthFor(int frameWidth) const;
    void syncShadow();                 // the shadow follows the bar: place, stacking, shown
    void notifySpace();

    QPointer<QWidget> m_shadow;        // E1: painted outside the bar, so by a widget under it

    UndoMessage *m_text;
    QWidget *m_buttons;
    QPushButton *m_undo;
    QPushButton *m_close;
    QTimer m_timer;
    int m_remainingMs = 0;
    std::function<void()> m_undoAction;
    QString m_undoLabel;
    int m_space = 0;
};
