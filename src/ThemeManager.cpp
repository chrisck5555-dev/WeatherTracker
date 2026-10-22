#include "ThemeManager.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <QWidget>

#include "ClayStyle.h"
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
    p.setColor(QPalette::Disabled, QPalette::Text, c.mutedText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.mutedText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.mutedText);
    QApplication::setPalette(p);

    const bool contrast = m_style == Theme::Style::HighContrast;
    const int radius = contrast ? 4 : 16;
    const int border = contrast ? 3 : 1;

    // High contrast: flat outlined buttons and tabs from the stylesheet.
    // Clay: ClayStyle paints them, so no button or tab rules here.
    const QString contrastRules = contrast ? QString(
        "QPushButton { background: %1; color: %2; border: 3px solid %3; border-radius: 4px; padding: 6px 14px; }"
        "QPushButton:checked { background: %4; color: %5; border-color: %4; }"
        "QPushButton:disabled { color: %6; }"
        "QTabBar::tab { background: %1; color: %2; border: 3px solid %3; border-radius: 4px; padding: 8px 22px; margin-right: 4px; }"
        "QTabBar::tab:selected { background: %4; color: %5; }")
        .arg(c.surface.name(), c.text.name(), c.border.name())
        .arg(c.accent.name(), c.accentText.name(), c.mutedText.name())
        : QString();

    const QString otherRules = QString(
        "QLineEdit, QPlainTextEdit, QComboBox, QTextBrowser { background: %1; color: %2; border: %3px solid %4; border-radius: %5px; padding: 4px; }"
        "QLineEdit:disabled { color: %6; }"
        "QTabWidget::pane { border: %3px solid %4; border-radius: %7px; }"
        "QMessageBox { background: %8; }")
        .arg(c.surface.name(), c.text.name())                  // 1, 2
        .arg(border)                                           // 3
        .arg(c.border.name())                                  // 4
        .arg(radius / 2)                                       // 5
        .arg(c.mutedText.name())                               // 6
        .arg(radius)                                           // 7
        .arg(c.window.name());                                 // 8

    qApp->setStyleSheet(contrastRules + otherRules);

    emit themeChanged();
    for (QWidget *w : QApplication::allWidgets())
        w->update();
}