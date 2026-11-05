#pragma once

#include <QAccessible>
#include <QEvent>
#include <QKeyEvent>
#include <QWidget>
#include <initializer_list>

// Helpers for the painted calendar views (Month, Week, Year), which have no child widgets
// for the keyboard or a screen reader to reach.
namespace KeyboardNav {

// The window turns Left/Right into Previous/Next. A view that uses those keys itself
// claims them while it has the focus (Qt asks first, with a ShortcutOverride event).
inline bool claim(QEvent *event, std::initializer_list<int> keys)
{
    if (event->type() != QEvent::ShortcutOverride)
        return false;
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->modifiers() & ~Qt::KeypadModifier)
        return false;                               // Ctrl+..., Alt+...: not ours
    for (int k : keys)
        if (key->key() == k) {
            event->accept();
            return true;
        }
    return false;
}

// What a screen reader says for the view: the selected day or month. Narrator reads the
// new name each time it changes while the view has the focus.
inline void announce(QWidget *view, const QString &text)
{
    if (view->accessibleName() == text)
        return;
    view->setAccessibleName(text);
    QAccessibleEvent event(view, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&event);
}

} // namespace KeyboardNav
