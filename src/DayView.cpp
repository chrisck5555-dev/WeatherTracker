#include "DayView.h"

#include <QBoxLayout>
#include <QGridLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QLocale>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShowEvent>
#include <algorithm>
#include <cmath>

#include "AppState.h"
#include "Artwork.h"
#include "Card.h"
#include "Design.h"
#include "EventTypes.h"
#include "Format.h"
#include "Icons.h"
#include "Repository.h"
#include "Responsive.h"
#include "Typography.h"
#include "Units.h"
#include "WeekStrip.h"

namespace {

// RZ10: the Day view's cards stretch with the window, the banner growing with its card.
constexpr int kBannerMin = 112, kBannerMax = 200;
constexpr double kBannerShare = 0.18;   // of the card's width
constexpr int kEmptyMax = 720;          // the empty state's text column
constexpr int kEarlierYears = 5;        // rows in "On this day in earlier years"
constexpr int kNoteLines = 5;           // the Note card's room before it grows
constexpr int kTwoColumnDetails = 700;  // the Weather card's width for two columns of details
constexpr int kDetailColumnGap = 24;    // between the two columns: wider than label to value (S3)
constexpr int kDeleteGap = 24;          // between the cards, "This week" and Delete day (B4)

// The day's picture across the top of the Weather card (IL1), built from the banner squares
// for this date. It only paints, so it needs no moc.
class BannerStrip : public QWidget
{
public:
    explicit BannerStrip(QWidget *parent = nullptr) : QWidget(parent) {}

    void setPicture(const QDate &date, BannerPlan::Sky sky, bool snowGround)
    {
        m_date = date;
        m_sky = sky;
        m_snow = snowGround;
        update();
    }

protected:
    // Taller on big windows: 18% of its width, 112 to 200 px, on the 4 px grid (RZ10).
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        const int h = std::clamp(int(std::lround(width() * kBannerShare / 4.0)) * 4, kBannerMin, kBannerMax);
        if (h != height())
            setFixedHeight(h);
    }

    void paintEvent(QPaintEvent *) override
    {
        const QPixmap picture = Artwork::banner(m_date, m_sky, m_snow, size(), devicePixelRatioF());
        if (picture.isNull())
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (const auto *card = dynamic_cast<const Card *>(parentWidget()))
            p.setClipPath(card->topPictureClip(this));      // the card's corners (R3, R5)
        p.drawPixmap(QPointF(0, 0), picture);
    }

private:
    QDate m_date;
    BannerPlan::Sky m_sky = BannerPlan::Sky::Plain;
    bool m_snow = false;
};

QLabel *label(const QString &text, Design::TextStyle style, bool muted = false, bool wrap = true)
{
    auto *l = new QLabel(text);
    l->setFont(textFont(style, l->font()));
    l->setWordWrap(wrap);
    l->setTextFormat(Qt::PlainText);
    if (muted)
        l->setForegroundRole(QPalette::PlaceholderText);    // the look's muted text, 4.5:1
    return l;
}

void setValue(QLabel *l, const QString &text)
{
    l->setText(text);
    // "Not recorded" is quieter than a value (H3).
    l->setForegroundRole(text == Format::notRecorded() ? QPalette::PlaceholderText
                                                       : QPalette::WindowText);
}

// The heading under the banner (IL1: the condition in words, C3).
QString conditionHeading(const WeatherDay &w, Units::System units)
{
    QStringList parts;
    if (!w.precipType.isEmpty() && w.precipType != "None")
        parts << w.precipType;
    else if (!w.cloud.isEmpty())
        parts << w.cloud;
    // Each temperature stays on one line when the heading wraps.
    auto keep = [](QString t) { return t.replace(' ', QChar(0x00A0)); };
    if (w.highC)
        parts << keep("High " + Format::temperature(w.highC, units));
    if (w.lowC)
        parts << keep("Low " + Format::temperature(w.lowC, units));
    return parts.isEmpty() ? QStringLiteral("Weather recorded") : parts.join(QString::fromUtf8(" · "));
}

} // namespace

