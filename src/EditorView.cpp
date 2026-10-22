#include "EditorView.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>
#include <functional>
#include <utility>
#include <QCheckBox>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QScrollBar>

#include "Confirm.h"
#include "EventTypes.h"
#include "NumberField.h"
#include "Repository.h"
#include "Units.h"
#include "WeatherIcons.h"
#include "WeatherInput.h"
#include "ThemeManager.h"

namespace {

// A dropdown that never reacts to the mouse wheel, so scrolling the form never
// changes a value. The ignored wheel event passes up to the scroll area, which scrolls.
class NoWheelCombo : public QComboBox
{
public:
    NoWheelCombo()
    {
        setFocusPolicy(Qt::StrongFocus);        // the wheel alone never gives it focus
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        event->ignore();
    }
};

// A dropdown whose first item means "not recorded". Each item carries its
// stored text as hidden data, so we never read the display label back.
QComboBox *makeCombo(const QStringList &items)
{
    auto *c = new NoWheelCombo;
    c->setMinimumHeight(44);
    c->addItem("Not recorded", QString());
    for (const QString &s : items)
        c->addItem(s, s);
    return c;
}

QString valueOf(const QComboBox *c) { return c->currentData().toString(); }

void select(QComboBox *c, const QString &value)
{
    const int i = c->findData(value);
    c->setCurrentIndex(i < 0 ? 0 : i);
}

NumberSpec make(const QString &unit, std::optional<double> min, std::optional<double> max,
                double step, double start,
                std::function<double(double)> toDisplay,
                std::function<double(double)> fromDisplay)
{
    NumberSpec s;
    s.unit = unit;
    s.min = min;
    s.max = max;
    s.step = step;
    s.start = start;
    s.toDisplay = std::move(toDisplay);
    s.fromDisplay = std::move(fromDisplay);
    return s;
}

} // namespace

EditorView::EditorView(Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_repo(repo)
{
    QFont f = font();
    f.setPointSize(13);
    setFont(f);                                  // children inherit it

    m_title = new QLabel;
    m_title->setStyleSheet("font-size: 16pt; font-weight: bold;");

    // Rules and units are filled in by applyUnits() each time the editor opens.
    m_high = new NumberField(NumberSpec{});
    m_low = new NumberField(NumberSpec{});
    m_wind = new NumberField(NumberSpec{});
    m_gust = new NumberField(NumberSpec{});
    m_humidity = new NumberField(NumberSpec{});
    m_pressure = new NumberField(NumberSpec{});
    m_precipAmount = new NumberField(NumberSpec{});

    m_windDir = makeCombo(WeatherInput::windDirections());
    m_gustDir = makeCombo(WeatherInput::windDirections());
    m_precipType = makeCombo(WeatherInput::precipTypes());
    m_cloud = makeCombo(WeatherInput::cloudCovers());

    m_precipUnit = new NoWheelCombo;                // items added by applyUnits()
    m_precipUnit->setMinimumHeight(44);

    m_typeError = new QLabel;
    const auto err = ThemeManager::instance()->colors().error;
    m_typeError->setStyleSheet(QString("color: %1; font-weight: bold;").arg(err.name()));
    m_typeError->hide();

    auto *typeBox = new QWidget;
    auto *typeLayout = new QVBoxLayout(typeBox);
    typeLayout->setContentsMargins(0, 0, 0, 0);
    typeLayout->addWidget(m_precipType);
    typeLayout->addWidget(m_typeError);

    auto *amountBox = new QWidget;
    auto *amountLayout = new QHBoxLayout(amountBox);
    amountLayout->setContentsMargins(0, 0, 0, 0);
    amountLayout->addWidget(m_precipAmount, 0, Qt::AlignTop);
    amountLayout->addWidget(m_precipUnit, 0, Qt::AlignTop);
    amountLayout->addStretch(1);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    form->setVerticalSpacing(14);
    form->addRow("High temperature", m_high);
    form->addRow("Low temperature", m_low);
    form->addRow("Wind speed", m_wind);
    form->addRow("Wind direction", m_windDir);
    form->addRow("Gust speed", m_gust);
    form->addRow("Gust direction", m_gustDir);
    form->addRow("Precipitation type", typeBox);
    form->addRow("Precipitation amount", amountBox);
    form->addRow("Humidity", m_humidity);
    form->addRow("Pressure", m_pressure);
    form->addRow("Cloud cover", m_cloud);

    m_note = new QPlainTextEdit;
    m_note->setMinimumHeight(110);
    m_note->setPlaceholderText("Anything you want to remember about this day");
    form->addRow("Note", m_note);

    auto *eventsBox = new QWidget;
    auto *eventsLayout = new QVBoxLayout(eventsBox);
    eventsLayout->setContentsMargins(0, 0, 0, 0);
    for (const EventType &e : eventTypes()) {
        auto *check = new QCheckBox(e.label);
        check->setMinimumHeight(34);
        eventsLayout->addWidget(check);
        m_eventBoxes.append(check);
        m_eventKeys.append(e.key);
    }
    form->addRow("Events", eventsBox);    

    auto *formHolder = new QWidget;
    formHolder->setLayout(form);
    m_scroll = new QScrollArea;              // the form scrolls on small windows
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidget(formHolder);
    m_scroll->viewport()->setAutoFillBackground(false);   // let the background show through
    formHolder->setAutoFillBackground(false);

    auto *cancel = new QPushButton("Cancel");
    auto *saveButton = new QPushButton("Save");
    m_saveButton = saveButton;
    m_cancelButton = cancel;
    for (auto *b : {cancel, saveButton})
        b->setMinimumSize(140, 52);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(cancel);                  // safe choice on the left
    buttons->addStretch(1);
    buttons->addWidget(saveButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_title);
    layout->addWidget(m_scroll, 1);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, [this] {
        if (confirmDiscard())
            emit finished(false);
    });
    connect(saveButton, &QPushButton::clicked, this, &EditorView::save);
    connect(m_precipType, &QComboBox::currentIndexChanged, this, [this] {
        m_typeError->hide();
        updatePrecipState();
    });
}

