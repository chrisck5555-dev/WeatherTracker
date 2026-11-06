#pragma once

#include <QDate>
#include <QWidget>

class AppState;
class HoverFade;

// Twelve painted month tiles (4 x 3). Clicking one opens that month.
class YearView : public QWidget
{
    Q_OBJECT

public:
    explicit YearView(AppState &state, QWidget *parent = nullptr);

signals:
    void monthActivated(const QDate &dateInMonth);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;                 // claims the arrow keys
    void keyPressEvent(QKeyEvent *event) override;      // arrows move a month, Enter opens it
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    QSize minimumSizeHint() const override { return QSize(620, 420); }

private:
    QRectF tileRect(int index) const;     // 0..11
    void updateAccessibleText();

    AppState &m_state;
    int m_pressed = -1;                   // tile being pressed
    HoverFade *m_hover = nullptr;         // ST2's tint under the mouse
};