#pragma once

#include <QDate>
#include <QWidget>

class AppState;

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
    QSize minimumSizeHint() const override { return QSize(620, 420); }

private:
    QRectF tileRect(int index) const;     // 0..11

    AppState &m_state;
    int m_pressed = -1;                   // tile being pressed
};