void EditorView::setSaveEnabled(bool on)
{
    m_saveButton->setEnabled(on);
}

void EditorView::applyUnits()
{
    const Units::System u = m_units;
    const bool imp = u == Units::System::Imperial;

    auto temp = [u](double v) { return Units::cToDisplay(v, u); };
    auto tempBack = [u](double v) { return Units::displayToC(v, u); };
    m_high->setSpec(make(Units::tempUnit(u), std::nullopt, std::nullopt, 1, imp ? 32 : 0, temp, tempBack));
    m_low->setSpec(make(Units::tempUnit(u), std::nullopt, std::nullopt, 1, imp ? 32 : 0, temp, tempBack));

    m_wind->setSpec(make(Units::windUnit(u), 0.0, std::nullopt, 1, 0,
        [u](double v) { return Units::kmhToDisplay(v, u); },
        [u](double v) { return Units::displayToKmh(v, u); }));

    m_gust->setSpec(make(Units::windUnit(u), 0.0, std::nullopt, 1, 0,
        [u](double v) { return Units::kmhToDisplay(v, u); },
        [u](double v) { return Units::displayToKmh(v, u); }));

    m_humidity->setSpec(make("%", 0.0, 100.0, 1, 50, nullptr, nullptr));

    m_pressure->setSpec(make(Units::pressureUnit(u), std::nullopt, std::nullopt,
        imp ? 0.01 : 1, imp ? 29.92 : 1013,
        [u](double v) { return Units::hpaToDisplay(v, u); },
        [u](double v) { return Units::displayToHpa(v, u); }));

    // Precip amount: its conversion depends on type and the unit dropdown,
    // so open() and save() handle it; the field itself converts nothing.
    m_precipAmount->setSpec(make(QString(), 0.0, std::nullopt,
                                 imp ? 0.05 : 0.5, imp ? 0.1 : 1, nullptr, nullptr));

    m_precipUnit->clear();
    if (imp) {
        m_precipUnit->addItem("in");             // Imperial: inches only
    } else {
        m_precipUnit->addItem("mm");             // index 0
        m_precipUnit->addItem("cm");             // index 1
    }
}

