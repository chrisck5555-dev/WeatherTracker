#include "EditorView.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

#include "ChoiceGroup.h"
#include "ClayPaint.h"
#include "Confirm.h"
#include "Design.h"
#include "EventTypes.h"
#include "FieldError.h"
#include "Icons.h"
#include "KeyboardNav.h"
#include "NumberField.h"
#include "Repository.h"
#include "Responsive.h"
#include "ThemeManager.h"
#include "Typography.h"
#include "Units.h"
#include "WeatherIcons.h"
#include "WeatherInput.h"

namespace {

constexpr int kBetweenGroups = 32;              // S3: between groups
constexpr int kColumnGap = 48;                  // between the two columns
constexpr int kBetweenItems = 16;               // inside a group, between its fields
constexpr int kTitleToForm = 24;

NumberSpec make(const QString &unit, std::optional<double> min, std::optional<double> max,
                std::function<double(double)> toDisplay = nullptr,
                std::function<double(double)> fromDisplay = nullptr)
{
    NumberSpec s;
    s.unit = unit;
    s.spokenUnit = WeatherInput::spokenUnit(unit);
    s.min = min;
    s.max = max;
    s.toDisplay = std::move(toDisplay);
    s.fromDisplay = std::move(fromDisplay);
    return s;
}

QLabel *heading(const QString &text)
{
    auto *l = new QLabel(text);
    l->setStyleSheet(textStyleSheet(Design::TextStyle::Heading));   // FM2
    return l;
}

// A group (FM2): its heading, then its contents 8 px below (S2, S3).
QVBoxLayout *group(QWidget *block, const QString &title)
{
    auto *v = new QVBoxLayout(block);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(Design::kGapHeader);
    v->addWidget(heading(title));
    return v;
}

// Two number fields side by side, each half of the column so an error under one wraps in
// its own half and never pushes the other sideways.
QHBoxLayout *pair(NumberField *a, NumberField *b)
{
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kBetweenGroups);
    row->addWidget(a, 1, Qt::AlignTop);
    if (b)
        row->addWidget(b, 1, Qt::AlignTop);
    else
        row->addStretch(1);
    return row;
}

const QString &sideName(const QString &side)
{
    static const QHash<QString, QString> names = {
        {"N", "north"}, {"NE", "northeast"}, {"E", "east"}, {"SE", "southeast"},
        {"S", "south"}, {"SW", "southwest"}, {"W", "west"}, {"NW", "northwest"}};
    static const QString none;
    const auto it = names.constFind(side);
    return it == names.constEnd() ? none : *it;
}

// CF4: each button names the side the wind blows toward, and its arrow points that way.
QList<Choice> compass()
{
    QList<Choice> list;
    for (const QString &side : WeatherInput::windDirections()) {
        QString spoken = sideName(side);
        spoken[0] = spoken[0].toUpper();
        list << Choice{side, side, spoken, WeatherIcons::windToName(side)};
    }
    return list;
}

QList<Choice> precipChoices()
{
    QList<Choice> list;
    for (const QString &t : WeatherInput::precipTypes())
        list << Choice{t, t, QString(), WeatherIcons::choicePrecipName(t)};
    return list;
}

QList<Choice> cloudChoices()
{
    QList<Choice> list;
    for (const QString &c : WeatherInput::cloudCovers())
        list << Choice{c, c, QString(), WeatherIcons::cloudName(c)};
    return list;
}

} // namespace

// --- EditorFooter ---------------------------------------------------------------------------

EditorFooter::EditorFooter(QWidget *parent) : QWidget(parent)
{
    m_row = new QHBoxLayout(this);
    m_row->setContentsMargins(Design::kWindowMargin, Design::kPadCard, Design::kWindowMargin,
                              Design::kWindowMargin);
    m_row->setSpacing(Design::kGapButtons);
    m_row->addStretch(1);
    setFixedHeight(footerHeight());
    // The layout check: it's pinned over the page on purpose, and its margins place the
    // buttons at the window's edge (16, or the scroll bar's 17), not spacing.
    setProperty("wtFloats", true);
    setProperty("wtPlacement", true);
}

int EditorFooter::footerHeight() { return Design::kPadCard + Design::kTarget + Design::kWindowMargin; }

