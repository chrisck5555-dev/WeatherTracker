#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class FlowLayout;
class QAbstractButton;
class QButtonGroup;
class QLabel;
class QPushButton;

// A row of choice buttons where exactly one is chosen: the tabs, and each tab's
// Day/Week/Month/Year choice (L10). Chosen = accent fill (ST4).
// - Keyboard (KB2): the group is one Tab stop, entered at the chosen button; Left/Right
//   (and Up/Down) move and choose, Home/End go to the first and last. The window's own use
//   of Left/Right (Previous/Next) gives way while a button here has the focus.
// - When it no longer fits one line, it lays out on equal cells in shared columns, with the
//   label on its own line above (K10).
class ChoiceButtons : public QWidget
{
    Q_OBJECT

public:
    explicit ChoiceButtons(const QString &label = QString(), QWidget *parent = nullptr);

    QPushButton *addChoice(int id, const QString &text, const QString &icon = QString());
    void setChosen(int id);                     // shows the choice; doesn't emit
    int chosen() const;
    QPushButton *button(int id) const;
    QList<QAbstractButton *> buttons() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

signals:
    void choiceMade(int id);                    // a click, or an arrow key

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateTabStop();

    QButtonGroup *m_group;
    FlowLayout *m_flow;
    QLabel *m_label = nullptr;
};
