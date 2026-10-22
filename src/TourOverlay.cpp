#include "TourOverlay.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegion>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "ClayPaint.h"
#include "PanelHost.h"
#include "ThemeManager.h"

namespace {

constexpr int kCardWidth = 380;
constexpr int kGap = 16;           // between the card and the window edge
constexpr int kRing = 6;           // how far the lit area reaches beyond a widget
constexpr int kHoleRadius = 10;    // rounded corners of the lit area

// The tour card: a flat card with an accent outline so it stands out.
class TourCard : public QWidget
{
public:
    explicit TourCard(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_NoMousePropagation);
    }

    std::function<void()> onLayout;               // called when the contents have been laid out

protected:
    bool event(QEvent *e) override
    {
        const bool handled = QWidget::event(e);   // the layout is up to date after this
        if ((e->type() == QEvent::LayoutRequest || e->type() == QEvent::Show) && onLayout)
            onLayout();
        return handled;
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
        ClayPaint::drawCard(p, r, 18, palette().color(QPalette::Base));
        p.setPen(QPen(ThemeManager::instance()->colors().accent, 3));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 18, 18);
    }
};

// A rectangle with rounded corners, as a region (the overlay's mask).
QRegion roundedRegion(const QRect &r, int radius)
{
    radius = qMin(radius, qMin(r.width(), r.height()) / 2);
    const int d = 2 * radius;
    QRegion region(r.adjusted(radius, 0, -radius, 0));
    region += QRegion(r.adjusted(0, radius, 0, -radius));
    region += QRegion(QRect(r.left(), r.top(), d, d), QRegion::Ellipse);
    region += QRegion(QRect(r.right() - d + 1, r.top(), d, d), QRegion::Ellipse);
    region += QRegion(QRect(r.left(), r.bottom() - d + 1, d, d), QRegion::Ellipse);
    region += QRegion(QRect(r.right() - d + 1, r.bottom() - d + 1, d, d), QRegion::Ellipse);
    return region;
}

int overlap(const QRect &a, const QRect &b)
{
    const QRect i = a.intersected(b);
    return i.isEmpty() ? 0 : i.width() * i.height();
}

} // namespace

TourOverlay::TourOverlay(QWidget *root, PanelHost *host, Hooks hooks)
    : QWidget(root), m_root(root), m_host(host), m_hooks(std::move(hooks))
{
    setFocusPolicy(Qt::NoFocus);
    setGeometry(root->rect());
    hide();

    m_steps = {
        {"Welcome to Weather Tracker",
         "This program is a diary for the weather. You write down each day's weather, and it "
         "shows you calendars, averages, charts and records.\n\n"
         "In this tour you try each main part yourself. Everything that is not part of the "
         "step is greyed out. You can leave at any time with “Skip the tour”, and see it "
         "again from Settings.",
         {}},
        {"Writing down the weather",
         "Fill in what you know and leave the rest empty. Nothing is saved during the tour.",
         {{"Press the blue button next to the tabs (“Add today's weather”)", "editorOpened"},
          {"Type in any box", "editorEdited"},
          {"Press “Cancel”", "editorCancelled"}}},
        {"Looking at your days",
         "The Calendar shows a day, a week, a month or a year.",
         {{"Press “Week”", "week"},
          {"Press “Next”", "next"},
          {"Click a day to open it", "dayOpened"}}},
        {"More ways to look",
         "Analytics shows averages and totals. Charts draws graphs. "
         "Records shows the hottest and coldest days, streaks and more.",
         {{"Open the Analytics tab", "tab1"},
          {"Open the Charts tab", "tab2"},
          {"Open the Records tab", "tab3"}}},
        {"Finding things",
         "Search finds days by what you wrote in their notes. Go to date jumps to any date. "
         "To close either one, press Esc, press its button, or click the greyed area.",
         {{"Press “Search”", "searchOpened"},
          {"Type a word", "searchTyped"},
          {"Close it", "searchClosed"},
          {"Press “Go to date”", "gotoOpened"},
          {"Close it", "gotoClosed"}}},
        {"Settings",
         "Settings changes the units, the text size and the colours, and keeps your backups.",
         {{"Press “Settings”", "settingsOpened"},
          {"Press “Done”", "settingsDone"}}},
        {"All done",
         "The program makes a backup of your data every day. Press F1 at any time for the "
         "user guide, or open it from Settings.\n\nPress Finish to start using Weather Tracker.",
         {}},
    };

    buildCard();

    m_timer = new QTimer(this);
    m_timer->setInterval(250);
    connect(m_timer, &QTimer::timeout, this, &TourOverlay::tick);
}

TourOverlay::~TourOverlay()
{
    qApp->removeEventFilter(this);
    delete m_card.data();
}

