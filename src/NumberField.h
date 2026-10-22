#pragma once

#include <QString>
#include <QWidget>
#include <functional>
#include <optional>

// Rules and stepping behaviour of one numeric field.
// min/max, step and start are in DISPLAY units (what she sees and types).
struct NumberSpec {
    QString unit;                               // shown after the box ("" = none)
    std::optional<double> min, max;             // unset = no limit
    double step = 1;                            // what the arrows add or subtract
    double start = 0;                           // where the arrows begin when the box is blank
    std::function<double(double)> toDisplay;    // stored -> shown (unset = no change)
    std::function<double(double)> fromDisplay;  // typed -> stored (unset = no change)
};

class QLabel;
class QLineEdit;
class QPushButton;

// A text box with down/up buttons and its own inline error message.
class NumberField : public QWidget
{
    Q_OBJECT

public:
    explicit NumberField(const NumberSpec &spec, QWidget *parent = nullptr);

    void setSpec(const NumberSpec &spec);       // change units/limits (EditorView::applyUnits)

    // `original` is the stored (metric) value. The box shows toDisplay(original).
    void setValue(const std::optional<double> &original);
    // Show `shown` in the box but remember `original` (the exact stored value).
    void setShown(const std::optional<double> &shown, const std::optional<double> &original);

    bool isUntouched() const;                   // text still equals what we displayed
    bool isBlank() const;
    std::optional<double> original() const { return m_original; }
    std::optional<double> typedValue() const;        // parsed from the text, in display units
    std::optional<double> storedTypedValue() const;  // typed text converted to storage units
    std::optional<double> value() const;             // original if untouched, else typed (storage units)

    bool validate();                            // shows/clears the error; true = OK
    void setError(const QString &message);
    void setInputEnabled(bool on);

    QString text() const;    

private:
    void stepBy(int direction);

    NumberSpec m_spec;
    QLineEdit *m_edit = nullptr;
    QPushButton *m_down = nullptr;
    QPushButton *m_up = nullptr;
    QLabel *m_unitLabel = nullptr;
    QLabel *m_error = nullptr;
    std::optional<double> m_original;
    QString m_shownText;
};