void EditorFooter::setRightMargin(int right)
{
    const QMargins m = m_row->contentsMargins();
    if (m.right() != right)
        m_row->setContentsMargins(m.left(), m.top(), right, m.bottom());
    update();
}

void EditorFooter::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.save();
    if (paintBackground)
        paintBackground(p, this);
    else
        p.fillRect(rect(), palette().color(QPalette::Window));
    p.restore();
    // SC4's hard edge: E2's outline, across the page's width.
    const int line = ClayPaint::outlineWidth();
    const QColor c = ThemeManager::instance() ? ThemeManager::instance()->colors().border
                                              : palette().color(QPalette::Mid);
    const QMargins m = m_row->contentsMargins();
    p.fillRect(QRect(m.left(), 0, width() - m.left() - m.right(), line), c);
}

// --- EditorView -----------------------------------------------------------------------------

EditorView::EditorView(Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_repo(repo)
{
    setFont(textFont(Design::TextStyle::Body, font()));     // children inherit it
    ChoiceGroup::setPictureSource(&WeatherIcons::pixmap);   // pictures follow the look

    // --- The heading and its one muted line (FM10: nothing is required) ---
    m_title = new QLabel;
    m_title->setStyleSheet(textStyleSheet(Design::TextStyle::Heading));
    auto *intro = new QLabel("Fill in what you know and leave the rest empty. Nothing is required.");
    intro->setForegroundRole(QPalette::PlaceholderText);    // the muted text role
    intro->setWordWrap(true);                               // one line, wrapping only when narrow
    auto *titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(4);
    titleBox->addWidget(m_title);
    titleBox->addWidget(intro);

    // --- The fields. Units and limits are set by applyUnits() each time it opens ---
    m_high = new NumberField("High temperature", "High temperature");
    m_low = new NumberField("Low temperature", "Low temperature");
    m_wind = new NumberField("Speed", "Wind speed");
    m_gust = new NumberField("Speed", "Gust speed");
    m_humidity = new NumberField("Humidity", "Humidity");
    m_pressure = new NumberField("Air pressure", "Air pressure");
    m_precipAmount = new NumberField("Amount", "Precipitation amount");

    m_precipType = new ChoiceGroup("Type", "Precipitation type", precipChoices(),
                                   ChoiceGroup::Shape::Rows, 4);
    m_typeError = new FieldError;
    m_precipType->addToHeader(m_typeError);
    m_cloud = new ChoiceGroup("Cloud cover", "Cloud cover", cloudChoices(), ChoiceGroup::Shape::Rows, 5);
    m_windDir = new ChoiceGroup("Going to", "Wind direction, going to", compass(), ChoiceGroup::Shape::Compass);
    m_gustDir = new ChoiceGroup("Going to", "Gust direction, going to", compass(), ChoiceGroup::Shape::Compass);
    m_groups = {m_precipType, m_windDir, m_cloud, m_gustDir};
    for (ChoiceGroup *g : {m_windDir, m_gustDir})
        for (ChoiceCell *c : g->cells())
            c->setToolTip(QString("The wind blows toward the %1.").arg(sideName(c->choice().value)));

    // --- The six blocks, in reading order (plan 2.4, the Editor board) ---
    auto *temperature = new QWidget;
    group(temperature, "Temperature")->addLayout(pair(m_high, m_low));

    auto *precipitation = new QWidget;
    {
        auto *v = group(precipitation, "Precipitation");
        v->addWidget(m_precipType);
        v->addSpacing(kBetweenItems - Design::kGapHeader);
        v->addLayout(pair(m_precipAmount, nullptr));
    }

    auto *wind = new QWidget;
    {
        auto *v = group(wind, "Wind");
        v->addLayout(pair(m_wind, nullptr));
        v->addSpacing(kBetweenItems - Design::kGapHeader);
        v->addWidget(m_windDir);
    }

    auto *sky = new QWidget;
    group(sky, "Sky")->addWidget(m_cloud);
    auto *air = new QWidget;
    group(air, "Air")->addLayout(pair(m_humidity, m_pressure));

    auto *gusts = new QWidget;
    {
        auto *v = group(gusts, "Gusts");
        v->addLayout(pair(m_gust, nullptr));
        v->addSpacing(kBetweenItems - Design::kGapHeader);
        v->addWidget(m_gustDir);
    }

    auto *note = new QWidget;
    auto *events = new QWidget;
    {
        auto *nv = group(note, "Note");
        auto *hint = new QLabel("Anything you want to remember about this day");   // FM11
        hint->setStyleSheet(textStyleSheet(Design::TextStyle::Caption));
        hint->setForegroundRole(QPalette::PlaceholderText);
        hint->setWordWrap(true);
        nv->addWidget(hint);
        m_note = new QPlainTextEdit;
        m_note->setAccessibleName("Note");
        m_note->setTabChangesFocus(true);                   // Tab moves on, as in every field
        // Room for 4 lines before it scrolls; about 60 characters wide (T4).
        m_note->setFixedHeight(4 * Design::type(Design::TextStyle::Body).lineHeight
                               + 2 * Design::kGapButtons + 4);
        m_note->setMaximumWidth(Design::kParagraphMaxWidth + 2 * Design::kGapLabel);
        hint->setBuddy(m_note);
        nv->addWidget(m_note);

        auto *ev = group(events, "Events");
        m_eventsBox = new QWidget;
        m_eventsGrid = new QGridLayout(m_eventsBox);
        m_eventsGrid->setContentsMargins(0, 0, 0, 0);
        m_eventsGrid->setHorizontalSpacing(Design::kGapButtons);
        m_eventsGrid->setVerticalSpacing(Design::kTargetGap);
        for (const EventType &e : eventTypes()) {
            auto *check = new QCheckBox(e.label, m_eventsBox);   // in the block from the start (Tab order)
            check->setMinimumHeight(Design::kTarget);
            check->installEventFilter(this);                     // arrows inside, one Tab stop
            check->setProperty("wtOwnArrows", true);
            m_eventBoxes.append(check);
            m_eventKeys.append(e.key);
        }
        ev->addWidget(m_eventsBox);
        updateEventStop();
    }
    // The blocks always stay children of the body, so moving them between arrangements
    // never changes their own order; arrange() sets the Tab order for each one.
    m_blocks = {temperature, precipitation, wind, sky, air, gusts, note, events};

    auto *bodyHolder = new QWidget;
    m_bodyHolder = bodyHolder;
    for (QWidget *b : m_blocks)
        b->setParent(bodyHolder);
    m_body = new QGridLayout(bodyHolder);
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setHorizontalSpacing(kColumnGap);
    m_body->setVerticalSpacing(kBetweenGroups);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kTitleToForm);
    layout->addLayout(titleBox);
    layout->addWidget(bodyHolder);
    layout->addStretch(1);

    // --- Save and Cancel, pinned at the bottom (MainWindow places the footer) ---
    m_footer = new EditorFooter;
    m_saveButton = new QPushButton("Save");
    m_saveButton->setProperty("wtKind", "primary");          // B2: the only primary on screen
    m_saveButton->setIcon(Icons::icon("save"));
    m_saveButton->setToolTip("Save this day's weather (Ctrl+S)");
    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setIcon(Icons::icon("dismiss"));
    m_cancelButton->setToolTip("Leave without saving. If you changed anything, we ask first.");
    auto *row = static_cast<QHBoxLayout *>(m_footer->layout());
    row->addWidget(m_saveButton);                            // DLG3: action left, safe right
    row->addWidget(m_cancelButton);

    connect(m_cancelButton, &QPushButton::clicked, this, [this] {
        if (confirmDiscard())
            emit finished(false);
    });
    connect(m_saveButton, &QPushButton::clicked, this, &EditorView::save);
    connect(m_precipType, &ChoiceGroup::changed, this, [this] {
        m_typeError->setMessage(QString());
        updatePrecipState();
    });
    connect(m_precipAmount, &NumberField::edited, this, [this] { m_typeError->setMessage(QString()); });

    arrange();
}

