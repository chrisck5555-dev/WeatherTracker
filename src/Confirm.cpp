#include "Confirm.h"

#include <QMessageBox>
#include <QPushButton>

namespace Confirm {

bool destructive(QWidget *parent, const QString &title, const QString &message,
                 const QString &safeLabel, const QString &destructiveLabel)
{
    QMessageBox box(QMessageBox::Question, title, message, QMessageBox::NoButton, parent);
    box.setTextFormat(Qt::PlainText);        // messages can contain typed or downloaded text
    // Buttons appear in the order they are added on Windows, so safe is added first.
    QPushButton *safe = box.addButton(safeLabel, QMessageBox::RejectRole);
    QPushButton *danger = box.addButton(destructiveLabel, QMessageBox::AcceptRole);
    box.setDefaultButton(safe);
    box.setEscapeButton(safe);               // Esc / the window X = the safe choice
    for (auto *b : {safe, danger})
        b->setMinimumSize(160, 48);
    box.exec();
    return box.clickedButton() == danger;
}

} // namespace Confirm