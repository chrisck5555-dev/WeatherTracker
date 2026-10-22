#include "DayView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

#include "AppState.h"
#include "Artwork.h"
#include "EventTypes.h"
#include "Format.h"
#include "Repository.h"
#include "Units.h"
#include "WeatherIcons.h"

namespace {

// A strip showing the day's kind of weather. It only paints a picture, so it needs no moc.
class BannerStrip : public QWidget
{
public:
    explicit BannerStrip(QWidget *parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        const QPixmap picture = Artwork::banner(property("bannerName").toString(), size(),
                                                devicePixelRatioF());
        if (picture.isNull())
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), 18, 18);
        p.setClipPath(clip);
        p.drawPixmap(QPointF(0, 0), picture);
    }
};

// Which picture suits the day: what fell from the sky first, otherwise the cloud cover.
QString bannerFor(const std::optional<WeatherDay> &weather)
{
    if (!weather)
        return "banner-plain";
    const QString &t = weather->precipType;
    if (t == "Snow")
        return "banner-snow";
    if (t == "Rain" || t == "Drizzle" || t == "Freezing rain" || t == "Hail"
        || t == "Sleet" || t == "Mixed")
        return "banner-rain";
    const QString &c = weather->cloud;
    if (c == "Clear" || c == "Mostly clear")
        return "banner-sunny";
    if (c == "Partly cloudy" || c == "Mostly cloudy" || c == "Overcast")
        return "banner-cloudy";
    return "banner-plain";
}

// Three columns: label, a fixed-width picture column, value. Every row has the picture
// column (empty when there is no picture) so all the values start at the same place.
// `iconHtml` and `valueHtml` must already be escaped.
QString rowHtml(const QString &label, const QString &iconHtml, const QString &valueHtml)
{
    return QString("<tr><td width='25%' style='padding:4px 12px 4px 0'>%1</td>"
                   "<td width='44' align='center' style='padding:2px 0'>%2</td>"
                   "<td style='padding:4px 0'><b>%3</b></td></tr>")
        .arg(label.toHtmlEscaped(), iconHtml, valueHtml);
}

QString row(const QString &label, const QString &value)
{
    return rowHtml(label, QString(), value.toHtmlEscaped());
}

} // namespace

DayView::DayView(AppState &state, Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_repo(repo)
{
    m_editButton = new QPushButton("Add / edit this day");
    m_editButton->setMinimumHeight(52);

    m_text = new QTextBrowser;
    m_text->setFrameShape(QFrame::NoFrame);
    QFont f = font();
    f.setPointSize(14);
    m_text->setFont(f);

    // Shown instead of the text when the day is empty: message, then the button under it.
    m_empty = new QWidget;
    auto *emptyText = new QLabel("Nothing recorded for this day yet.");
    QFont ef = font();
    ef.setPointSize(14);
    emptyText->setFont(ef);
    emptyText->setWordWrap(true);
    auto *emptyAdd = new QPushButton("Add this day's weather");
    emptyAdd->setMinimumHeight(52);
    m_emptyPicture = new QLabel;
    auto *emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->addWidget(m_emptyPicture, 0, Qt::AlignLeft);
    emptyLayout->addWidget(emptyText);
    emptyLayout->addWidget(emptyAdd, 0, Qt::AlignLeft);
    emptyLayout->addStretch(1);
    m_empty->hide();

    // The weather picture is the strip's background and the button sits on top of it.
    m_banner = new BannerStrip;
    auto *top = new QHBoxLayout(m_banner);
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(m_editButton);
    top->addStretch(1);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_banner);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_empty, 1);

    const auto requestEdit = [this] { emit editRequested(m_state.currentDate()); };
    connect(m_editButton, &QPushButton::clicked, this, requestEdit);
    connect(emptyAdd, &QPushButton::clicked, this, requestEdit);
    connect(&m_state, &AppState::currentDateChanged, this, [this] {
        if (isVisible())                 // hidden views refresh when shown instead
            refresh();
    });
}

void DayView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
}