QWidget *EditorView::firstField() const { return m_high->edit(); }
// The events are one Tab stop, like the choice groups (KB2; Chris, 2026-11-13: "navigate
// the check boxes in events like the compasses"). The arrows move in the grid, Space or
// Enter ticks, and Tab comes back in at the first box.
void EditorView::updateEventStop()
{
    for (int i = 0; i < m_eventBoxes.size(); ++i)
        m_eventBoxes[i]->setFocusPolicy(i == m_eventStop ? Qt::StrongFocus : Qt::ClickFocus);
}

bool EditorView::eventFilter(QObject *watched, QEvent *event)
{
    auto *box = qobject_cast<QCheckBox *>(watched);
    const int at = box ? int(m_eventBoxes.indexOf(box)) : -1;
    if (at < 0)
        return QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::FocusIn && m_eventStop != at) {
        m_eventStop = at;
        updateEventStop();
    } else if (event->type() == QEvent::FocusOut && m_eventStop != 0
               && !m_eventBoxes.contains(qobject_cast<QCheckBox *>(QApplication::focusWidget()))) {
        m_eventStop = 0;
        updateEventStop();
    }
    const auto keys = {int(Qt::Key_Left), int(Qt::Key_Right), int(Qt::Key_Up), int(Qt::Key_Down),
                       int(Qt::Key_Home), int(Qt::Key_End)};
    if (KeyboardNav::claim(event, keys))
        return true;
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->modifiers() & ~Qt::KeypadModifier)
        return QWidget::eventFilter(watched, event);
    const int n = int(m_eventBoxes.size());
    const int cols = std::max(1, m_eventColumns);
    int to = at;
    switch (key->key()) {
    case Qt::Key_Left:  to = std::max(0, at - 1); break;
    case Qt::Key_Right: to = std::min(n - 1, at + 1); break;
    case Qt::Key_Up:    to = at - cols >= 0 ? at - cols : at; break;
    case Qt::Key_Down:  to = at + cols < n ? at + cols : at; break;
    case Qt::Key_Home:  to = 0; break;
    case Qt::Key_End:   to = n - 1; break;
    default:            return QWidget::eventFilter(watched, event);
    }
    m_eventStop = to;
    updateEventStop();
    m_eventBoxes[to]->setFocus(Qt::TabFocusReason);        // keeps the focus tint showing (F1)
    return true;
}

