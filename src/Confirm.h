#pragma once

#include <QString>

class QWidget;

namespace Confirm {

// Two-button question. The safe choice is on the LEFT and is the default
// (pressing Enter picks it). Returns true if the user chose the destructive one.
bool destructive(QWidget *parent, const QString &title, const QString &message,
                 const QString &safeLabel, const QString &destructiveLabel);

} // namespace Confirm