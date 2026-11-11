#include <QtTest>
#include <QImage>

#include "BoxBlur.h"
#include "Design.h"

class TestBoxBlur : public QObject
{
    Q_OBJECT

    static QImage image(int w, int h, QColor fill)
    {
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(fill);
        return img;
    }

    // One pass of a box blur done the slow, obvious way, for comparison.
    static QImage reference(const QImage &src, int r)
    {
        QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (int dir = 0; dir < 2; ++dir)
            for (int pass = 0; pass < 3; ++pass) {
                QImage out = img;
                for (int y = 0; y < img.height(); ++y)
                    for (int x = 0; x < img.width(); ++x) {
                        int s[4] = {0, 0, 0, 0};
                        for (int i = -r; i <= r; ++i) {
                            const int sx = dir == 0 ? qBound(0, x + i, img.width() - 1) : x;
                            const int sy = dir == 1 ? qBound(0, y + i, img.height() - 1) : y;
                            const QRgb c = img.pixel(sx, sy);
                            s[0] += qBlue(c); s[1] += qGreen(c); s[2] += qRed(c); s[3] += qAlpha(c);
                        }
                        const int n = 2 * r + 1;
                        out.setPixel(x, y, qRgba((s[2] + n / 2) / n, (s[1] + n / 2) / n,
                                                 (s[0] + n / 2) / n, (s[3] + n / 2) / n));
                    }
                img = out;
            }
        return img;
    }

private slots:
    void flatColourStaysFlat()
    {
        QImage img = image(40, 30, QColor(30, 120, 200));
        BoxBlur::blur(img, 5);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                QCOMPARE(img.pixel(x, y), QColor(30, 120, 200).rgb());
    }

    void matchesTheSlowWay()
    {
        QImage img = image(23, 17, Qt::white);
        for (int y = 0; y < 17; ++y)
            for (int x = 0; x < 23; ++x)
                if ((x / 3 + y / 4) % 2)
                    img.setPixel(x, y, qRgb(20 + x * 5, 200 - y * 7, 90));
        const QImage expected = reference(img, 2);
        BoxBlur::blur(img, 2);
        for (int y = 0; y < 17; ++y)
            for (int x = 0; x < 23; ++x) {
                const QRgb a = img.pixel(x, y), b = expected.pixel(x, y);
                QVERIFY(qAbs(qRed(a) - qRed(b)) <= 1 && qAbs(qGreen(a) - qGreen(b)) <= 1
                        && qAbs(qBlue(a) - qBlue(b)) <= 1);
            }
    }

    void spreadsEvenlyAtAnyRadius()
    {
        for (int r : {1, 3, 8}) {
            QImage img = image(61, 61, Qt::black);
            img.setPixel(30, 30, qRgb(255, 255, 255));
            img.setPixel(31, 30, qRgb(255, 255, 255));
            img.setPixel(30, 31, qRgb(255, 255, 255));
            img.setPixel(31, 31, qRgb(255, 255, 255));
            BoxBlur::blur(img, r);
            // Symmetric around the 2 x 2 spot, and brightest at it.
            QCOMPARE(qRed(img.pixel(30 - r, 30)), qRed(img.pixel(31 + r, 30)));
            QCOMPARE(qRed(img.pixel(30, 30 - r)), qRed(img.pixel(30, 31 + r)));
            QVERIFY(qRed(img.pixel(30, 30)) >= qRed(img.pixel(30 - r, 30)));
            QVERIFY(qRed(img.pixel(30 - r, 30)) > 0);
        }
    }

    void sharpEdgesBecomeSoft()
    {
        QImage img = image(40, 4, Qt::black);
        for (int x = 20; x < 40; ++x)
            for (int y = 0; y < 4; ++y)
                img.setPixel(x, y, qRgb(255, 255, 255));
        BoxBlur::blur(img, 3);
        int previous = -1;
        for (int x = 0; x < 40; ++x) {       // rises steadily from black to white
            const int v = qRed(img.pixel(x, 1));
            QVERIFY(v >= previous);
            previous = v;
        }
        QVERIFY(qRed(img.pixel(19, 1)) > 60 && qRed(img.pixel(19, 1)) < 195);
    }

    void radiusZeroDoesNothing()
    {
        QImage img = image(5, 5, Qt::black);
        img.setPixel(2, 2, qRgb(255, 0, 0));
        const QImage before = img;
        BoxBlur::blur(img, 0);
        QCOMPARE(img, before);
    }

    void passRadiusForTheQuarterCopy()
    {
        QCOMPARE(BoxBlur::passRadius(Design::kBlurRadius, 0.25), 3);   // G4: 24 px at quarter size
        QCOMPARE(BoxBlur::passRadius(Design::kBlurRadius, 0.5), 6);
        QCOMPARE(BoxBlur::passRadius(2, 0.25), 1);                     // never 0
    }
};

QTEST_APPLESS_MAIN(TestBoxBlur)
#include "tst_boxblur.moc"