DayView::DayView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_repo(repo)
{
    // --- The Weather card ---
    m_weatherCard = new Card;
    m_weatherCard->setAccessibleName("Weather");
    m_banner = new BannerStrip;
    m_banner->setFixedHeight(kBannerMin);
    m_weatherCard->setTopPicture(m_banner);
    m_condition = label(QString(), Design::TextStyle::Heading);
    m_editButton = new QPushButton(Icons::icon("edit"), "Edit this day");
    m_editButton->setToolTip("Change this day's weather, note and events.");
    auto *headingRow = new QHBoxLayout;
    headingRow->setContentsMargins(0, 0, 0, 0);
    headingRow->setSpacing(Design::kGapCards);
    headingRow->addWidget(m_condition, 1);
    headingRow->addWidget(m_editButton, 0, Qt::AlignTop);
    m_weatherCard->body()->setSpacing(Design::kGapAreas);   // S3: heading closer to its details
    m_weatherCard->body()->addLayout(headingRow);
    m_details = new QWidget;
    auto *columns = m_detailColumnsBox = new QHBoxLayout(m_details);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(kDetailColumnGap);
    for (QGridLayout *&grid : m_detailGrids) {
        grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(Design::kGapLabel);    // S2: a value to its label
        grid->setVerticalSpacing(Design::kGapButtons);
        grid->setColumnStretch(1, 1);
        columns->addLayout(grid, 1);
    }
    for (const char *name : {"Wind", "Gusts", "Precipitation", "Cloud cover", "Humidity", "Pressure"}) {
        m_detailLabels << label(name, Design::TextStyle::Body, true, false);
        m_detailValues << label(QString(), Design::TextStyle::Body);
    }
    m_weatherCard->body()->addWidget(m_details);

    // --- The Note and Events cards ---
    auto makeCard = [](const QString &title, QLabel *&text) {
        auto *card = new Card;
        card->setAccessibleName(title);
        card->body()->addWidget(label(title, Design::TextStyle::Heading));
        text = label(QString(), Design::TextStyle::Body);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        card->body()->addWidget(text);
        return card;
    };
    m_noteCard = makeCard("Note", m_noteText);
    m_earlierCard = new Card;
    m_earlierCard->setAccessibleName("On this day in earlier years");
    m_earlierCard->body()->addWidget(label("On this day in earlier years", Design::TextStyle::Heading));
    m_earlierEmpty = label("No weather was recorded on this date in earlier years.",
                           Design::TextStyle::Body, true);
    m_earlierCard->body()->addWidget(m_earlierEmpty);
    auto *table = new QWidget;
    m_earlierGrid = new QGridLayout(table);
    m_earlierGrid->setContentsMargins(0, 0, 0, 0);
    m_earlierGrid->setHorizontalSpacing(Design::kGapLabel);
    m_earlierGrid->setVerticalSpacing(Design::kGapButtons);
    m_earlierGrid->setColumnStretch(3, 1);
    m_earlierCard->body()->addWidget(table);
    m_noteText->setMaximumWidth(Design::kParagraphMaxWidth);             // T4
    // Room for a few lines, so the card keeps its size from day to day; longer notes grow it.
    m_noteText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_noteText->setMinimumHeight(kNoteLines * QFontMetrics(m_noteText->font()).lineSpacing());

    // Events: two columns with room for all six, so the card never changes size.
    m_eventsCard = new Card;
    m_eventsCard->setAccessibleName("Events");
    m_eventsCard->body()->addWidget(label("Events", Design::TextStyle::Heading));
    auto *eventsBox = new QWidget;
    m_eventsGrid = new QGridLayout(eventsBox);
    m_eventsGrid->setContentsMargins(0, 0, 0, 0);
    m_eventsGrid->setHorizontalSpacing(kDetailColumnGap);
    m_eventsGrid->setVerticalSpacing(Design::kGapButtons);
    m_eventsEmpty = label("No events for this day.", Design::TextStyle::Body, true);
    for (const EventType &e : eventTypes())
        m_eventLabels << label(e.label, Design::TextStyle::Body, false, false);
    const int rows = int((eventTypes().size() + 1) / 2);
    const int line = m_eventsEmpty->sizeHint().height();
    eventsBox->setFixedHeight(rows * line + (rows - 1) * Design::kGapButtons);
    m_eventsGrid->setColumnStretch(0, 1);
    m_eventsGrid->setColumnStretch(1, 1);
    m_eventsCard->body()->addWidget(eventsBox);

    m_leftColumn = new QWidget;
    auto *left = new QVBoxLayout(m_leftColumn);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(kDeleteGap);
    left->addWidget(m_weatherCard);
    left->addStretch(1);
    m_leftLayout = left;
    m_rightColumn = new QWidget;
    auto *right = new QVBoxLayout(m_rightColumn);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(Design::kGapCards);
    right->addWidget(m_noteCard);
    right->addWidget(m_eventsCard);
    right->addWidget(m_earlierCard);
    right->addStretch(1);

    // Deleting sits apart at the bottom right, quiet, so it isn't pressed by accident (B4).
    m_deleteButton = new QPushButton(Icons::icon("delete"), "Delete day");
    m_deleteButton->setProperty("wtKind", "quiet");
    m_deleteButton->setToolTip("Deletes this day's weather, note and events. "
                               "You can undo it right after.");

    m_filled = new QWidget;
    auto *filled = new QVBoxLayout(m_filled);
    filled->setContentsMargins(0, 0, 0, 0);
    filled->setSpacing(kDeleteGap);
    m_cardsBox = new QBoxLayout(QBoxLayout::TopToBottom);
    m_cardsBox->setSpacing(Design::kGapCards);
    m_cardsBox->addWidget(m_leftColumn);
    m_cardsBox->addWidget(m_rightColumn);
    filled->addLayout(m_cardsBox);

    // --- The empty day (EM1, IL2): headline, picture, explanation, then the button ---
    m_empty = new QWidget;
    auto *empty = new QVBoxLayout(m_empty);
    empty->setContentsMargins(0, 0, 0, 0);
    empty->setSpacing(Design::kGapCards);
    auto *emptyHead = label("Nothing recorded for this day", Design::TextStyle::Heading);
    m_emptyPicture = new QLabel;
    m_emptyPicture->setAccessibleName("Picture: an empty calendar page");
    auto *emptyText = label("This day's weather, note and events will show here once you add them.",
                            Design::TextStyle::Body);
    emptyText->setMaximumWidth(Design::kParagraphMaxWidth);
    auto *emptyAdd = new QPushButton(Icons::icon("calendar_add"), "Add this day's weather");
    emptyAdd->setToolTip("Record the weather, a note or events for this day.");
    empty->addWidget(emptyHead);
    empty->addWidget(m_emptyPicture, 0, Qt::AlignLeft);
    empty->addWidget(emptyText);
    empty->addWidget(emptyAdd, 0, Qt::AlignLeft);
    m_empty->hide();

    // Both fill the width up to their own limit, from the left (H1, RZ10).
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(m_filled, 1);
    row->addWidget(m_empty, 1);
    row->addStretch(0);
    // "This week" (D63): under the Weather card, or under the empty state (placeWeek).
    m_week = new QWidget;
    m_weekStrip = new WeekStrip(m_repo);
    auto *week = new QVBoxLayout(m_week);
    week->setContentsMargins(0, 0, 0, 0);
    week->setSpacing(Design::kGapHeader);
    week->addWidget(label("This week", Design::TextStyle::Heading));
    week->addWidget(m_weekStrip);
    auto *layout = m_rootLayout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kDeleteGap);
    layout->addLayout(row);
    layout->addWidget(m_deleteButton, 0, Qt::AlignRight);
    layout->addStretch(1);
    placeWeek(false);

    const auto requestEdit = [this] { emit editRequested(m_state.currentDate()); };
    connect(m_editButton, &QPushButton::clicked, this, requestEdit);
    connect(emptyAdd, &QPushButton::clicked, this, requestEdit);
    connect(m_deleteButton, &QPushButton::clicked, this,
            [this] { emit deleteRequested(m_state.currentDate()); });
    connect(m_weekStrip, &WeekStrip::dayActivated, this,
            [this](const QDate &d) { m_state.setCurrentDate(d); });
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible())                 // hidden views refresh when shown instead
            refresh();
    });
    arrange();
}