void TourOverlay::buildCard()
{
    auto *card = new TourCard(m_root);
    card->onLayout = [this] { placeCard(); };    // measure again whenever the contents change
    m_card = card;
    m_card->hide();

    const auto colors = ThemeManager::instance()->colors();
    QFont base = m_card->font();

    m_counter = new QLabel;
    QFont small = base;
    small.setPointSize(11);
    m_counter->setFont(small);
    m_counter->setStyleSheet(QString("color: %1;").arg(colors.mutedText.name()));

    m_title = new QLabel;
    QFont big = base;
    big.setPointSize(16);
    big.setBold(true);
    m_title->setFont(big);
    m_title->setWordWrap(true);

    QFont normal = base;
    normal.setPointSize(12);
    m_text = new QLabel;
    m_text->setFont(normal);
    m_text->setWordWrap(true);
    m_text->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_checks = new QVBoxLayout;
    m_checks->setSpacing(4);

    m_hint = new QLabel("Do everything in the list. “Next” turns on when all of it is ticked.");
    m_hint->setFont(small);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QString("color: %1;").arg(colors.mutedText.name()));

    m_skip = new QPushButton("Skip the tour");
    m_back = new QPushButton("Back");
    m_next = new QPushButton("Next");
    for (auto *b : {m_skip, m_back, m_next}) {
        b->setMinimumHeight(44);
        b->setAutoDefault(false);
    }
    m_back->setMinimumWidth(80);
    m_next->setMinimumWidth(100);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_skip);
    buttons->addStretch(1);
    buttons->addWidget(m_back);
    buttons->addWidget(m_next);

    auto *layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(8);
    layout->addWidget(m_counter);
    layout->addWidget(m_title);
    layout->addWidget(m_text);
    layout->addLayout(m_checks);
    layout->addWidget(m_hint);
    layout->addSpacing(4);
    layout->addLayout(buttons);

    connect(m_skip, &QPushButton::clicked, this, &TourOverlay::finish);
    connect(m_back, &QPushButton::clicked, this, [this] {
        if (m_step > 0) showStep(m_step - 1);
    });
    connect(m_next, &QPushButton::clicked, this, [this] {
        if (m_step + 1 < m_steps.size()) showStep(m_step + 1);
        else finish();                                    // the last step's button is "Finish"
    });
}

void TourOverlay::start()
{
    m_root->installEventFilter(this);                     // follow the window's size
    qApp->installEventFilter(this);                       // block keys outside the step
    show();
    m_timer->start();
    showStep(0);
}

void TourOverlay::showStep(int index)
{
    m_step = -1;                                          // reports are ignored during the reset
    if (m_hooks.resetScreen)
        m_hooks.resetScreen();                            // a known screen for every step
    m_events.clear();
    m_step = index;

    const Step &s = m_steps[index];
    m_counter->setText(QString("Step %1 of %2").arg(index + 1).arg(m_steps.size()));
    m_title->setText(s.title);
    m_text->setText(s.text);
    m_back->setEnabled(index > 0);
    m_next->setText(index + 1 == m_steps.size() ? "Finish" : "Next");

    while (QLayoutItem *item = m_checks->takeAt(0)) {     // rebuild the checklist
        delete item->widget();
        delete item;
    }
    m_checkLabels.clear();
    QFont f = m_card->font();
    f.setPointSize(12);
    for (int i = 0; i < s.items.size(); ++i) {
        auto *label = new QLabel;
        label->setFont(f);
        label->setWordWrap(true);
        m_checks->addWidget(label);
        m_checkLabels.append(label);
    }

    raise();                                              // above the window, below any panel
    updateChecklist();
    refreshTargets(true);
    m_card->raise();
    m_next->setFocus();
    QTimer::singleShot(0, this, [this] { placeCard(); });  // once the new labels have a size
}

void TourOverlay::updateChecklist()
{
    const Step &s = m_steps[m_step];
    const auto colors = ThemeManager::instance()->colors();
    bool all = true;
    for (int i = 0; i < s.items.size(); ++i) {
        const bool done = m_events.contains(s.items[i].event);
        all = all && done;
        m_checkLabels[i]->setText((done ? QString("☑  ") : QString("☐  ")) + s.items[i].text);
        m_checkLabels[i]->setStyleSheet(done ? QString("color: %1; font-weight: bold;").arg(colors.accent.name())
                                             : QString());
    }
    m_next->setEnabled(all);
    m_hint->setVisible(!s.items.isEmpty() && !all);
    placeCard();                                          // its height may have changed
}

void TourOverlay::report(const QString &event)
{
    if (m_step < 0 || m_finished)
        return;
    if (m_events.contains(event))
        return;
    m_events.insert(event);
    updateChecklist();
    refreshTargets(false);
}