QWidget *EditorView::lastField() const { return m_lastField ? m_lastField : m_eventBoxes.last(); }

void EditorView::setSaveEnabled(bool on)
{
    m_saveButton->setEnabled(on);
}

void EditorView::requestSave()
{
    if (m_saveButton->isEnabled() && isVisible())
        save();
}

void EditorView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    arrange();
}

namespace {

// Empties a layout of its items and nested layouts; the widgets themselves stay where they are.
void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QLayout *inner = item->layout()) {
            clearLayout(inner);
            delete inner;                                   // the item is the layout itself
        } else {
            delete item;
        }
    }
}

QBoxLayout *stack(QBoxLayout::Direction d, std::initializer_list<QWidget *> widgets)
{
    auto *l = new QBoxLayout(d);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(kBetweenGroups);
    for (QWidget *w : widgets)
        l->addWidget(w, d == QBoxLayout::LeftToRight ? 1 : 0, Qt::AlignTop);
    if (d == QBoxLayout::TopToBottom)
        l->addStretch(1);                                   // extra height goes below, not between
    return l;
}

} // namespace

int EditorView::fitWidth()
{
    // D64/D65's arrangement: two compasses side by side on the left, the cloud row on the right.
    return 2 * ChoiceGroup::widthFor(3) + kBetweenGroups + kBetweenGroups + ChoiceGroup::widthFor(5);
}

