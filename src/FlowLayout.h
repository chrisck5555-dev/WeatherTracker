#pragma once

#include <QLayout>
#include <QHash>
#include <QList>

// Lays its items out left to right and moves whole items to the next line when they don't
// fit (G7, RZ2: reposition). Used by the floating bars, their glass groups, the page header
// and the undo bar.
//
// - An item added with PushRight starts the right-hand part of its line, like a spacer
//   before it. On a line of its own it stays at the left edge (L1).
// - In grid mode (K10's choice groups, such as the tabs), a group that no longer fits one
//   line is laid out on equal-width cells in shared columns instead, with any Header item
//   (the "Show:" label) on its own line above them.
class FlowLayout : public QLayout
{
public:
    enum Flag { None = 0, PushRight = 1, Header = 2 };

    explicit FlowLayout(QWidget *parent = nullptr, int hSpacing = 12, int vSpacing = 12);
    ~FlowLayout() override;

    void addWidget(QWidget *widget, int flags = None);
    void setGridWhenWrapped(bool on) { m_grid = on; invalidate(); }
    int lineCount() const { return m_lines; }         // after the last layout
    int horizontalGap() const { return m_h; }
    int verticalGap() const { return m_v; }

    void addItem(QLayoutItem *item) override;
    int count() const override { return int(m_items.size()); }
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;
    void invalidate() override;

private:
    struct Entry { QLayoutItem *item; int flags; };
    // Lays out in `rect` (inside the margins); moves the items only if `apply`.
    // Returns the height used.
    int doLayout(const QRect &rect, bool apply) const;

    QList<Entry> m_items;
    int m_h, m_v;
    bool m_grid = false;
    mutable int m_lines = 0;
    mutable QHash<int, int> m_hfw;          // height for each width asked, until invalidated
};
