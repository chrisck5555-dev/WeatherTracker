#include "FlowLayout.h"

#include <QWidget>
#include <algorithm>

FlowLayout::FlowLayout(QWidget *parent, int hSpacing, int vSpacing)
    : QLayout(parent), m_h(hSpacing), m_v(vSpacing)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowLayout::addWidget(QWidget *widget, int flags)
{
    addChildWidget(widget);
    m_items.append({new QWidgetItem(widget), flags});
    invalidate();
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_items.append({item, None});
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items[index].item : nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    return m_items.takeAt(index).item;
}

int FlowLayout::heightForWidth(int width) const
{
    if (const auto it = m_hfw.constFind(width); it != m_hfw.constEnd())
        return it.value();
    const QMargins m = contentsMargins();
    const int lines = m_lines;                  // asking doesn't change the last layout's count
    const int h = doLayout(QRect(0, 0, width - m.left() - m.right(), 0), false) + m.top() + m.bottom();
    m_lines = lines;
    m_hfw.insert(width, h);
    return h;
}

void FlowLayout::invalidate()
{
    m_hfw.clear();
    QLayout::invalidate();
}

QSize FlowLayout::minimumSize() const
{
    // As narrow as the widest item. The height at a given width comes from heightForWidth,
    // so the minimum height is the least it ever needs: one line.
    int w = 0, h = 0;
    for (const Entry &e : m_items)
        if (!e.item->isEmpty()) {
            w = std::max(w, e.item->minimumSize().width());
            h = std::max(h, e.item->minimumSize().height());
        }
    const QMargins m = contentsMargins();
    return QSize(w + m.left() + m.right(), h + m.top() + m.bottom());
}

QSize FlowLayout::sizeHint() const
{
    // Everything on one line.
    int w = 0, h = 0, n = 0;
    for (const Entry &e : m_items) {
        if (e.item->isEmpty())
            continue;
        const QSize s = e.item->sizeHint();
        w += s.width() + (n++ ? m_h : 0);
        h = std::max(h, s.height());
    }
    const QMargins m = contentsMargins();
    return QSize(w + m.left() + m.right(), h + m.top() + m.bottom());
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect.marginsRemoved(contentsMargins()), true);
}

int FlowLayout::doLayout(const QRect &rect, bool apply) const
{
    struct Placed { QLayoutItem *item; int flags; int w; };
    QList<Placed> visible;
    for (const Entry &e : m_items)
        if (!e.item->isEmpty())
            visible.append({e.item, e.flags, e.item->sizeHint().width()});
    if (visible.isEmpty()) {
        m_lines = 0;
        return 0;
    }
    const int width = std::max(1, rect.width());
    auto heightOf = [](QLayoutItem *item, int w) {
        return item->hasHeightForWidth() ? item->heightForWidth(w) : item->sizeHint().height();
    };
    auto place = [&](QLayoutItem *item, int x, int y, int w, int lineH) {
        if (!apply)
            return;
        const int h = std::min(lineH, heightOf(item, w));
        item->setGeometry(QRect(x, y + (lineH - h) / 2, w, h));
    };

    int total = 0;
    for (const Placed &p : visible)
        total += p.w;
    total += m_h * int(visible.size() - 1);

    // Grid mode, when the group doesn't fit one line (K10).
    if (m_grid && total > width) {
        int y = rect.y(), lines = 0;
        QList<Placed> cells;
        for (const Placed &p : visible) {
            if (p.flags & Header) {
                const int w = std::min(p.w, width);
                const int h = heightOf(p.item, w);
                place(p.item, rect.x(), y, w, h);
                y += h + m_v;
                ++lines;
            } else {
                cells.append(p);
            }
        }
        int cellW = 0, cellH = 0;
        for (const Placed &p : cells)
            cellW = std::max(cellW, p.w);
        const int n = int(cells.size());
        const int cols = std::clamp((width + m_h) / std::max(1, cellW + m_h), 1, std::max(1, n));
        const int w = cols == 1 ? width : (width - (cols - 1) * m_h) / cols;
        for (const Placed &p : cells)
            cellH = std::max(cellH, heightOf(p.item, w));
        for (int i = 0; i < n; ++i) {
            const int row = i / cols, col = i % cols;
            place(cells[i].item, rect.x() + col * (w + m_h), y + row * (cellH + m_v), w, cellH);
        }
        const int rows = n ? (n + cols - 1) / cols : 0;
        lines += rows;
        m_lines = lines;
        return (y - rect.y()) + (rows ? rows * cellH + (rows - 1) * m_v : -m_v);
    }

    // Flow: fill each line, then move whole items to the next.
    QList<QList<Placed>> lines;
    int x = 0;
    for (const Placed &p : visible) {
        const int w = std::min(p.w, width);
        if (!lines.isEmpty() && !lines.last().isEmpty() && x + m_h + w > width) {
            lines.append(QList<Placed>());
            x = 0;
        }
        if (lines.isEmpty())
            lines.append(QList<Placed>());
        if (!lines.last().isEmpty())
            x += m_h;
        lines.last().append({p.item, p.flags, w});
        x += w;
    }

    int y = rect.y();
    for (const auto &line : lines) {
        int lineH = 0;
        for (const Placed &p : line)
            lineH = std::max(lineH, heightOf(p.item, p.w));
        // Items before the first PushRight sit at the left; it and the rest at the right.
        int split = int(line.size());
        for (int i = 1; i < line.size(); ++i)
            if (line[i].flags & PushRight) {
                split = i;
                break;
            }
        int lx = rect.x();
        for (int i = 0; i < split; ++i) {
            place(line[i].item, lx, y, line[i].w, lineH);
            lx += line[i].w + m_h;
        }
        int rightW = 0;
        for (int i = split; i < line.size(); ++i)
            rightW += line[i].w + (i > split ? m_h : 0);
        int rx = rect.x() + width - rightW;
        for (int i = split; i < line.size(); ++i) {
            place(line[i].item, rx, y, line[i].w, lineH);
            rx += line[i].w + m_h;
        }
        y += lineH + m_v;
    }
    m_lines = int(lines.size());
    return y - m_v - rect.y();
}
