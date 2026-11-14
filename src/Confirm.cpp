#include "Confirm.h"

#include "Design.h"

#include <QApplication>
#include <algorithm>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace Confirm {

// A plain dialog instead of QMessageBox: on Windows, QMessageBox gives its text the small
// system font and sizes itself from it, which squeezed the app-sized buttons (Chris,
// 2026-11-13: "the close without saving is too big"). Here the text and the buttons use
// the app's Body font, and each button is as wide as its label needs.
bool destructive(QWidget *parent, const QString &title, const QString &message,
                 const QString &safeLabel, const QString &destructiveLabel)
{
    QDialog box(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                            | Qt::WindowCloseButtonHint);
    box.setWindowTitle(title);
    box.setFont(QApplication::font());

    auto *layout = new QVBoxLayout(&box);
    layout->setContentsMargins(Design::kPadCard * 3 / 2, Design::kPadCard * 3 / 2,
                               Design::kPadCard * 3 / 2, Design::kPadCard * 3 / 2);   // 24 (S1)
    layout->setSpacing(Design::kPadCard * 3 / 2);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto *text = new QLabel(message);
    text->setTextFormat(Qt::PlainText);       // messages can contain typed or downloaded text
    text->setWordWrap(true);
    // One line when it fits in 480 px; longer messages wrap at 480.
    text->setFixedWidth(std::clamp(text->fontMetrics().horizontalAdvance(message) + 4, 360, 480));
    layout->addWidget(text);

    // DLG3: the action on the left, the safe answer on the right.
    auto *row = new QHBoxLayout;
    row->setSpacing(Design::kGapButtons);
    // Made in this order, the safe answer comes first in Tab order (D71), though it sits
    // on the right.
    auto *safe = new QPushButton(safeLabel, &box);
    auto *danger = new QPushButton(destructiveLabel, &box);
    for (QPushButton *b : {danger, safe}) {
        b->setAutoDefault(false);
        b->setMinimumHeight(Design::kTarget);   // Z1, B6
        row->addWidget(b);
    }
    row->addStretch(1);
    layout->addLayout(row);

    // D72: the safe answer is the default button and has the primary look, so it draws
    // the eye first; Enter and Esc both give it (DLG3, DLG4), and so does the window's X.
    safe->setDefault(true);
    safe->setProperty("wtKind", "primary");
    // D71: nothing shows the focus tint when the dialog opens, and the first Tab goes to
    // the safe answer, so a key pressed before she reads it can only do the safe thing.
    QTimer::singleShot(0, &box, [&box] {
        box.setAttribute(Qt::WA_KeyboardFocusChange, false);
        box.setFocus(Qt::OtherFocusReason);              // the dialog itself, not a button
    });
    bool destroy = false;
    QObject::connect(danger, &QPushButton::clicked, &box, [&] { destroy = true; box.accept(); });
    QObject::connect(safe, &QPushButton::clicked, &box, &QDialog::reject);

    text->setAccessibleName(message);
    box.setAccessibleName(title);
    box.exec();
    return destroy;
}

} // namespace Confirm