void EditorView::open(const QDate &date)
{
    m_date = date;
    m_units = Units::fromSetting(m_repo.setting("units", "metric"));
    applyUnits();                                // must come before setValue()
    refreshIcons();

    m_title->setText("Weather for " + date.toString("dddd, MMMM d, yyyy"));

    const WeatherDay d = m_repo.weatherFor(date).value_or(WeatherDay{});
    m_high->setValue(d.highC);
    m_low->setValue(d.lowC);
    m_wind->setValue(d.windKmh);
    m_gust->setValue(d.gustKmh);
    m_humidity->setValue(d.humidityPct);
    m_pressure->setValue(d.pressureHpa);
    select(m_windDir, d.windDir);
    select(m_gustDir, d.gustDir);
    select(m_cloud, d.cloud);
    select(m_precipType, d.precipType);

    // Stored in mm. Metric snow is shown in cm (unit index 1), other metric
    // precipitation in mm (index 0). Imperial shows inches for everything.
    const bool snow = d.precipType == "Snow";
    const bool imp = m_units == Units::System::Imperial;
    m_precipUnit->setCurrentIndex(imp ? 0 : (snow ? 1 : 0));
    m_loadedUnit = m_precipUnit->currentIndex();

    if (d.precipMm) {
        const double shown = snow ? Units::snowMmToDisplay(*d.precipMm, m_units)
                                  : Units::mmToDisplay(*d.precipMm, m_units);
        m_precipAmount->setShown(shown, d.precipMm);   // show converted, keep exact mm
    } else {
        m_precipAmount->setShown(std::nullopt, std::nullopt);
    }

    const QString note = m_repo.note(date);
    m_note->setPlainText(note);
    const QSet<QString> events = m_repo.events(date);
    for (int i = 0; i < m_eventBoxes.size(); ++i)
        m_eventBoxes[i]->setChecked(events.contains(m_eventKeys[i]));

    m_hadWeather = m_repo.weatherFor(date).has_value();
    m_hadNote = !note.trimmed().isEmpty();
    m_hadEvents = !events.isEmpty();

    m_typeError->hide();
    updatePrecipState();

    m_loaded = snapshot();
    m_scroll->verticalScrollBar()->setValue(0);
}

void EditorView::refreshIcons()
{
    // Items without a picture get a blank one, so all the text lines up.
    auto apply = [](QComboBox *combo, QString (*nameFor)(const QString &)) {
        constexpr int size = 28;
        combo->setIconSize(QSize(size, size));
        for (int i = 0; i < combo->count(); ++i) {
            const QString name = nameFor(combo->itemData(i).toString());
            combo->setItemIcon(i, name.isEmpty() ? WeatherIcons::blankIcon(size)
                                                 : WeatherIcons::icon(name));
        }
    };
    apply(m_windDir, WeatherIcons::windName);
    apply(m_gustDir, WeatherIcons::windName);
    apply(m_precipType, WeatherIcons::precipName);
    apply(m_cloud, WeatherIcons::cloudName);
}

void EditorView::updatePrecipState()
{
    const bool none = valueOf(m_precipType) == "None";
    m_precipAmount->setInputEnabled(!none);      // type "None": no amount to enter
    m_precipUnit->setEnabled(!none);
    if (none)
        m_precipAmount->setError(QString());
}

QSet<QString> EditorView::checkedEvents() const
{
    QSet<QString> keys;
    for (int i = 0; i < m_eventBoxes.size(); ++i)
        if (m_eventBoxes[i]->isChecked())
            keys.insert(m_eventKeys[i]);
    return keys;
}

EditorView::Snapshot EditorView::snapshot() const
{
    Snapshot s;
    s.high = m_high->text();
    s.low = m_low->text();
    s.wind = m_wind->text();
    s.gust = m_gust->text();
    s.humidity = m_humidity->text();
    s.pressure = m_pressure->text();
    s.amount = m_precipAmount->text();
    s.windDir = valueOf(m_windDir);
    s.gustDir = valueOf(m_gustDir);
    s.precipType = valueOf(m_precipType);
    s.cloud = valueOf(m_cloud);
    s.unit = m_precipUnit->currentText();
    s.note = m_note->toPlainText();
    s.events = checkedEvents();
    return s;
}

