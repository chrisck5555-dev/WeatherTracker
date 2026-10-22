#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>

// Approach B: one widget that draws all 42 cells itself.
class PaintedGrid : public QWidget
{
public:
    int offset = 0;
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const double cw = width() / 7.0, ch = height() / 6.0;
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 7; ++c) {
                const QRectF cell(c * cw, r * ch, cw, ch);
                p.drawRect(cell);
                p.drawText(cell.adjusted(6, 4, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                           QString::number((r * 7 + c + offset) % 31 + 1));
            }
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const int runs = 20;
    QElapsedTimer timer;

    // Approach A: destroy and rebuild 42 widgets on every "navigation".
    timer.start();
    for (int i = 0; i < runs; ++i) {
        QWidget w;
        w.resize(900, 500);
        auto *layout = new QGridLayout(&w);
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 7; ++c) {
                auto *cell = new QLabel(QString::number((r * 7 + c + i) % 31 + 1));
                cell->setFrameStyle(QFrame::Box);
                layout->addWidget(cell, r, c);
            }
        w.grab();                              // forces layout and painting
    }
    qDebug() << "widget per cell:" << timer.nsecsElapsed() / 1e6 / runs << "ms per navigation";

    // Approach B: keep one widget and just repaint it.
    PaintedGrid grid;
    grid.resize(900, 500);
    timer.start();
    for (int i = 0; i < runs; ++i) {
        grid.offset = i;
        grid.grab();
    }
    qDebug() << "direct drawing: " << timer.nsecsElapsed() / 1e6 / runs << "ms per navigation";
    return 0;
}