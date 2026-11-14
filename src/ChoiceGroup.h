#pragma once

#include <QList>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <functional>

class QGridLayout;
class QLabel;

// One choice in a ChoiceGroup: what is stored, the words in the cell, what Narrator says,
// and the weather picture's name.
struct Choice {
    QString value;                              // stored, e.g. "Freezing rain" or "SE"
    QString words;                              // in the cell (Caption), e.g. "SE"
    QString spoken;                             // Narrator's name; empty = the words
    QString picture;                            // weather picture name; empty = none
};

// A cell of a choice group (K10, D65): 108 x 56, a 24 px picture above a Caption label. ClayStyle
// paints its states like every button: hover and pressed tints, chosen = accent fill
// (ST4), the focus tint only with the keyboard (F1), disabled muted (ST5).
class ChoiceCell : public QPushButton
{
    Q_OBJECT

public:
    ChoiceCell(const Choice &choice, QWidget *parent = nullptr);
    const Choice &choice() const { return m_choice; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

    int row = 0, column = 0;                    // where it sits in its group's grid

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Choice m_choice;
};

// K10's picture-button grid for the editor: wind and gust direction (a 3 x 3 compass with the
// centre empty), precipitation type (2 rows of 4) and cloud cover (1 row of 5).
// - Every cell is 108 x 56 with 8 px between them, so all groups on a page share columns.
// - When a page is too narrow for the widest group, every group switches to 3 columns
//   together (D43): fitAll().
// - A quiet [Clear] sits beside the label, disabled when nothing is chosen (D42). Clicking the
//   chosen cell again keeps it chosen.
// - Keyboard (KB2): the group is one Tab stop, entered at the chosen cell (or the first);
//   the arrows move and choose inside it, Up and Down by column, Home and End to the ends.
class ChoiceGroup : public QWidget
{
    Q_OBJECT

public:
    enum class Shape { Compass, Rows };

    static constexpr int kCellWidth = 108;      // D65: room for "Freezing rain" chosen (Semibold)
    static constexpr int kCellHeight = 56;
    static constexpr int kCellGap = 8;
    static constexpr int kNarrowColumns = 3;

    // `fullColumns` is the layout when there is room (4 for precipitation, 5 for cloud);
    // a compass is always 3. `spokenName` is the group's name for Narrator.
    ChoiceGroup(const QString &label, const QString &spokenName, const QList<Choice> &choices,
                Shape shape, int fullColumns = 3, QWidget *parent = nullptr);

    QString value() const;                      // the stored value, or empty
    void setValue(const QString &value);        // shows it; doesn't emit changed()
    void clear();                               // as [Clear]: emits changed() if something was chosen

    void setNarrow(bool narrow);
    bool isNarrow() const { return m_narrow; }
    int columns() const;                        // in the current layout
    int rows() const;
    int fullWidth() const { return widthFor(m_fullColumns); }

    void addToHeader(QWidget *w);               // after [Clear], e.g. an error (FM3)

    QLabel *label() const { return m_label; }
    QPushButton *clearButton() const { return m_clear; }
    QList<ChoiceCell *> cells() const { return m_cells; }
    ChoiceCell *cell(const QString &value) const;

    // The width of `columns` cells and their gaps: 340 for 3, 456 for 4, 572 for 5.
    static int widthFor(int columns) { return columns * kCellWidth + (columns - 1) * kCellGap; }

    // D43: every group switches to 3 columns together when `available` is too narrow for
    // the widest group's full layout.
    static void fitAll(const QList<ChoiceGroup *> &groups, int available);

    // Where the cells get their pictures (the editor sets it to the weather pictures, so
    // this class needs nothing from the theme). Arguments: name, size, device-pixel ratio.
    static void setPictureSource(std::function<QPixmap(const QString &, int, qreal)> source);
    static QPixmap picture(const QString &name, int size, qreal dpr);

signals:
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void place();
    void updateState();                         // Tab stop, Clear's state
    ChoiceCell *neighbour(ChoiceCell *from, int key) const;

    QList<ChoiceCell *> m_cells;
    ChoiceCell *m_roving = nullptr;     // the cell the arrow keys moved to, while focus is inside
    Shape m_shape;
    int m_fullColumns;
    bool m_narrow = false;
    QLabel *m_label = nullptr;
    QPushButton *m_clear = nullptr;
    class QHBoxLayout *m_header = nullptr;
    QWidget *m_grid = nullptr;
    QGridLayout *m_gridLayout = nullptr;
};
