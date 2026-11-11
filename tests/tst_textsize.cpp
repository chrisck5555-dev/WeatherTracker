#include <QtTest>

#include "TextSize.h"

using TextSize::Size;

class TestTextSize : public QObject
{
    Q_OBJECT

private slots:
    // Z4: three sizes, all offered.
    void sizesAndFactors()
    {
        QCOMPARE(TextSize::all().size(), 3);
        QCOMPARE(TextSize::factor(Size::Normal), 1.0);
        QCOMPARE(TextSize::factor(Size::Large), 1.25);
        QCOMPARE(TextSize::factor(Size::ExtraLarge), 1.5);
        QCOMPARE(TextSize::label(Size::ExtraLarge), QString("Extra large"));
    }

    void settingText()
    {
        for (Size s : TextSize::all())
            QVERIFY(TextSize::fromSetting(TextSize::toSetting(s)) == s);
        QVERIFY(TextSize::fromSetting("xlarge") == Size::ExtraLarge);   // saved by 1.1.0
        QVERIFY(TextSize::fromSetting("largest") == Size::ExtraLarge);  // removed in 1.2.0
        QVERIFY(TextSize::fromSetting("garbage") == Size::Normal);
        QVERIFY(TextSize::fromSetting("") == Size::Normal);
    }
};

QTEST_APPLESS_MAIN(TestTextSize)
#include "tst_textsize.moc"
