#include "ThemeManager.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <QWidget>

#include "ClayStyle.h"
#include "Design.h"
#include "Repository.h"

namespace {
ThemeManager *g_instance = nullptr;
}

ThemeManager::ThemeManager(Repository &repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
    g_instance = this;
    m_style = Theme::styleFromSetting(repo.setting("style", "clay"));
    m_brightness = Theme::brightnessFromSetting(repo.setting("brightness", "system"));
    QApplication::setStyle(new ClayStyle);       // Qt takes ownership
    // Follow Windows live when the choice is System.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_brightness == Theme::Brightness::System)
            apply();
    });
    apply();
}

ThemeManager *ThemeManager::instance() { return g_instance; }

bool ThemeManager::resolveDark() const
{
    if (m_brightness == Theme::Brightness::Dark)  return true;
    if (m_brightness == Theme::Brightness::Light) return false;
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void ThemeManager::setStyle(Theme::Style s)
{
    m_style = s;
    m_repo.setSetting("style", Theme::styleSetting(s));
    apply();
}

void ThemeManager::setBrightness(Theme::Brightness b)
{
    m_brightness = b;
    m_repo.setSetting("brightness", Theme::brightnessSetting(b));
    apply();
}

void ThemeManager::apply()
{
    m_colors = Theme::colors(m_style, resolveDark());
    const auto &c = m_colors;

    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.surface);
    p.setColor(QPalette::AlternateBase, c.window);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.surface);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::BrightText, c.text);
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Dark, c.shadow);
    p.setColor(QPalette::Light, c.highlight);
    p.setColor(QPalette::PlaceholderText, c.mutedText);
    p.setColor(QPalette::ToolTipBase, c.surface);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::Disabled, QPalette::Text, c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.disabledText);
    QApplication::setPalette(p);

    const bool contrast = m_style == Theme::Style::HighContrast;
    const int border = contrast ? Design::kOutlineContrast : Design::kOutline;
    const int radius = contrast ? Design::kRadiusContrast : Design::kRadiusControl;

    // Buttons, tabs and check boxes are painted by ClayStyle in every look. Text boxes get
    // the control radius (R2), 12 px inside including the outline (S1), 44 px high (Z1),
    // and the focus tint (F1) as their background while she types in them.
    const QColor focusFill = Theme::mix(c.surface, c.accent,
                                        contrast ? Design::kFocusTintContrast : Design::kFocusTint);
    const QString otherRules = QString(
        "QLineEdit, QPlainTextEdit, QComboBox, QTextBrowser { background: %1; color: %2; border: %3px solid %4; border-radius: %5px; }"
        "QLineEdit, QComboBox { padding: %6px %7px; min-height: %8px; }"
        "QPlainTextEdit, QTextBrowser { padding: %6px %7px; }"
        "QLineEdit, QPlainTextEdit, QComboBox { border-color: %9; }"   // boxes she types in: easy to see
        "QLineEdit:focus, QPlainTextEdit:focus, QComboBox:focus { background: %10; }"
        "QLineEdit:disabled { color: %11; }"
        "QComboBox::drop-down { subcontrol-origin: content; subcontrol-position: center right; border: none; width: 24px; }"   // EditorView draws the chevron
        "QToolTip { background: %1; color: %2; border: %3px solid %4; padding: %6px %7px; }"
        "QMessageBox { background: %12; }")
        .arg(c.surface.name(), c.text.name())                  // 1, 2
        .arg(border)                                           // 3
        .arg(c.border.name())                                  // 4
        .arg(radius)                                           // 5
        .arg(Design::kGapButtons - border)                     // 6: 8 incl. the outline
        .arg(Design::kGapLabel - border)                       // 7: 12 incl. the outline
        .arg(Design::kTarget - 2 * Design::kGapButtons)        // 8: 28, so the box is 44
        .arg(c.inputBorder.name())                             // 9
        .arg(focusFill.name(), c.disabledText.name())          // 10, 11
        .arg(c.window.name());                                 // 12

    // The outline around the tab pages. Clay draws it itself (ClayStyle, continuous corners).
    const QString paneRule = contrast
        ? QString("QTabWidget::pane { border: %1px solid %2; border-radius: %3px; }")
              .arg(border).arg(c.border.name()).arg(Design::kRadiusContrast)
        : QString();

    qApp->setStyleSheet(otherRules + paneRule);

    emit themeChanged();
    for (QWidget *w : QApplication::allWidgets())
        w->update();
}
