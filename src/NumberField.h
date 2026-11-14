#pragma once

#include <QString>
#include <QWidget>
#include <functional>
#include <optional>

// Rules of one numeric field. min and max are in DISPLAY units (what she sees and types).
struct NumberSpec {
    QString unit;                               // shown right after the box ("" = none), FM8
    QString spokenUnit;                         // Narrator's words for it: "degrees Celsius"
    std::optional<double> min, max;             // unset = no limit
    std::function<double(double)> toDisplay;    // stored -> shown (unset = no change)
    std::function<double(double)> fromDisplay;  // typed -> stored (unset = no change)
};

class FieldError;
class QLabel;
class QLineEdit;

// A labelled number field for the editor:
// - the label above (FM1), the box about 5 characters wide (FM7) with no up/down arrows
//   (FM9), and the unit right after it (FM8), also in the box's spoken name
// - a comma, spaces, "°" and a typographic minus are accepted (K5)
// - it's checked when she leaves it or saves, never while she types, and an empty box is
//   never an error (FM4); an error shows right below the box with an icon, words and the
//   box's outline in the error colour (FM3, FM5), and what she typed stays
class NumberField : public QWidget
{
    Q_OBJECT

public:
    explicit NumberField(const QString &label, const QString &spokenLabel, QWidget *parent = nullptr);

    void setSpec(const NumberSpec &spec);       // change units/limits (EditorView::applyUnits)
    void setUnit(const QString &unit, const QString &spokenUnit);   // just the unit (D54)

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
    QString error() const;
    void setInputEnabled(bool on);

    QString text() const;
    void setText(const QString &text);          // as if she typed it (tests, screenshots)
    QLineEdit *edit() const { return m_edit; }
    QLabel *unitLabel() const { return m_unitLabel; }

    static int boxWidth(const QFont &font);     // FM7: about 5 characters

signals:
    void edited();                              // she changed the text

private:
    void restyle();
    void updateSpokenName();

    NumberSpec m_spec;
    QString m_spokenLabel;
    QLabel *m_label = nullptr;
    QLineEdit *m_edit = nullptr;
    QLabel *m_unitLabel = nullptr;
    FieldError *m_error = nullptr;
    std::optional<double> m_original;
    QString m_shownText;
};
