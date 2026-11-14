#pragma once

#include <QList>
#include <QWidget>

class FlowLayout;
class QButtonGroup;
class QPushButton;

// A row of choice buttons for Settings (K10): Metric / Imperial, the text sizes, the style and
// the brightness.
// - Each button is as wide as its words (at least 88) and they wrap to the next line when
//   the row doesn't fit (RZ2), so Settings never scrolls sideways (SC1).
// - Chosen = accent fill (ST4); clicking the chosen one again keeps it chosen.
// - Keyboard (KB2, D66): one Tab stop, entered at the chosen button. The arrows move the
//   focus only; Space or Enter chooses.
class ChoiceRow : public QWidget
{
    Q_OBJECT

public:
    explicit ChoiceRow(const QString &spokenName, QWidget *parent = nullptr);

    // `words` may hold a "\n" for a second line; Narrator reads it as one line.
    QPushButton *addChoice(int id, const QString &words, const QString &tooltip);
    void setChosen(int id);
    int chosen() const;
    QList<QPushButton *> buttons() const { return m_buttons; }

signals:
    void chosenChanged(int id);                 // she chose (not when setChosen is called)

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateStops();

    FlowLayout *m_flow = nullptr;
    QButtonGroup *m_group = nullptr;
    QList<QPushButton *> m_buttons;
    QPushButton *m_roving = nullptr;            // where the arrows moved, while focus is inside
};