// Three arrangements (plan 2.4, D64, D65), chosen by width:
// - Fit, when the page is at least fitWidth() wide (her screen maximized): Temperature and Air,
//   then Wind and Gusts, on the left; Precipitation and Sky on the right; Note and Events below.
//   At Normal text on 1366 x 705 all the weather shows without scrolling.
// - Two columns in other Wide windows (RZ9): Temperature, Wind and Gusts down the left;
//   Precipitation, Sky, Air, Note and Events down the right, each column stacked on its own.
// - One column otherwise.
// D43: the choice grids switch to 3 columns together when their column is too narrow for the
// widest grid. The events take 3 columns when they fit, and 2 otherwise.
void EditorView::arrange()
{
    QWidget *win = window();
    const bool wide = Responsive::twoColumns(Responsive::band(win && win != this ? win->width() : width()));
    const Arrangement a = wide && width() >= fitWidth() ? Arrangement::Fit
                        : wide ? Arrangement::Two : Arrangement::One;
    QWidget *temperature = m_blocks[0], *precipitation = m_blocks[1], *wind = m_blocks[2],
            *sky = m_blocks[3], *air = m_blocks[4], *gusts = m_blocks[5], *note = m_blocks[6],
            *events = m_blocks[7];
    bool changed = false;
    if (!m_arranged || a != m_arrangement) {
        changed = true;
        m_arranged = true;
        m_arrangement = a;
        clearLayout(m_body);
        for (int c = 0; c < 2; ++c)
            m_body->setColumnStretch(c, 0);
        QList<QWidget *> order;                             // reading order, for Tab (KB1)
        switch (a) {
        case Arrangement::Fit: {
            m_body->setHorizontalSpacing(kBetweenGroups);
            // Nested layouts go in without an alignment: QGridLayout gives an aligned item only
            // its size hint, not its height for the width, so a wrapped error would squeeze
            // the groups under it.
            auto *left = new QVBoxLayout;
            left->setContentsMargins(0, 0, 0, 0);
            left->setSpacing(kBetweenGroups);
            left->addLayout(stack(QBoxLayout::LeftToRight, {temperature, air}));
            left->addLayout(stack(QBoxLayout::LeftToRight, {wind, gusts}));
            left->addStretch(1);
            m_body->addLayout(left, 0, 0);
            m_body->addLayout(stack(QBoxLayout::TopToBottom, {precipitation, sky}), 0, 1);
            m_body->addWidget(note, 1, 0, Qt::AlignTop);
            m_body->addWidget(events, 1, 1, Qt::AlignTop);
            m_body->setColumnStretch(0, 1);
            m_body->setColumnStretch(1, 1);
            order = {temperature, air, wind, gusts, precipitation, sky, note, events};
            break;
        }
        case Arrangement::Two:
            m_body->setHorizontalSpacing(kColumnGap);
            // Each column stacks on its own, so a tall group on one side never leaves a gap
            // under its neighbour (Chris, 2026-11-13: "a huge gap between the temperature and
            // the wind").
            m_body->addLayout(stack(QBoxLayout::TopToBottom, {temperature, wind, gusts}), 0, 0);
            m_body->addLayout(stack(QBoxLayout::TopToBottom, {precipitation, sky, air, note, events}), 0, 1);
            m_body->setColumnStretch(0, 1);
            m_body->setColumnStretch(1, 1);
            order = {temperature, wind, gusts, precipitation, sky, air, note, events};
            break;
        case Arrangement::One:
            m_body->setHorizontalSpacing(kColumnGap);
            order = {temperature, precipitation, wind, sky, air, gusts, note, events};
            for (int i = 0; i < order.size(); ++i)
                m_body->addWidget(order[i], i, 0, Qt::AlignTop);
            m_body->setColumnStretch(0, 1);
            break;
        }
        // Tab goes through the blocks in reading order, then Save and Cancel (KB1).
        // Each block's own stops are read once, in the order they were made (top to bottom
        // inside the block), before any reordering.
        if (m_blockStops.isEmpty())
            for (QWidget *b : m_blocks) {
                QList<QWidget *> stops;
                for (QWidget *w = b->nextInFocusChain(); w && w != b; w = w->nextInFocusChain())
                    if (b->isAncestorOf(w) && w->focusPolicy() != Qt::NoFocus)
                        stops << w;
                m_blockStops.insert(b, stops);
            }
        QList<QWidget *> chain;
        for (QWidget *b : order)
            chain << m_blockStops.value(b);
        for (int i = 1; i < chain.size(); ++i)
            QWidget::setTabOrder(chain[i - 1], chain[i]);
        // Save follows the last widget in the chain, whether or not Tab stops on it now
        // (the events' and the groups' Tab stops move with the arrows).
        m_lastField = chain.isEmpty() ? nullptr : chain.last();
        if (m_lastField && m_saveButton->window() == m_lastField->window())
            QWidget::setTabOrder(m_lastField, m_saveButton);
    }

    // How wide the columns the grids and events sit in are.
    int column = width();                                   // One
    int gridColumn = width();
    if (a == Arrangement::Two) {
        column = gridColumn = (width() - kColumnGap) / 2;
    } else if (a == Arrangement::Fit) {
        const int half = (width() - kBetweenGroups) / 2;
        const int left = std::max(half, 2 * ChoiceGroup::widthFor(3) + kBetweenGroups);
        column = gridColumn = width() - kBetweenGroups - left;   // precipitation, sky, events
    }
    ChoiceGroup::fitAll(m_groups, gridColumn);

    int widest = 0;
    for (QCheckBox *c : m_eventBoxes)
        widest = std::max(widest, c->sizeHint().width());
    const int cols = 3 * widest + 2 * Design::kGapButtons <= column ? 3 : 2;
    if (cols != m_eventColumns) {
        changed = true;
        m_eventColumns = cols;
        for (QCheckBox *c : m_eventBoxes)
            m_eventsGrid->removeWidget(c);
        for (int i = 0; i < m_eventBoxes.size(); ++i)
            m_eventsGrid->addWidget(m_eventBoxes[i], i / cols, i % cols);
        for (int c = 0; c < 3; ++c)
            m_eventsGrid->setColumnStretch(c, c < cols ? 1 : 0);
        // Its own minimum is two columns, so the page can always narrow to the next arrangement.
        // Its height is set outright: the grid's rows would otherwise squeeze for an event.
        m_eventsBox->setMinimumWidth(2 * widest + Design::kGapButtons);
        const int rows = (int(m_eventBoxes.size()) + cols - 1) / cols;
        m_eventsBox->setFixedHeight(rows * Design::kTarget + (rows - 1) * Design::kTargetGap);
    }
    // The form can always narrow to one column's width, whatever it shows now, so a Snap to
    // half the screen is never held back by the layout it is leaving (Z5, RZ2).
    int oneColumn = 0;
    for (QWidget *b : m_blocks)
        oneColumn = std::max(oneColumn, b->minimumSizeHint().width());
    m_bodyHolder->setMinimumWidth(oneColumn);
    // Lay out the new arrangement now, at this width, rather than one event later, so a Snap
    // never shows a frame of the old one.
    if (changed) {
        m_body->activate();
        // The page around the editor learns its new height from a posted event; ask for it
        // now, once, so the window never lays out a frame with the old height.
        updateGeometry();
        if (!m_settling) {
            m_settling = true;
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            m_settling = false;
        }
    }
    m_eventsGrid->activate();
}

