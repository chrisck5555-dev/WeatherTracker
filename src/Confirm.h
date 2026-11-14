#pragma once

#include <QString>

class QWidget;

namespace Confirm {

// Two-button question (DLG3): the action on the left, the safe answer on the right.
// Enter, Esc and the window's X all give the safe answer. Returns true if the user
// chose the destructive one.
bool destructive(QWidget *parent, const QString &title, const QString &message,
                 const QString &safeLabel, const QString &destructiveLabel);

} // namespace Confirm