#pragma once

#include <QTimer>
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;

// A small bar at the bottom of the window: "The day was deleted.  [Undo]  [×]".
// It goes away by itself after 10 seconds; the time stops while the mouse is over it.
// Only the newest action can be undone: showing a new message drops the old undo.
class Toast : public QWidget
{
    Q_OBJECT

public:
    explicit Toast(QWidget *window);        // `window`: the area it sits at the bottom of

    void showMessage(const QString &text, std::function<void()> undo);
    bool undo();                            // Ctrl+Z: undoes the shown action; false if none
    void dismiss();                         // hide without undoing

    static constexpr int kShowMs = 10000;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void place();

    QLabel *m_text = nullptr;
    QPushButton *m_undo = nullptr;
    QPushButton *m_close = nullptr;
    QTimer m_timer;
    int m_remainingMs = 0;
    std::function<void()> m_undoAction;
};