void EditorView::applyUnits()
{
    const Units::System u = m_units;

    auto temp = [u](double v) { return Units::cToDisplay(v, u); };
    auto tempBack = [u](double v) { return Units::displayToC(v, u); };
    m_high->setSpec(make(Units::tempUnit(u), std::nullopt, std::nullopt, temp, tempBack));
    m_low->setSpec(make(Units::tempUnit(u), std::nullopt, std::nullopt, temp, tempBack));

    auto speed = [u](double v) { return Units::kmhToDisplay(v, u); };
    auto speedBack = [u](double v) { return Units::displayToKmh(v, u); };
    m_wind->setSpec(make(Units::windUnit(u), 0.0, std::nullopt, speed, speedBack));
    m_gust->setSpec(make(Units::windUnit(u), 0.0, std::nullopt, speed, speedBack));

    m_humidity->setSpec(make("%", 0.0, 100.0));
    m_pressure->setSpec(make(Units::pressureUnit(u), std::nullopt, std::nullopt,
        [u](double v) { return Units::hpaToDisplay(v, u); },
        [u](double v) { return Units::displayToHpa(v, u); }));

    // The amount's unit follows the type (D54); open() and save() convert it.
    m_precipAmount->setSpec(make(QString(), 0.0, std::nullopt));
}

