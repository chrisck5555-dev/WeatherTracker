#pragma once

#include <QDate>
#include <QWidget>
#include <QSet>
#include <QList>

#include "Units.h"

class NumberField;
class QComboBox;
class QLabel;
class Repository;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;

class EditorView : public QWidget
{
    Q_OBJECT

public:
    EditorView(Repository &repo, QWidget *parent = nullptr);

    void open(const QDate &date);

    bool hasUnsavedChanges() const;     // anything differs from what open() loaded
    bool confirmDiscard();              // asks if there are changes; true = OK to leave
    void abandon() { emit finished(false); }          // leave without asking and without saving
    void setSaveEnabled(bool on);       // the tour turns Save off
    QPushButton *cancelButton() const { return m_cancelButton; }

signals:
    void finished(bool saved);

private:
    void save();
    void applyUnits();
    void updatePrecipState();
    void refreshIcons();                // pictures in the dropdowns follow the theme

    struct Snapshot {
        QString high, low, wind, humidity, pressure, amount, gust;
        QString windDir, gustDir, precipType, cloud, unit;
        QString note;
        QSet<QString> events;
        bool operator==(const Snapshot &o) const
        {
            return high == o.high && low == o.low && wind == o.wind
                && humidity == o.humidity && pressure == o.pressure && amount == o.amount
                && gust == o.gust && gustDir == o.gustDir && windDir == o.windDir && precipType == o.precipType && cloud == o.cloud
                && unit == o.unit && note == o.note && events == o.events;
        }
    };
    Snapshot snapshot() const;
    QSet<QString> checkedEvents() const;

    Snapshot m_loaded;                  // the form as open() filled it
    bool m_hadWeather = false;          // what the database held when we opened
    bool m_hadNote = false;
    bool m_hadEvents = false;
    QPlainTextEdit *m_note = nullptr;
    QList<QCheckBox *> m_eventBoxes;
    QList<QString> m_eventKeys;         // same order as m_eventBoxes    

    Repository &m_repo;
    QDate m_date;
    Units::System m_units = Units::System::Metric;
    QLabel *m_title = nullptr;
    NumberField *m_high = nullptr;
    NumberField *m_low = nullptr;
    NumberField *m_wind = nullptr;
    NumberField *m_gust = nullptr;
    NumberField *m_humidity = nullptr;
    NumberField *m_pressure = nullptr;
    NumberField *m_precipAmount = nullptr;
    QComboBox *m_windDir = nullptr;
    QComboBox *m_gustDir = nullptr;
    QComboBox *m_precipType = nullptr;
    QComboBox *m_precipUnit = nullptr;
    QComboBox *m_cloud = nullptr;
    QLabel *m_typeError = nullptr;
    int m_loadedUnit = 0;
    QScrollArea *m_scroll = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
};