#include <QtTest>

#include "Theme.h"

using namespace Theme;

class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void textIsReadableEverywhere()
    {
        for (Style s : {Style::Clay, Style::HighContrast})
            for (bool dark : {false, true}) {
                const Colors c = colors(s, dark);
                QVERIFY2(contrastRatio(c.text, c.window) >= 7.0,  "text on window");
                QVERIFY2(contrastRatio(c.text, c.surface) >= 7.0, "text on surface");
                QVERIFY2(contrastRatio(c.mutedText, c.window) >= 4.5, "muted text");
                QVERIFY2(contrastRatio(c.accentText, c.accent) >= 4.5, "button text");
                QVERIFY2(contrastRatio(c.error, c.window) >= 4.5, "error text");
            }
    }

    void highContrastIsStrongest()
    {
        for (bool dark : {false, true}) {
            const Colors c = colors(Style::HighContrast, dark);
            QVERIFY(contrastRatio(c.text, c.window) >= 15.0);
            QVERIFY(contrastRatio(c.border, c.window) >= 15.0);
            QVERIFY(contrastRatio(c.accentText, c.accent) >= 7.0);
        }
    }

    void chartAndMarkersVisible()
    {
        for (Style s : {Style::Clay, Style::HighContrast})
            for (bool dark : {false, true}) {
                const Colors c = colors(s, dark);
                for (const QColor &k : {c.high, c.low, c.rain, c.markWeather, c.markNote, c.markEvent})
                    QVERIFY(contrastRatio(k, c.surface) >= 3.0);
            }
    }

    void settingsRoundTrip()
    {
        QCOMPARE(styleFromSetting(styleSetting(Style::HighContrast)), Style::HighContrast);
        QCOMPARE(styleFromSetting("garbage"), Style::Clay);
        for (Brightness b : {Brightness::Light, Brightness::Dark, Brightness::System})
            QCOMPARE(brightnessFromSetting(brightnessSetting(b)), b);
        QCOMPARE(brightnessFromSetting(""), Brightness::System);
    }

    void markersDifferFromEachOther()
    {
        for (Style s : {Style::Clay, Style::HighContrast})
            for (bool dark : {false, true}) {
                const Colors c = colors(s, dark);
                QVERIFY(c.markWeather != c.markNote);
                QVERIFY(c.markNote != c.markEvent);
                QVERIFY(contrastRatio(c.snow, c.surface) >= 3.0);
            }
    }
};

QTEST_APPLESS_MAIN(TestTheme)
#include "tst_theme.moc"