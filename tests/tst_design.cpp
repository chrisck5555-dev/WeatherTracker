#include <QtTest>

#include "Design.h"
#include "Responsive.h"

using namespace Design;

class TestDesign : public QObject
{
    Q_OBJECT

private slots:
    // S1: the scale itself: multiples of 4, the larger ones multiples of 8, each step at
    // least 25% bigger than the one before.
    void spacingScale()
    {
        int previous = 0;
        for (int v : kSpacingScale) {
            QVERIFY(v % 4 == 0);
            if (v >= 16)
                QVERIFY(v % 8 == 0);
            if (previous > 0)
                QVERIFY(v >= previous * 1.25);
            previous = v;
        }
        QVERIFY(onSpacingScale(0));
        QVERIFY(onSpacingScale(12));
        QVERIFY(!onSpacingScale(6));
        QVERIFY(!onSpacingScale(10));
        QVERIFY(!onSpacingScale(20));
    }

    // S2: every named gap is on the scale.
    void namedGapsOnScale()
    {
        for (int v : {kGapButtons, kGapHeader, kGapLabel, kGapCards, kGapAreas, kPadCard,
                      kIndentSection, kTargetGap, kWindowMargin, kBarInnerPad, kBarGroupGap,
                      kBarRowGap, kBarToPage})
            QVERIFY2(onSpacingScale(v), qPrintable(QString::number(v)));
    }

    // T6: four sizes, line heights on the 4 px grid, Semibold only for Heading and Title.
    void typeSizes()
    {
        QCOMPARE(type(TextStyle::Caption).px, 14);
        QCOMPARE(type(TextStyle::Body).px, 18);
        QCOMPARE(type(TextStyle::Heading).px, 24);
        QCOMPARE(type(TextStyle::Title).px, 32);
        for (auto s : {TextStyle::Caption, TextStyle::Body, TextStyle::Heading, TextStyle::Title}) {
            QVERIFY(type(s).lineHeight % 4 == 0);
            QVERIFY(type(s).lineHeight > type(s).px);
        }
        QVERIFY(!type(TextStyle::Body).semibold);
        QVERIFY(type(TextStyle::Heading).semibold);
    }

    // R2, R3: radii and concentric nesting.
    void radii()
    {
        QCOMPARE(kRadiusControl, 8);
        QCOMPARE(kRadiusCard, 16);
        QCOMPARE(kRadiusContrast, 4);
        QCOMPARE(innerRadius(16, 8), 8.0);       // a 16 shape with 8 padding holds 8 controls
        QCOMPARE(innerRadius(16, 16), 0.0);
        QCOMPARE(innerRadius(8, 12), 0.0);       // never below 0
    }

    // Z1, Z3, Z5: sizes are multiples of 4.
    void sizesOnGrid()
    {
        for (int v : {kTarget, kIconInText, kIconAlone, kIconTwoLine, kCheckBox,
                      kMinWindowWidth, kMinWindowHeight, kCheckedWindowWidth, kCheckedWindowHeight,
                      kParagraphMaxWidth, kListMaxWidth})
            QVERIFY2(v % 4 == 0, qPrintable(QString::number(v)));
        QVERIFY(kTarget >= 44);
    }

    // L9 and K1: bars 60 high, 164 px in all; the undo bar's space 76 px.
    void barsAndUndoSpace()
    {
        QCOMPARE(barHeight(), 60);
        QCOMPARE(barsTotal(), 164);
        QCOMPARE(undoReserve(), 76);
    }

    // M3: durations within the limits; entering slightly longer than leaving.
    void motionLimits()
    {
        for (int ms : {kTapMs, kQuickMs, kPageMs, kPageOutMs, kExitMs, kReducedFadeMs})
            QVERIFY(ms <= kLongestMs);
        QVERIFY(kPageMs >= 150 && kPageMs <= 200);
        QVERIFY(kQuickMs > kExitMs);
        QVERIFY(kPageMovePx <= 24);
        QVERIFY(kZoomFrom >= 0.94 && kZoomTo <= 1.04);
    }

    // RZ9: the three bands and their edges.
    void bands()
    {
        using Responsive::Band;
        QCOMPARE(Responsive::band(512), Band::Narrow);
        QCOMPARE(Responsive::band(639), Band::Narrow);
        QCOMPARE(Responsive::band(640), Band::Medium);
        QCOMPARE(Responsive::band(1007), Band::Medium);
        QCOMPARE(Responsive::band(1008), Band::Wide);
        QCOMPARE(Responsive::band(683), Band::Medium);    // Largest text at 1366 x 768
        QCOMPARE(Responsive::band(1366), Band::Wide);
        QVERIFY(!Responsive::twoColumns(Band::Medium));
        QVERIFY(Responsive::twoColumns(Band::Wide));
        QCOMPARE(Responsive::bandName(Band::Medium), QString("Medium"));
    }
};

QTEST_APPLESS_MAIN(TestDesign)
#include "tst_design.moc"
