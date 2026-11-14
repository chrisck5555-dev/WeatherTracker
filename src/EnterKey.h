#pragma once

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QObject>

// KB3: Enter activates the focused button, as Space does. Qt buttons outside dialogs only
// answer Space (Chris, 2026-11-13: "I can use space to activate the clear button, but I
// cannot use enter"). Installed on the application, so it covers every button.
// Check boxes too, so every button answers the same keys (Chris, 2026-11-13).
//
// The arrow keys never move between separate buttons: Qt's buttons treat them like Tab,
// which ran from the events through Save and Cancel into the note (Chris, 2026-11-13).
// They go to the page instead, which scrolls with Up and Down (KB9). Groups that use the
// arrows inside themselves (KB2) mark their buttons with the "wtOwnArrows" property.
class EnterKey : public QObject
{
public:
    using QObject::QObject;

private:
    static bool isArrow(int k)
    {
        return k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::KeyPress)
            return QObject::eventFilter(watched, event);
        auto *key = static_cast<QKeyEvent *>(event);
        if (isArrow(key->key()) && !(key->modifiers() & ~Qt::KeypadModifier)) {
            auto *button = qobject_cast<QAbstractButton *>(watched);
            if (!button || button->property("wtOwnArrows").toBool() || !button->parentWidget())
                return QObject::eventFilter(watched, event);
            QKeyEvent copy(key->type(), key->key(), key->modifiers(), key->text(),
                           key->isAutoRepeat(), ushort(key->count()));
            QCoreApplication::sendEvent(button->parentWidget(), &copy);   // on up to the page
            return true;
        }
        if ((key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter)
            || (key->modifiers() & ~Qt::KeypadModifier))
            return QObject::eventFilter(watched, event);
        auto *button = qobject_cast<QAbstractButton *>(watched);
        if (!button || !button->hasFocus() || !button->isEnabled())
            return QObject::eventFilter(watched, event);
        if (key->isAutoRepeat())
            return true;                                   // one press, one action
        // A chosen button in a group of choices stays chosen (D42): nothing to do.
        if (button->isCheckable() && button->isChecked() && button->group()
            && button->group()->exclusive())
            return true;
        button->animateClick();                            // shows the press, as Space does (M7)
        return true;
    }
};