bool EditorView::hasUnsavedChanges() const
{
    return !(snapshot() == m_loaded);
}

bool EditorView::confirmDiscard()
{
    if (!hasUnsavedChanges())
        return true;                         // nothing to lose, so no question
    return Confirm::destructive(this, "Discard your changes?",
                                "You have changes that have not been saved.",
                                "Keep editing", "Discard changes");
}

void EditorView::save()
{
    // Check EVERY field so all errors show at once. Writing `ok = ok && f->validate()`
    // would stop at the first error and skip the rest (&& short-circuits).
    bool ok = true;
    ok = m_high->validate() && ok;
    ok = m_low->validate() && ok;
    ok = m_wind->validate() && ok;
    ok = m_gust->validate() && ok;
    ok = m_humidity->validate() && ok;
    ok = m_pressure->validate() && ok;

    const QString type = valueOf(m_precipType);
    const bool none = type == "None";
    QString typeError;
    if (!none) {
        bool amountOk = m_precipAmount->validate();
        if (amountOk) {
            const auto check = WeatherInput::checkPrecipPair(type, m_precipAmount->isBlank());
            typeError = check.typeError;
            if (!check.amountError.isEmpty()) {
                m_precipAmount->setError(check.amountError);
                amountOk = false;
            }
        }
        ok = amountOk && typeError.isEmpty() && ok;
    }
    m_typeError->setText(typeError.isEmpty() ? QString() : "\u26A0 " + typeError);
    m_typeError->setVisible(!typeError.isEmpty());
    if (!ok)
        return;                                  // errors are showing; stay in the editor
    WeatherDay day;
    day.highC = m_high->value();
    day.lowC = m_low->value();
    day.windKmh = m_wind->value();
    day.gustKmh = m_gust->value();
    day.humidityPct = m_humidity->value();
    day.pressureHpa = m_pressure->value();
    day.windDir = valueOf(m_windDir);
    day.gustDir = valueOf(m_gustDir);
    day.cloud = valueOf(m_cloud);
    day.precipType = type;

    // Precipitation is always stored in millimetres, whatever unit was typed.
    if (none) {
        day.precipMm = 0.0;
    } else if (!type.isEmpty()) {
        const bool unitChanged = m_precipUnit->currentIndex() != m_loadedUnit;
        if (m_precipAmount->isUntouched() && !unitChanged) {
            day.precipMm = m_precipAmount->original();
        } else if (const auto typed = m_precipAmount->typedValue()) {
            const bool imp = m_units == Units::System::Imperial;
            const double mm = imp ? Units::displayToMm(*typed, m_units)
                                  : *typed * (m_precipUnit->currentIndex() == 1 ? 10.0 : 1.0);
            day.precipMm = std::round(mm * 1000.0) / 1000.0;
        }
    }

    const QString note = m_note->toPlainText();
    const QSet<QString> events = checkedEvents();
    const bool noteBlank = note.trimmed().isEmpty();

    // Would this save erase something that was stored before?
    QStringList erased;
    if (m_hadWeather && day.isEmpty())
        erased << "the weather you recorded for this day";
    if (m_hadNote && noteBlank)
        erased << "the note for this day";
    if (m_hadEvents && events.isEmpty())
        erased << "the events ticked for this day";
    if (!erased.isEmpty()) {
        const QString what = erased.size() == 1
            ? erased.first()
            : erased.mid(0, erased.size() - 1).join(", ") + " and " + erased.last();
        if (!Confirm::destructive(this, "Erase saved information?",
                                  "Saving will remove " + what + ".",
                                  "Go back", "Erase and save"))
            return;
    }

    bool saved = m_repo.saveWeather(m_date, day);
    saved = m_repo.saveNote(m_date, note) && saved;
    saved = m_repo.saveEvents(m_date, events) && saved;
    if (!saved) {
        QMessageBox::warning(this, "Weather Tracker",
                             "Sorry, this day could not be saved.\n\n"
                             "Your other data is safe. Please try again.");
        return;
    }
    emit finished(true);
}