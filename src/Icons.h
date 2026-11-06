#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// The interface icons: Fluent UI System Icons (Microsoft, MIT licence; I1-I3), drawn from
// their SVG files with Qt SVG. Each icon is one solid colour: the text colour of whatever it
// sits on. Regular for actions and finding your way; Filled when selected and below 16 px.
namespace Icons {

// An icon for a button or label, by its Fluent name without size or style ("search",
// "calendar_add"). ClayStyle colours it to match the button's label; elsewhere it uses the
// palette's text colour.
QIcon icon(const QString &name);

// The Fluent name an icon was made with, or an empty string for any other icon.
QString nameOf(const QIcon &icon);

// One drawing of an icon: `size` in logical pixels (16 or 24 are the drawn sizes; others
// use the nearest one), in one colour, Regular or Filled, for a screen with this
// device-pixel ratio. Cached.
QPixmap pixmap(const QString &name, int size, const QColor &color, bool filled, qreal dpr);

bool exists(const QString &name);

} // namespace Icons
