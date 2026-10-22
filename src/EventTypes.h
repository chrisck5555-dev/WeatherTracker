#pragma once

#include <QList>
#include <QString>

// The fixed list of per-day events (Requirements §4). The key is what the
// database stores; the label is only for display, so renaming a label later
// never orphans old data.
struct EventType {
    QString key;
    QString label;
};

inline const QList<EventType> &eventTypes()
{
    static const QList<EventType> list = {
        {"first_snow",         "First snow"},
        {"snow_cover_starts",  "Snow cover starts"},
        {"all_melted",         "All melted"},
        {"last_snow",          "Last snow"},
        {"first_frost",        "First frost"},
        {"last_frost",         "Last frost"},
    };
    return list;
}