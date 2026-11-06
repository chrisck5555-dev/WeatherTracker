#pragma once

// Every size, gap, radius, type size and timing in the program, from design-rules.md
// (the rule codes are in the comments). Screens use only these numbers.
//
// Sizes are in "layout px" (rule RZ1): effective pixels at the Normal text size. The Text
// size setting scales everything through QT_SCALE_FACTOR, so in Qt's own units these
// numbers never change.

namespace Design {

// --- S: spacing --------------------------------------------------------------------------

// S1: the only values allowed for margins, padding and gaps. Spacing is measured from a
// shape's outer edge, so a 1 px outline counts as part of the padding.
inline constexpr int kSpacingScale[] = {4, 8, 12, 16, 24, 32, 48};

constexpr bool onSpacingScale(int px)
{
    if (px == 0)
        return true;                            // touching (no gap) is allowed
    for (int v : kSpacingScale)
        if (v == px)
            return true;
    return false;
}

// S2: standard gaps.
inline constexpr int kGapButtons = 8;           // between buttons in a row
inline constexpr int kGapHeader = 8;            // between a control and its header
inline constexpr int kGapLabel = 12;            // between a control and its label beside it
inline constexpr int kGapCards = 16;            // between cards
inline constexpr int kGapAreas = 12;            // between other content areas
inline constexpr int kPadCard = 16;             // card or panel edge to the text inside
inline constexpr int kIndentSection = 48;       // controls inside an expanding section

// --- Z: sizes and targets ----------------------------------------------------------------

inline constexpr int kTarget = 44;              // Z1: everything clickable, at least 44 x 44
inline constexpr int kTargetGap = 8;            // Z2: between neighbouring click targets
inline constexpr int kIconInText = 16;          // Z3: inside buttons and text
inline constexpr int kIconAlone = 24;           // Z3: standing alone
inline constexpr int kIconTwoLine = 32;         // Z3: beside two-line list items
inline constexpr int kCheckBox = 20;            // the box of a check box (a multiple of 4, Z5)

// Z5: the window can shrink to this (layout px) and everything still works by reflowing.
inline constexpr int kMinWindowWidth = 512;
inline constexpr int kMinWindowHeight = 320;
// SC3: from this size up, the important parts fit without scrolling at Normal text.
inline constexpr int kCheckedWindowWidth = 1024;
inline constexpr int kCheckedWindowHeight = 640;

// --- T: text -----------------------------------------------------------------------------

// T6: four type sizes, at most three on any one screen. Regular and Semibold only.
enum class TextStyle { Caption, Body, Heading, Title };

struct TypeSpec {
    int px;                                     // font size in pixels
    int lineHeight;                             // on the 4 px grid
    bool semibold;
};

constexpr TypeSpec type(TextStyle s)
{
    switch (s) {
    case TextStyle::Caption: return {14, 20, false};
    case TextStyle::Body:    return {18, 28, false};
    case TextStyle::Heading: return {24, 32, true};
    case TextStyle::Title:   return {32, 40, true};
    }
    return {18, 28, false};
}

inline constexpr int kMaxTextSizesPerScreen = 3;   // T6
inline constexpr int kParagraphMaxWidth = 540;     // T4: about 60 characters per line
inline constexpr int kListMaxWidth = 640;          // RZ10: label-value lists and tables

// --- R: shape ----------------------------------------------------------------------------

inline constexpr int kRadiusControl = 8;        // R2: buttons, fields, choice buttons
inline constexpr int kRadiusCard = 16;          // R2: cards, floating panels, dialogs
inline constexpr int kRadiusContrast = 4;       // R2: every radius in high contrast
inline constexpr double kCornerSmoothing = 0.6; // R1: continuous corners in the soft looks

// R3: nested corners are concentric. Padding includes the outline.
constexpr double innerRadius(double outer, double padding)
{
    return outer - padding > 0 ? outer - padding : 0;
}

// --- E: elevation and outlines -----------------------------------------------------------

// E1 (Windows elevation values). Controls show theirs with a darker bottom edge, as
// Windows' own controls do; floating parts get real shadows.
inline constexpr int kElevationControl = 2;
inline constexpr int kElevationPressed = 1;
inline constexpr int kElevationCard = 8;
inline constexpr int kElevationBar = 16;        // the toolbar and tab row (G14)
inline constexpr int kElevationPanel = 32;      // Search, Go to date, Ctrl+K
inline constexpr int kElevationDialog = 128;    // dialogs and the undo bar

inline constexpr int kOutline = 1;              // E2: soft looks
inline constexpr int kOutlineContrast = 3;      // E2: high contrast

// --- ST and F: states and focus ----------------------------------------------------------

inline constexpr double kHoverTint = 0.08;      // ST2: toward the text colour
inline constexpr double kPressTint = 0.10;      // ST3
inline constexpr double kFocusTint = 0.30;      // F4: accent, soft looks
inline constexpr double kFocusTintContrast = 0.16; // F4: accent, high contrast

// --- G: glass ----------------------------------------------------------------------------

inline constexpr double kFrostLight = 0.85;     // G3
inline constexpr double kFrostDark = 0.90;      // G3
inline constexpr int kBlurRadius = 24;          // G4
inline constexpr double kGlassBudgetMs = 4.0;   // G11: per frame while something moves
inline constexpr double kSmoke = 0.30;          // DLG7: black behind a dialog that needs an answer

// --- L: layout ---------------------------------------------------------------------------

inline constexpr int kWindowMargin = 16;        // L8
inline constexpr int kBarInnerPad = 8;          // L9: glass edge to its controls
inline constexpr int kBarGroupGap = 12;         // L9: between glass shapes in a row
inline constexpr int kBarRowGap = 12;           // L9: toolbar to tab row
inline constexpr int kBarToPage = 16;           // L9: tab row to the page at rest

// L9: each bar is as high as its tallest control plus 16.
constexpr int barHeight(int tallestControl = kTarget) { return tallestControl + 2 * kBarInnerPad; }

// L9: the space the two bars take from the top of the window at Normal text (164).
constexpr int barsTotal() { return kWindowMargin + barHeight() + kBarRowGap + barHeight() + kBarToPage; }

// K1: the page ends this far above the window's bottom, so the undo bar never covers it.
constexpr int undoReserve(int undoBarHeight = barHeight()) { return undoBarHeight + kWindowMargin; }

// --- RZ: resizing ------------------------------------------------------------------------

inline constexpr int kMediumMinWidth = 640;     // RZ9: Narrow is below this
inline constexpr int kWideMinWidth = 1008;      // RZ9: Wide is this and up
inline constexpr int kRelayoutGlassDelayMs = 150; // RZ4: new blurred copy after resizing stops

// --- M: motion ---------------------------------------------------------------------------

inline constexpr int kTapMs = 100;              // M3: press, hover, small feedback
inline constexpr int kQuickMs = 150;            // panels and the undo bar appearing
inline constexpr int kPageMs = 200;             // page changes (150-200, D5)
inline constexpr int kPageOutMs = 80;           // the old page fading out
inline constexpr int kExitMs = 100;             // panels and the undo bar leaving
inline constexpr int kReducedFadeMs = 120;      // M19: Reduced motion
inline constexpr int kLongestMs = 400;          // M3: nothing longer
inline constexpr int kPageMovePx = 24;          // M15
inline constexpr double kZoomFrom = 0.94;       // M15
inline constexpr double kZoomTo = 1.04;         // M15

// --- K: behaviour ------------------------------------------------------------------------

inline constexpr int kUndoSeconds = 10;         // K1

} // namespace Design
