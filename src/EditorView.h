#pragma once

#include "Repository.h"

#include <QDate>
#include <QHash>
#include <QList>
#include <QSet>
#include <QWidget>
#include <functional>

#include "Units.h"

class ChoiceGroup;
class FieldError;
class NumberField;
class QCheckBox;
class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// The strip pinned at the bottom of the window while the editor is open (plan 2.4): [Save]
// (primary) then [Cancel] at the right (DLG3), with SC4's hard edge above them: a 1 px line
// (3 in high contrast). It's solid: the window's own background, so the form scrolls under
// it and never shows through. MainWindow places it and gives it the background.
class EditorFooter : public QWidget
{
    Q_OBJECT

public:
    explicit EditorFooter(QWidget *parent = nullptr);
    static int footerHeight();                  // 16 + 44 + 16
    void setRightMargin(int right);             // 16, or the scroll bar's width while it shows
    std::function<void(QPainter &, QWidget *)> paintBackground;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    class QHBoxLayout *m_row = nullptr;
};

// The editor for one day (plan 2.4): a heading, then the groups Temperature, Precipitation,
// Wind, Sky, Air, Gusts, Note and Events, in two columns in Wide windows and one otherwise.
// Choices are K10 picture grids (ChoiceGroup); measurements are plain number fields.
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
    void requestSave();                 // Ctrl+S: as pressing Save, if it's enabled
    QPushButton *cancelButton() const { return m_cancelButton; }
    QPushButton *saveButton() const { return m_saveButton; }
    EditorFooter *footer() const { return m_footer; }
    QWidget *firstField() const;        // KB5: where the focus starts
    QWidget *lastField() const;         // the last stop before Save

    // For tests and screenshots.
    QList<ChoiceGroup *> choiceGroups() const { return m_groups; }
    ChoiceGroup *precipTypeGroup() const { return m_precipType; }
    ChoiceGroup *windGroup() const { return m_windDir; }
    NumberField *highField() const { return m_high; }
    NumberField *amountField() const { return m_precipAmount; }
    enum class Arrangement { One, Two, Fit };
    Arrangement arrangement() const { return m_arrangement; }
    bool twoColumns() const { return m_arrangement != Arrangement::One; }
    static int fitWidth();              // the page width Fit needs: 1316

signals:
    void finished(bool saved);
    // A save removed something that was stored before (she confirmed it). `before` is
    // the whole day as it was, so the window can offer Undo.
    void erasedOnSave(const QDate &date, const QString &what, const Repository::DayRecord &before);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void save();
    void applyUnits();
    void updatePrecipState();
    void arrange();                     // columns, choice grids and events for this width

    struct Snapshot {
        QString high, low, wind, humidity, pressure, amount, gust;
        QString windDir, gustDir, precipType, cloud;
        QString note;
        QSet<QString> events;
        bool operator==(const Snapshot &o) const
        {
            return high == o.high && low == o.low && wind == o.wind
                && humidity == o.humidity && pressure == o.pressure && amount == o.amount
                && gust == o.gust && gustDir == o.gustDir && windDir == o.windDir
                && precipType == o.precipType && cloud == o.cloud
                && note == o.note && events == o.events;
        }
    };
    Snapshot snapshot() const;
    QSet<QString> checkedEvents() const;

    Snapshot m_loaded;                  // the form as open() filled it
    bool m_hadWeather = false;          // what the database held when we opened
    bool m_hadNote = false;
    bool m_hadEvents = false;
    QString m_loadedType;               // the precipitation type open() loaded (D54)

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
    ChoiceGroup *m_windDir = nullptr;
    ChoiceGroup *m_gustDir = nullptr;
    ChoiceGroup *m_precipType = nullptr;
    ChoiceGroup *m_cloud = nullptr;
    QList<ChoiceGroup *> m_groups;
    FieldError *m_typeError = nullptr;
    QPlainTextEdit *m_note = nullptr;
    QList<QCheckBox *> m_eventBoxes;
    QList<QString> m_eventKeys;         // same order as m_eventBoxes
    QWidget *m_eventsBox = nullptr;
    QGridLayout *m_eventsGrid = nullptr;
    int m_eventColumns = 0;
    int m_eventStop = 0;                // the event box Tab stops at (KB2)
    void updateEventStop();

    QList<QWidget *> m_blocks;          // Temperature, Precipitation, Wind, Sky, Air, Gusts, Note, Events
    QGridLayout *m_body = nullptr;
    QWidget *m_bodyHolder = nullptr;
    Arrangement m_arrangement = Arrangement::One;
    QWidget *m_lastField = nullptr;     // the last Tab stop in the form
    QHash<QWidget *, QList<QWidget *>> m_blockStops;   // each block's Tab stops, in its own order
    bool m_arranged = false;
    bool m_settling = false;            // arrange() is passing the page its new size

    EditorFooter *m_footer = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
};
