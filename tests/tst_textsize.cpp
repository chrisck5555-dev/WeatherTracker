#include <QtTest>

#include "TextSize.h"

using TextSize::Size;

class TestTextSize : public QObject
{
    Q_OBJECT

private slots:
    void fitting()
    {
        QVERIFY(TextSize::fitting(1366, 728) == QList<Size>{Size::Normal});        // small laptop
        QVERIFY(TextSize::fitting(1536, 824)
                == (QList<Size>{Size::Normal, Size::Large}));                       // 1080p at 125%
        QVERIFY(TextSize::fitting(1920, 1040).size() == 3);                         // big screen
        QVERIFY(TextSize::fitting(800, 500) == QList<Size>{Size::Normal});          // tiny: Normal stays
    }

    void effectiveFallsBack()
    {
        const QList<Size> nl{Size::Normal, Size::Large};
        QVERIFY(TextSize::effective(Size::ExtraLarge, nl) == Size::Large);
        QVERIFY(TextSize::effective(Size::Large, nl) == Size::Large);
        QVERIFY(TextSize::effective(Size::Large, {Size::Normal}) == Size::Normal);
        QVERIFY(TextSize::effective(Size::Normal, nl) == Size::Normal);
    }

    void settingText()
    {
        for (Size s : {Size::Normal, Size::Large, Size::ExtraLarge})
            QVERIFY(TextSize::fromSetting(TextSize::toSetting(s)) == s);
        QVERIFY(TextSize::fromSetting("garbage") == Size::Normal);
        QVERIFY(TextSize::fromSetting("") == Size::Normal);
    }
};

QTEST_APPLESS_MAIN(TestTextSize)
#include "tst_textsize.moc"