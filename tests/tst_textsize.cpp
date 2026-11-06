#include <QtTest>

#include "TextSize.h"

using TextSize::Size;

class TestTextSize : public QObject
{
    Q_OBJECT

private slots:
    // Z4: four sizes, all offered.
    void sizesAndFactors()
    {
        QCOMPARE(TextSize::all().size(), 4);
        QCOMPARE(TextSize::factor(Size::Normal), 1.0);
        QCOMPARE(TextSize::factor(Size::Large), 1.25);
        QCOMPARE(TextSize::factor(Size::ExtraLarge), 1.5);
        QCOMPARE(TextSize::factor(Size::Largest), 2.0);
        QCOMPARE(TextSize::label(Size::Largest), QString("Largest"));
    }

    void settingText()
    {
        for (Size s : TextSize::all())
            QVERIFY(TextSize::fromSetting(TextSize::toSetting(s)) == s);
        QVERIFY(TextSize::fromSetting("xlarge") == Size::ExtraLarge);   // saved by 1.1.0
        QVERIFY(TextSize::fromSetting("garbage") == Size::Normal);
        QVERIFY(TextSize::fromSetting("") == Size::Normal);
    }
};

QTEST_APPLESS_MAIN(TestTextSize)
#include "tst_textsize.moc"
