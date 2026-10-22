#pragma once

#include <QObject>

#include "Theme.h"

class Repository;

// Applies the saved theme to the whole application and follows Windows' light/dark.
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(Repository &repo, QObject *parent = nullptr);

    static ThemeManager *instance();
    Theme::Colors colors() const { return m_colors; }
    Theme::Style style() const { return m_style; }

    void setStyle(Theme::Style s);
    void setBrightness(Theme::Brightness b);
    Theme::Brightness brightness() const { return m_brightness; }

    void apply();                       // (re)build the palette and stylesheet

signals:
    void themeChanged();

private:
    bool resolveDark() const;

    Repository &m_repo;
    Theme::Style m_style = Theme::Style::Clay;
    Theme::Brightness m_brightness = Theme::Brightness::System;
    Theme::Colors m_colors;
};