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
                QVERIFY2(contrastRatio(c.accentText, c.accent) >= 4.5, "selected tab label");
                QVERIFY2(contrastRatio(c.error, c.window) >= 4.5, "error text");
                QVERIFY2(contrastRatio(c.disabledText, c.window) >= 4.5, "disabled labels (ST5)");
                QVERIFY2(contrastRatio(c.disabledText, c.surface) >= 4.5, "disabled labels on cards");
            }
    }

    // WCAG 2.2 AA: 4.5:1 for text, 3:1 for the outlines and rings that show where things are.
    void everythingElseMeetsWcagAA()
    {
        for (Style s : {Style::Clay, Style::HighContrast})
            for (bool dark : {false, true}) {
                const Colors c = colors(s, dark);
                QVERIFY2(contrastRatio(c.mutedText, c.surface) >= 4.5, "muted text on cards");
                QVERIFY2(contrastRatio(c.error, c.surface) >= 4.5, "error text on cards");
                QVERIFY2(contrastRatio(c.accent, c.surface) >= 3.0, "focus ring and today on cards");
                QVERIFY2(contrastRatio(c.accent, c.window) >= 3.0, "focus ring on the page");
                QVERIFY2(contrastRatio(c.inputBorder, c.surface) >= 3.0, "text box outline on cards");
                QVERIFY2(contrastRatio(c.inputBorder, c.window) >= 3.0, "text box outline on the page");
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
                for (const QColor &k : {c.high, c.low, c.rain, c.snow, c.markWeather, c.markNote, c.markEvent})
                    QVERIFY(contrastRatio(k, c.surface) >= 3.0);
            }
    }

    // C5: the accent is only for actions, selection, focus and Today, so charts don't use it.
    void chartColoursAreNotTheAccent()
    {
        for (Style s : {Style::Clay, Style::HighContrast})
            for (bool dark : {false, true}) {
                const Colors c = colors(s, dark);
                QVERIFY(c.low != c.accent);
                QVERIFY(c.rain != c.accent);
                QVERIFY(c.high != c.accent);
            }
    }

    // G3: glass colours, and text on glass over the page's darkest and brightest colours.
    void textOnGlass()
    {
        QCOMPARE(colors(Style::Clay, false).raised, colors(Style::Clay, false).surface);
        QCOMPARE(colors(Style::Clay, true).raised, QColor("#313852"));
        for (bool dark : {false, true}) {
            const Colors c = colors(Style::Clay, dark);
            const double frost = dark ? 0.90 : 0.85;
            for (const QColor &under : {c.text, c.window, c.surface, c.highlight, c.shadow, c.accent}) {
                const QColor glass = mix(under, c.raised, frost);
                QVERIFY2(contrastRatio(c.text, glass) >= 7.0, "main text on glass");
                QVERIFY2(contrastRatio(c.mutedText, glass) >= 4.5, "muted (disabled) text on glass");
            }
        }
    }

    // ST2, ST3, F4: text stays readable under every state overlay.
    void stateOverlaysKeepTextReadable()
    {
        auto over = [](const QColor &base, const QColor &overlay) {
            return mix(base, QColor(overlay.red(), overlay.green(), overlay.blue()), overlay.alphaF());
        };
        for (Style s : {Style::Clay, Style::HighContrast})
            for (bool dark : {false, true}) {
                const Colors c = colors(s, dark);
                const double mainMin = s == Style::HighContrast ? 15.0 : 7.0;
                const double accentMin = s == Style::HighContrast ? 7.0 : 4.5;
                QVERIFY2(contrastRatio(c.text, over(c.surface, hoverOverlay(c))) >= 7.0, "hover");
                QVERIFY2(contrastRatio(c.text, over(c.surface, pressOverlay(c))) >= 7.0, "pressed");
                QVERIFY2(contrastRatio(c.text, over(c.surface, focusOverlay(s, c, false))) >= mainMin, "focus");
                QVERIFY2(contrastRatio(c.accentText, over(c.accent, hoverOverlay(c, c.accent))) >= accentMin, "hover on accent");
                QVERIFY2(contrastRatio(c.accentText, over(c.accent, pressOverlay(c, c.accent))) >= accentMin, "pressed on accent");
                const QColor focused = over(c.accent, focusOverlay(s, c, true));
                QVERIFY2(contrastRatio(c.accentText, focused) >= accentMin, "focus on accent");
                QVERIFY2(contrastRatio(focused, c.accent) >= 1.15, "focus on accent can be seen");
            }
    }

    // C7, D58: hover changes lightness by 5-10 (of 100) on every fill, the accent included,
    // and pressed changes it more.
    void hoverChangesEveryFillAlike()
    {
        auto over = [](const QColor &base, const QColor &overlay) {
            return mix(base, QColor(overlay.red(), overlay.green(), overlay.blue()), overlay.alphaF());
        };
        for (Style s : {Style::Clay, Style::HighContrast})
            for (bool dark : {false, true}) {
                const Colors c = colors(s, dark);
                QCOMPARE(hoverOverlay(c, c.surface).alphaF(), hoverOverlay(c).alphaF());
                const double onSurface = std::abs(lightness(over(c.surface, hoverOverlay(c))) - lightness(c.surface));
                for (const QColor &fill : {c.surface, c.window, c.accent}) {
                    const double hover = std::abs(lightness(over(fill, hoverOverlay(c, fill))) - lightness(fill));
                    const double press = std::abs(lightness(over(fill, pressOverlay(c, fill))) - lightness(fill));
                    QVERIFY2(hover >= 5.0 && hover <= 10.0, "hover within C7");
                    QVERIFY2(hover >= onSurface - 0.1, "hover as clear as on the surface");
                    QVERIFY2(press > hover, "pressed stronger than hover");
                }
            }
        QCOMPARE(qRound(lightness(QColor("#ffffff"))), 100);
        QCOMPARE(qRound(lightness(QColor("#000000"))), 0);
    }

    void mixAndSmoke()
    {
        QCOMPARE(mix(QColor("#000000"), QColor("#ffffff"), 0.0), QColor("#000000"));
        QCOMPARE(mix(QColor("#000000"), QColor("#ffffff"), 1.0), QColor("#ffffff"));
        QCOMPARE(smoke().alpha(), 77);
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
