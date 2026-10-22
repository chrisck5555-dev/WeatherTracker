#include "NumberField.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "Format.h"
#include "WeatherInput.h"
#include "ThemeManager.h"

NumberField::NumberField(const NumberSpec &spec, QWidget *parent)
    : QWidget(parent)
    , m_spec(spec)
{
    m_edit = new QLineEdit;
    m_edit->setMinimumHeight(44);
    m_edit->setMaximumWidth(140);

    m_down = new QPushButton("\u25BC");
    m_up = new QPushButton("\u25B2");
    for (auto *b : {m_down, m_up}) {
        b->setMinimumSize(44, 44);
        b->setAutoRepeat(true);                  // hold to keep stepping
    }

    m_unitLabel = new QLabel(spec.unit);

    m_error = new QLabel;
    const auto err = ThemeManager::instance()->colors().error;
    m_error->setStyleSheet(QString("color: %1; font-weight: bold;").arg(err.name()));
    m_error->setWordWrap(true);
    m_error->hide();

    auto *row = new QHBoxLayout;
    row->addWidget(m_edit);
    row->addWidget(m_down);
    row->addWidget(m_up);
    row->addWidget(m_unitLabel);
    row->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(row);
    layout->addWidget(m_error);

    connect(m_down, &QPushButton::clicked, this, [this] { stepBy(-1); });
    connect(m_up, &QPushButton::clicked, this, [this] { stepBy(+1); });
    connect(m_edit, &QLineEdit::textChanged, this, [this] { setError(QString()); });
}

void NumberField::setSpec(const NumberSpec &spec)
{
    m_spec = spec;
    m_unitLabel->setText(spec.unit);
}

void NumberField::setValue(const std::optional<double> &original)
{
    m_original = original;
    if (original) {
        const double shown = m_spec.toDisplay ? m_spec.toDisplay(*original) : *original;
        m_shownText = Format::number(shown, 3);
    } else {
        m_shownText.clear();
    }
    m_edit->setText(m_shownText);
    setError(QString());
}

// Show `shown` in the box, but remember `original` (the exact stored value).
void NumberField::setShown(const std::optional<double> &shown,
                           const std::optional<double> &original)
{
    m_original = original;
    m_shownText = shown ? Format::number(*shown, 3) : QString();
    m_edit->setText(m_shownText);
    setError(QString());
}

bool NumberField::isUntouched() const { return m_edit->text() == m_shownText; }
bool NumberField::isBlank() const { return m_edit->text().trimmed().isEmpty(); }

std::optional<double> NumberField::typedValue() const
{
    double v = 0;
    if (WeatherInput::parseNumber(m_edit->text(), v) == WeatherInput::Parse::Ok)
        return v;
    return std::nullopt;
}

std::optional<double> NumberField::storedTypedValue() const
{
    const auto typed = typedValue();
    if (!typed)
        return std::nullopt;
    return m_spec.fromDisplay ? m_spec.fromDisplay(*typed) : *typed;
}

std::optional<double> NumberField::value() const
{
    return isUntouched() ? m_original : storedTypedValue();   // untouched: NEVER convert
}

QString NumberField::text() const { return m_edit->text(); }

bool NumberField::validate()
{
    if (isUntouched()) {                         // a stored value is trusted as it is
        setError(QString());
        return true;
    }
    std::optional<double> v;
    const QString message =
        WeatherInput::validateNumber(m_edit->text(), m_spec.min, m_spec.max, v);
    setError(message);
    return message.isEmpty();
}

void NumberField::setError(const QString &message)
{
    m_error->setText(message.isEmpty() ? QString() : "\u26A0 " + message);
    m_error->setVisible(!message.isEmpty());
}

void NumberField::setInputEnabled(bool on)
{
    m_edit->setEnabled(on);
    m_down->setEnabled(on);
    m_up->setEnabled(on);
}

void NumberField::stepBy(int direction)
{
    double current = 0;
    double next = m_spec.start;                  // blank or unreadable: begin at the start value
    if (WeatherInput::parseNumber(m_edit->text(), current) == WeatherInput::Parse::Ok)
        next = current + direction * m_spec.step;
    next = std::round(next * 1e6) / 1e6;         // 0.1 + 0.2 must not show as 0.30000000000000004
    if (m_spec.min) next = std::max(next, *m_spec.min);
    if (m_spec.max) next = std::min(next, *m_spec.max);
    m_edit->setText(Format::number(next, 3));
}

