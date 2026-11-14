#include "NumberField.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "Design.h"
#include "FieldError.h"
#include "Format.h"
#include "ThemeManager.h"
#include "WeatherInput.h"

NumberField::NumberField(const QString &label, const QString &spokenLabel, QWidget *parent)
    : QWidget(parent)
    , m_spokenLabel(spokenLabel)
{
    m_label = new QLabel(label);
    m_edit = new QLineEdit;
    m_edit->setMinimumHeight(Design::kTarget);
    m_edit->setFixedWidth(boxWidth(font()));
    m_label->setBuddy(m_edit);
    m_unitLabel = new QLabel;
    m_error = new FieldError;

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(Design::kGapButtons);       // FM8: the unit just after the box
    row->addWidget(m_edit);
    row->addWidget(m_unitLabel);
    row->addStretch(1);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(Design::kGapHeader);     // S2: 8 between a control and its header
    column->addWidget(m_label);
    column->addLayout(row);
    column->addWidget(m_error);

    // FM4: an error goes as soon as she changes the text, and the box is checked again when
    // she leaves it.
    connect(m_edit, &QLineEdit::textEdited, this, [this] {
        setError(QString());
        emit edited();
    });
    connect(m_edit, &QLineEdit::editingFinished, this, [this] {
        if (isEnabled() && m_edit->isEnabled())
            validate();
    });
    if (auto *t = ThemeManager::instance())
        connect(t, &ThemeManager::themeChanged, this, &NumberField::restyle);
}

int NumberField::boxWidth(const QFont &font)
{
    // Five digits, the padding on both sides (12 including the outline, S1) and room for
    // the cursor, on the 4 px grid.
    const int w = QFontMetrics(font).horizontalAdvance(QStringLiteral("00000"))
                + 2 * Design::kGapLabel + Design::kGapButtons;
    return (w + 3) / 4 * 4;
}

void NumberField::setSpec(const NumberSpec &spec)
{
    m_spec = spec;
    m_unitLabel->setText(spec.unit);
    updateSpokenName();
}

void NumberField::setUnit(const QString &unit, const QString &spokenUnit)
{
    m_spec.unit = unit;
    m_spec.spokenUnit = spokenUnit;
    m_unitLabel->setText(unit);
    updateSpokenName();
}

void NumberField::updateSpokenName()
{
    // FM8: Narrator doesn't read the unit beside the box, so its name carries it.
    m_edit->setAccessibleName(m_spec.spokenUnit.isEmpty()
                                  ? m_spokenLabel
                                  : m_spokenLabel + ", in " + m_spec.spokenUnit);
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

void NumberField::setText(const QString &text)
{
    m_edit->setText(text);
    setError(QString());
    emit edited();
}

bool NumberField::isUntouched() const { return m_edit->text() == m_shownText; }
bool NumberField::isBlank() const
{
    double v = 0;
    return WeatherInput::parseNumber(m_edit->text(), v) == WeatherInput::Parse::Blank;
}

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
    m_error->setMessage(message);
    m_edit->setAccessibleDescription(message);   // Narrator reads it with the box
    restyle();
}

QString NumberField::error() const { return m_error->message(); }

void NumberField::restyle()
{
    // FM3: the box's outline turns the error colour too, so it's never colour alone.
    if (m_error->message().isEmpty() || !ThemeManager::instance()) {
        m_edit->setStyleSheet(QString());
        return;
    }
    m_edit->setStyleSheet(QString("QLineEdit { border-color: %1; }")
                              .arg(ThemeManager::instance()->colors().error.name()));
}

void NumberField::setInputEnabled(bool on)
{
    m_edit->setEnabled(on);
    m_label->setEnabled(on);
    m_unitLabel->setEnabled(on);
}