void DayView::refresh()
{
    const QDate date = m_state.currentDate();
    const auto weather = m_repo.weatherFor(date);
    const QString note = m_repo.note(date);
    const QSet<QString> events = m_repo.events(date);
    const auto units = Units::fromSetting(m_repo.setting("units", "metric"));

    // An empty day shows the message and one button under it (the top button would repeat it).
    const bool empty = !weather && note.trimmed().isEmpty() && events.isEmpty();
    m_empty->setVisible(empty);
    m_text->setVisible(!empty);
    m_banner->setVisible(!empty);                  // the strip holds the top button

    // Clay looks: the strip is taller and shows a picture matching the day's weather behind
    // the button. Other looks (or a missing picture): just the button, no extra height.
    const QString bannerName = bannerFor(weather);
    const bool art = Artwork::bannerAvailable(bannerName);
    m_banner->setProperty("bannerName", art ? bannerName : QString());
    m_banner->setMinimumHeight(art ? 110 : 0);
    m_banner->layout()->setContentsMargins(art ? 24 : 0, 0, 0, 0);
    m_banner->update();

    if (empty) {
        const QPixmap picture = Artwork::emptyDay(112, devicePixelRatioF());
        m_emptyPicture->setPixmap(picture);
        m_emptyPicture->setVisible(!picture.isNull());
        return;
    }

    // A picture is handed to the text box as a ready-sized image, so it stays sharp.
    const qreal dpr = devicePixelRatioF();
    auto picture = [&](const QString &name, int size) -> QString {
        if (name.isEmpty())
            return QString();
        const QPixmap pm = WeatherIcons::pixmap(name, size, dpr);
        if (pm.isNull())
            return QString();                     // picture missing: show no broken image
        const QUrl url(QString("wt-icon:%1/%2").arg(name).arg(size));
        m_text->document()->addResource(QTextDocument::ImageResource, url, pm);
        return QString("<img src='%1' width='%2' height='%2'>").arg(url.toString()).arg(size);
    };

    QString html = "<h3>Weather</h3>";
    if (weather) {
        QString table = "<table width='100%'>";
        table += row("High temperature", Format::temperature(weather->highC, units));
        table += row("Low temperature",  Format::temperature(weather->lowC, units));
        table += row("Wind speed",       Format::windSpeed(weather->windKmh, units));
        table += rowHtml("Wind direction", picture(WeatherIcons::windName(weather->windDir), 28),
                         Format::text(weather->windDir).toHtmlEscaped());
        table += row("Gust speed",       Format::windSpeed(weather->gustKmh, units));
        table += rowHtml("Gust direction", picture(WeatherIcons::windName(weather->gustDir), 28),
                         Format::text(weather->gustDir).toHtmlEscaped());
        table += rowHtml("Precipitation", picture(WeatherIcons::precipName(weather->precipType), 28),
                         Format::precipitation(weather->precipType, weather->precipMm, units).toHtmlEscaped());
        table += row("Humidity",         Format::humidity(weather->humidityPct));
        table += row("Pressure",         Format::pressure(weather->pressureHpa, units));
        table += rowHtml("Cloud cover", picture(WeatherIcons::cloudName(weather->cloud), 28),
                         Format::text(weather->cloud).toHtmlEscaped());
        table += "</table>";

        // One large picture of the day's conditions beside the numbers.
        const QString big = picture(WeatherIcons::conditionName(*weather), 72);
        if (big.isEmpty())
            html += table;
        else
            html += "<table width='100%'><tr><td width='110' valign='middle'>" + big
                  + "</td><td>" + table + "</td></tr></table>";
    } else {
        html += "<p>" + Format::notRecorded() + "</p>";
    }

    html += "<h3>Note</h3>";
    if (note.trimmed().isEmpty())
        html += "<p>" + Format::notRecorded() + "</p>";
    else
        html += "<p>" + note.toHtmlEscaped().replace('\n', QStringLiteral("<br>")) + "</p>";

    html += "<h3>Events</h3>";
    QStringList labels;
    for (const EventType &e : eventTypes())          // fixed order, not set order
        if (events.contains(e.key))
            labels << e.label;
    html += "<p>" + (labels.isEmpty() ? Format::notRecorded() : labels.join(", ").toHtmlEscaped()) + "</p>";

    m_text->setHtml(html);
    m_text->verticalScrollBar()->setValue(0);        // always start at the top
}