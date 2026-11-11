#include <QtTest>
#include <QWidget>

#include "FlowLayout.h"

// A widget of a fixed preferred size.
class Box : public QWidget
{
public:
    Box(int w, int h) : m_size(w, h) {}
    QSize sizeHint() const override { return m_size; }
    QSize minimumSizeHint() const override { return QSize(10, m_size.height()); }

private:
    QSize m_size;
};

class TestFlowLayout : public QObject
{
    Q_OBJECT

private slots:
    void oneLineWhenItFits()
    {
        QWidget w;
        auto *flow = new FlowLayout(&w, 12, 12);
        QList<Box *> boxes{new Box(100, 44), new Box(80, 44), new Box(60, 44)};
        for (Box *b : boxes) flow->addWidget(b);
        w.resize(400, 44);
        flow->setGeometry(QRect(0, 0, 400, 44));
        QCOMPARE(flow->lineCount(), 1);
        QCOMPARE(boxes[1]->x(), 112);
        QCOMPARE(boxes[2]->x(), 204);
        QCOMPARE(flow->sizeHint(), QSize(264, 44));
    }

    void wholeItemsWrap()
    {
        QWidget w;
        auto *flow = new FlowLayout(&w, 12, 12);
        QList<Box *> boxes{new Box(100, 44), new Box(100, 44), new Box(100, 44)};
        for (Box *b : boxes) flow->addWidget(b);
        flow->setGeometry(QRect(0, 0, 230, 0));
        QCOMPARE(flow->lineCount(), 2);
        QCOMPARE(boxes[2]->geometry(), QRect(0, 56, 100, 44));   // never split, never cut
        QCOMPARE(flow->heightForWidth(230), 100);
        QCOMPARE(flow->heightForWidth(400), 44);
    }

    void pushRightGoesRightUnlessAlone()
    {
        QWidget w;
        auto *flow = new FlowLayout(&w, 12, 12);
        auto *a = new Box(100, 44), *b = new Box(100, 44), *c = new Box(50, 44);
        flow->addWidget(a);
        flow->addWidget(b, FlowLayout::PushRight);
        flow->addWidget(c);
        flow->setGeometry(QRect(0, 0, 500, 0));
        QCOMPARE(b->x(), 500 - 162);
        QCOMPARE(c->x(), 450);
        flow->setGeometry(QRect(0, 0, 200, 0));        // b starts a line of its own: left edge
        QCOMPARE(b->x(), 0);
        QCOMPARE(b->y(), 56);
    }

    void gridWhenWrapped()
    {
        QWidget w;
        auto *flow = new FlowLayout(&w, 8, 8);
        flow->setGridWhenWrapped(true);
        auto *label = new Box(70, 28);
        flow->addWidget(label, FlowLayout::Header);
        QList<Box *> cells{new Box(90, 44), new Box(120, 44), new Box(100, 44), new Box(80, 44)};
        for (Box *b : cells) flow->addWidget(b);
        flow->setGeometry(QRect(0, 0, 300, 0));
        // The label on its own line; then equal cells in two shared columns.
        QCOMPARE(label->geometry().topLeft(), QPoint(0, 0));
        QCOMPARE(cells[0]->width(), cells[1]->width());
        QCOMPARE(cells[2]->x(), cells[0]->x());
        QCOMPARE(cells[3]->x(), cells[1]->x());
        QCOMPARE(cells[0]->y(), 36);
        QCOMPARE(cells[2]->y(), 88);
        QCOMPARE(flow->lineCount(), 3);
        QVERIFY(cells[0]->width() >= 120);
        flow->setGeometry(QRect(0, 0, 600, 0));        // fits: one ordinary line
        QCOMPARE(flow->lineCount(), 1);
        QCOMPARE(cells[0]->width(), 90);
    }

    void hiddenItemsTakeNoSpace()
    {
        QWidget w;
        auto *flow = new FlowLayout(&w, 12, 12);
        auto *a = new Box(100, 44), *b = new Box(100, 44), *c = new Box(100, 44);
        flow->addWidget(a); flow->addWidget(b); flow->addWidget(c);
        w.show();
        b->hide();
        flow->setGeometry(QRect(0, 0, 400, 0));
        QCOMPARE(c->x(), 112);
    }
};

QTEST_MAIN(TestFlowLayout)
#include "tst_flowlayout.moc"