void TourOverlay::tick()
{
    if (m_finished)
        return;
    if (m_step == 1 && m_hooks.editorDirty && m_hooks.editorDirty() && !m_events.contains("editorEdited")) {
        m_events.insert("editorEdited");
        updateChecklist();
    }
    refreshTargets(false);
}

void TourOverlay::cardToFront()
{
    if (m_card)
        m_card->raise();
}

void TourOverlay::refreshTargets(bool force)
{
    m_targets = m_hooks.targets ? m_hooks.targets(m_step) : QList<TourTarget>();
    QList<QRect> holes, important;
    for (const TourTarget &t : m_targets) {
        if (!t.widget || !t.widget->isVisible() || t.widget->size().isEmpty())
            continue;
        QRect r(t.widget->mapTo(m_root, QPoint(0, 0)), t.widget->size());
        r = r.adjusted(-kRing, -kRing, kRing, kRing).intersected(rect());
        holes << r;
        if (t.important)
            important << r;
    }
    if (!force && holes == m_holes && important == m_important)
        return;
    m_holes = holes;
    m_important = important;

    // The overlay only exists outside the holes, so clicks inside a hole reach the widget.
    if (holes.isEmpty()) {
        clearMask();
    } else {
        QRegion region(rect());
        for (const QRect &h : holes)
            region -= roundedRegion(h, kHoleRadius);
        setMask(region);
    }
    placeCard();
    update();
}

void TourOverlay::placeCard()
{
    if (!m_card || m_step < 0)
        return;
    const int w = kCardWidth;
    if (m_placing)
        return;
    m_placing = true;                                     // resizing triggers layout events
    if (m_card->width() != w)
        m_card->setFixedWidth(w);
    int h = m_card->layout()->totalHeightForWidth(w);
    if (h < 0)
        h = m_card->sizeHint().height();
    h = qMax(h, m_card->layout()->sizeHint().height());
    if (m_card->height() != h)
        m_card->resize(w, h);

    const int W = m_root->width();
    const int H = m_root->height();
    QPoint best((W - w) / 2, (H - h) / 2);                // centred when nothing is lit
    if (!m_holes.isEmpty()) {
        const int right = W - w - kGap, left = kGap;
        const int top = kGap, bottom = H - h - kGap, mid = (H - h) / 2;
        const int centre = (W - w) / 2;
        const QPoint candidates[] = {{right, bottom}, {left, bottom}, {centre, bottom},
                                     {right, top}, {left, top}, {centre, top},
                                     {right, mid}, {left, mid}, {centre, mid}};
        long bestScore = -1;
        for (const QPoint &c : candidates) {
            const QRect r(c, QSize(w, h));
            long score = 0;
            for (const QRect &hole : m_holes)
                score += overlap(r, hole);
            for (const QRect &imp : m_important)
                score += 1000L * overlap(r, imp);
            if (bestScore < 0 || score < bestScore) {
                bestScore = score;
                best = c;
            }
        }
    }
    m_card->move(best);
    m_card->show();
    m_card->raise();
    m_host->setAvoid(m_card->geometry());                 // a panel opens beside the card
    m_placing = false;
}

bool TourOverlay::keyAllowed(QObject *receiver) const
{
    const auto *w = qobject_cast<const QWidget *>(receiver);
    if (!w)
        return true;
    if (w->window() != window())
        return true;                                      // dialogs and dropdown lists
    if (m_card && (w == m_card || m_card->isAncestorOf(w)))
        return true;
    if (m_host->isOpen() && m_host->isAncestorOf(w))
        return true;
    for (const TourTarget &t : m_targets)
        if (t.widget && (t.widget == w || t.widget->isAncestorOf(w)))
            return true;
    return false;
}

bool TourOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_root && event->type() == QEvent::Resize) {
        setGeometry(m_root->rect());
        refreshTargets(true);
    } else if (!m_finished
               && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
               && !keyAllowed(watched)) {
        return true;                                      // keys only work where the step allows
    }
    return QWidget::eventFilter(watched, event);
}

void TourOverlay::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    m_timer->stop();
    qApp->removeEventFilter(this);
    m_root->removeEventFilter(this);
    m_host->setAvoid(QRect());
    if (m_card)
        m_card->hide();
    hide();
    emit finished();
}

void TourOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0, 0, 0, 150));
    p.setPen(QPen(ThemeManager::instance()->colors().accent, 4));
    p.setBrush(Qt::NoBrush);
    for (const QRect &h : m_holes)
        p.drawRoundedRect(QRectF(h).adjusted(-2, -2, 2, 2), kHoleRadius + 2, kHoleRadius + 2);
}

void TourOverlay::mousePressEvent(QMouseEvent *event) { event->accept(); }
void TourOverlay::mouseReleaseEvent(QMouseEvent *event) { event->accept(); }
void TourOverlay::wheelEvent(QWheelEvent *event) { event->accept(); }
