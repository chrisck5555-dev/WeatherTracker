#pragma once

#include <QWidget>

class QLabel;

// A field's error, shown right beside it (FM3): a warning icon and plain words, in the
// error colour (C1: 4.5:1). Hidden while there is no message. The words wrap, and the
// colours follow the look.
class FieldError : public QWidget
{
    Q_OBJECT

public:
    explicit FieldError(QWidget *parent = nullptr);

    void setMessage(const QString &message);   // empty hides it
    QString message() const;

private:
    void restyle();

    QLabel *m_icon = nullptr;
    QLabel *m_text = nullptr;
};
