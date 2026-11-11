#pragma once

#include <QScrollArea>

class QScrollBar;
class QVBoxLayout;

// The scrolling page (G14, SC1, SC2, SC5). It fills the window and runs under the floating
// bars, so the page can slide beneath them; the undo bar floats over its bottom (K1).
// - Scrolling is vertical only; the page is never wider than the window (SC1).
// - Its scroll bar is a separate widget the window places at its right edge, starting below
//   the tab row, shown only when there is more to see (SC2, L2).
// - When a control gets the keyboard focus, the page scrolls so all of it can be seen, clear
//   of the bars and the undo bar, with 8 px to spare (SC5).
class PageArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit PageArea(QWidget *parent = nullptr);

    void setPage(QWidget *page);              // the page's column (header and the tab's view)
    QWidget *barsSlot() const { return m_barsSlot; }   // the bars, while they scroll with the page
    // L8, L9: where the page starts at rest (below the bars), and the room on the right
    // (16, or the scroll bar's width while it shows).
    void setPageMargins(int top, int right);
    void setFocusClearance(int top) { m_clearTop = top; }   // SC5: what the bars cover
    void setFocusClearanceBottom(int bottom) { m_clearBottom = bottom; }   // and the undo bar
    QScrollBar *bar() const { return m_bar; }

signals:
    void barShownChanged(bool shown);

protected:
    bool focusNextPrevChild(bool next) override;

private:
    void keepFocusVisible(QWidget *w);

    QWidget *m_content;
    QWidget *m_barsSlot;
    QVBoxLayout *m_pageBox;
    QScrollBar *m_bar;
    int m_clearTop = 0;
    int m_clearBottom = 0;
};