void EditorView::open(const QDate &date)
{
    m_date = date;
    m_units = Units::fromSetting(m_repo.setting("units", "metric"));
    applyUnits();                                // must come before setValue()

    m_title->setText("Weather for " + date.toString("dddd, MMMM d, yyyy"));

    const WeatherDay d = m_repo.weatherFor(date).value_or(WeatherDay{});
    m_high->setValue(d.highC);
    m_low->setValue(d.lowC);
    m_wind->setValue(d.windKmh);
    m_gust->setValue(d.gustKmh);
    m_humidity->setValue(d.humidityPct);
    m_pressure->setValue(d.pressureHpa);
    // CF4: the saved side is where the wind comes from; the buttons say where it goes.
    m_windDir->setValue(WeatherInput::windGoingToFrom(d.windDir));
    m_gustDir->setValue(WeatherInput::windGoingToFrom(d.gustDir));
    m_cloud->setValue(d.cloud);
    m_precipType->setValue(d.precipType);
    m_loadedType = m_precipType->value();

    // Stored in mm; shown in the type's unit (D54), keeping the exact stored value.
    if (d.precipMm && d.precipType != "None")
        m_precipAmount->setShown(WeatherInput::mmToAmount(*d.precipMm, d.precipType, m_units), d.precipMm);
    else
        m_precipAmount->setShown(std::nullopt, std::nullopt);

    const QString note = m_repo.note(date);
    m_note->setPlainText(note);
    const QSet<QString> events = m_repo.events(date);
    for (int i = 0; i < m_eventBoxes.size(); ++i)
        m_eventBoxes[i]->setChecked(events.contains(m_eventKeys[i]));

    m_hadWeather = m_repo.weatherFor(date).has_value();
    m_hadNote = !note.trimmed().isEmpty();
    m_hadEvents = !events.isEmpty();

    m_typeError->setMessage(QString());
    updatePrecipState();

    m_loaded = snapshot();
    // KB5: the focus starts on the first field, once the editor is on screen.
    QTimer::singleShot(0, m_high->edit(), [this] { m_high->edit()->setFocus(Qt::OtherFocusReason); });
}

void EditorView::updatePrecipState()
{
    const QString type = m_precipType->value();
    const bool none = type == "None";
    const auto unit = WeatherInput::precipUnit(type, m_units);
    m_precipAmount->setUnit(unit.unit, unit.spoken);        // D54: no unit to choose
    m_precipAmount->setInputEnabled(!none);                 // "None": no amount to enter
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
    s.windDir = m_windDir->value();
    s.gustDir = m_gustDir->value();
    s.precipType = m_precipType->value();
    s.cloud = m_cloud->value();
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

    const QString type = m_precipType->value();
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
    m_typeError->setMessage(typeError);
    if (!ok)
        return;                                  // errors are showing; stay in the editor

    WeatherDay day;
    day.highC = m_high->value();
    day.lowC = m_low->value();
    day.windKmh = m_wind->value();
    day.gustKmh = m_gust->value();
    day.humidityPct = m_humidity->value();
    day.pressureHpa = m_pressure->value();
    day.windDir = WeatherInput::windFromGoingTo(m_windDir->value());     // CF4: saved as "from"
    day.gustDir = WeatherInput::windFromGoingTo(m_gustDir->value());
    day.cloud = m_cloud->value();
    day.precipType = type;

    // Precipitation is always stored in millimetres, whatever unit it was typed in.
    if (none) {
        day.precipMm = 0.0;
    } else if (!type.isEmpty()) {
        if (m_precipAmount->isUntouched() && WeatherInput::sameAmountUnit(type, m_loadedType)) {
            day.precipMm = m_precipAmount->original();
        } else if (const auto typed = m_precipAmount->typedValue()) {
            const double mm = WeatherInput::amountToMm(*typed, type, m_units);
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
    const QString what = erased.isEmpty() ? QString()
        : erased.size() == 1 ? erased.first()
        : erased.mid(0, erased.size() - 1).join(", ") + " and " + erased.last();
    if (!erased.isEmpty()
        && !Confirm::destructive(this, "Erase saved information?", "Saving will remove " + what + ".",
                                 "Go back", "Erase and save"))
        return;

    const Repository::DayRecord before = m_repo.dayRecord(m_date);   // for Undo
    bool saved = m_repo.saveWeather(m_date, day);
    saved = m_repo.saveNote(m_date, note) && saved;
    saved = m_repo.saveEvents(m_date, events) && saved;
    if (!saved) {
        QMessageBox::warning(this, "Weather Tracker",
                             "Sorry, this day could not be saved.\n\n"
                             "Your other data is safe. Please try again.");
        return;
    }
    if (!erased.isEmpty())
        emit erasedOnSave(m_date, what, before);
    emit finished(true);
}