// "This week" sits under the Weather card, in its column; on an empty day, under the
// empty state.
void DayView::placeWeek(bool empty)
{
    if (m_weekPlaced && m_weekUnderEmpty == empty)
        return;
    if (m_weekPlaced)
        (m_weekUnderEmpty ? static_cast<QLayout *>(m_rootLayout) : m_leftLayout)->removeWidget(m_week);
    m_weekPlaced = true;
    m_weekUnderEmpty = empty;
    if (empty)
        m_rootLayout->insertWidget(1, m_week);
    else
        m_leftLayout->insertWidget(1, m_week);
    m_week->show();
}

void DayView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    arrange();
    refresh();
}

void DayView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    arrange();
}

void DayView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    Card::paintShadows(p, this);          // E1: the cards' shadows fall on the page
}

void DayView::arrange()
{
    // RZ9: the cards go side by side only in the Wide band.
    const bool side = Responsive::twoColumns(Responsive::band(window()->width()));
    if (!m_arranged || side != m_sideBySide) {
        m_arranged = true;
        m_sideBySide = side;
        m_cardsBox->setDirection(side ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
        m_cardsBox->setStretch(0, side ? 3 : 0);
        m_cardsBox->setStretch(1, side ? 2 : 0);
        m_empty->setMaximumWidth(kEmptyMax);
    }
    // The details take two columns when the card has room for them (RZ9: grids adapt by
    // what fits). The Weather card's width follows the page's (3/5 of it beside the others).
    const int cardWidth = side ? (width() - Design::kGapCards) * 3 / 5 : width();
    const int columns = cardWidth >= kTwoColumnDetails ? 2 : 1;
    if (columns == m_detailColumns)
        return;
    m_detailColumns = columns;
    m_detailColumnsBox->setStretch(1, columns == 2 ? 1 : 0);   // one column: all the width
    const int perColumn = columns == 2 ? 3 : 6;
    for (int i = 0; i < m_detailLabels.size(); ++i) {
        for (QGridLayout *grid : m_detailGrids) {
            grid->removeWidget(m_detailLabels[i]);
            grid->removeWidget(m_detailValues[i]);
        }
        QGridLayout *grid = m_detailGrids[i / perColumn];
        grid->addWidget(m_detailLabels[i], i % perColumn, 0, Qt::AlignTop);
        grid->addWidget(m_detailValues[i], i % perColumn, 1, Qt::AlignTop);
    }
}

void DayView::refresh()
{
    const QDate date = m_state.currentDate();
    const auto weather = m_repo.weatherFor(date);
    const QString note = m_repo.note(date);
    const QSet<QString> events = m_repo.events(date);
    const auto units = Units::fromSetting(m_repo.setting("units", "metric"));

    m_weekStrip->setWeek(date, units);
    const bool empty = !weather && note.trimmed().isEmpty() && events.isEmpty();
    placeWeek(empty);
    m_empty->setVisible(empty);
    m_filled->setVisible(!empty);
    m_deleteButton->setVisible(!empty);
    if (empty) {
        const QPixmap picture = Artwork::emptyDay(112, devicePixelRatioF());
        m_emptyPicture->setPixmap(picture);
        m_emptyPicture->setVisible(!picture.isNull());
        return;
    }

    // The banner, in the soft looks (IL1). It matches what was recorded, or is the plain one.
    const auto sky = weather ? BannerPlan::skyFor(weather->precipType, weather->cloud)
                             : BannerPlan::Sky::Plain;
    const bool snowGround = (weather && weather->precipType == "Snow") || m_repo.snowCoverOn(date);
    const bool art = Artwork::bannerAvailable();
    static_cast<BannerStrip *>(m_banner)->setPicture(date, sky, snowGround);
    m_banner->setVisible(art);
    QString pictureName = "Picture: " + BannerPlan::skyWords(sky);
    if (snowGround && sky != BannerPlan::Sky::Snow)
        pictureName += ", snow on the ground";
    m_banner->setAccessibleName(pictureName);                         // F3

    m_details->setVisible(bool(weather));
    if (weather) {
        m_condition->setText(conditionHeading(*weather, units));
        const QStringList values{
            Format::wind(weather->windKmh, weather->windDir, units),     // CF4: "going to"
            Format::wind(weather->gustKmh, weather->gustDir, units),
            Format::precipitation(weather->precipType, weather->precipMm, units),
            Format::text(weather->cloud),
            Format::humidity(weather->humidityPct),
            Format::pressure(weather->pressureHpa, units),
        };
        for (int i = 0; i < values.size(); ++i)
            setValue(m_detailValues[i], values[i]);
    } else {
        m_condition->setText("No weather recorded");
    }

    if (note.trimmed().isEmpty()) {
        m_noteText->setText("No note for this day.");
        m_noteText->setForegroundRole(QPalette::PlaceholderText);
    } else {
        m_noteText->setText(note.trimmed());
        m_noteText->setForegroundRole(QPalette::WindowText);
    }

    // The day's events in their fixed order, across then down.
    m_eventsGrid->removeWidget(m_eventsEmpty);
    m_eventsEmpty->hide();
    for (QLabel *l : m_eventLabels) {
        m_eventsGrid->removeWidget(l);
        l->hide();
    }
    int shown = 0;
    for (int i = 0; i < eventTypes().size(); ++i) {
        if (!events.contains(eventTypes()[i].key))
            continue;
        m_eventsGrid->addWidget(m_eventLabels[i], shown / 2, shown % 2, Qt::AlignLeft | Qt::AlignTop);
        m_eventLabels[i]->show();
        ++shown;
    }
    if (shown == 0) {
        m_eventsGrid->addWidget(m_eventsEmpty, 0, 0, 1, 2, Qt::AlignLeft | Qt::AlignTop);
        m_eventsEmpty->show();
    }

    refreshEarlierYears(date, units);
}

// "On this day in earlier years": the same date's high, low and precipitation, newest first
// (TB4), for the last five years that have it. Units in the headers, one decimal (TB1, TB2).
void DayView::refreshEarlierYears(const QDate &date, Units::System units)
{
    while (QLayoutItem *item = m_earlierGrid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QList<QPair<QDate, WeatherDay>> rows;
    const auto first = m_repo.firstWeatherDate();
    for (int back = 1; first && rows.size() < kEarlierYears; ++back) {
        const QDate earlier = date.addYears(-back);       // February 29 becomes the 28th
        if (!earlier.isValid() || earlier.year() < first->year())
            break;
        if (const auto w = m_repo.weatherFor(earlier))
            rows << qMakePair(earlier, *w);
    }
    m_earlierEmpty->setVisible(rows.isEmpty());
    m_earlierGrid->parentWidget()->setVisible(!rows.isEmpty());
    if (rows.isEmpty())
        return;

    const QString unit = Units::tempUnit(units);
    const QStringList headers{"Year", "High (" + unit + ")", "Low (" + unit + ")", "Precipitation"};
    for (int c = 0; c < headers.size(); ++c)
        m_earlierGrid->addWidget(label(headers[c], Design::TextStyle::Body, true, false), 0, c,
                                 (c == 1 || c == 2) ? Qt::AlignRight | Qt::AlignTop : Qt::AlignLeft | Qt::AlignTop);
    auto temperature = [units](const std::optional<double> &c) {
        if (!c)
            return QString::fromUtf8("\u2013");                 // nothing recorded
        QString t = QString::number(Units::cToDisplay(*c, units), 'f', 1);
        return t == "-0.0" ? QStringLiteral("0.0") : t;
    };
    for (int r = 0; r < rows.size(); ++r) {
        const QDate &d = rows[r].first;
        const WeatherDay &w = rows[r].second;
        QString year = QString::number(d.year());
        if (d.day() != date.day())
            year += " (" + QLocale().toString(d, "MMMM d") + ")";
        const QStringList cells{year, temperature(w.highC), temperature(w.lowC),
                                Format::precipitation(w.precipType, w.precipMm, units)};
        for (int c = 0; c < cells.size(); ++c) {
            QLabel *cell = label(cells[c], Design::TextStyle::Body, false, c == 3);
            cell->setFont(tabularDigits(cell->font()));       // T5
            if (cells[c] == QString::fromUtf8("\u2013") || cells[c] == Format::notRecorded())
                cell->setForegroundRole(QPalette::PlaceholderText);
            m_earlierGrid->addWidget(cell, r + 1, c,
                                     (c == 1 || c == 2) ? Qt::AlignRight | Qt::AlignTop : Qt::AlignLeft | Qt::AlignTop);
        }
    }
}
