#include "FieldError.h"

#include <QHBoxLayout>
#include <QLabel>

#include "Design.h"
#include "Icons.h"
#include "ThemeManager.h"

FieldError::FieldError(QWidget *parent) : QWidget(parent)
{
    m_icon = new QLabel;
    // As tall as one line of Body text (28), so the icon sits on the first line's middle.
    m_icon->setFixedSize(Design::kIconInText, Design::type(Design::TextStyle::Body).lineHeight);
    m_icon->setAlignment(Qt::AlignCenter);
    m_text = new QLabel;
    m_text->setWordWrap(true);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(Design::kGapButtons);                  // icon to words, as in buttons
    row->addWidget(m_icon, 0, Qt::AlignTop);
    row->addWidget(m_text, 1);
    if (auto *t = ThemeManager::instance())
        connect(t, &ThemeManager::themeChanged, this, &FieldError::restyle);
    restyle();
    hide();
}

void FieldError::restyle()
{
    const QColor c = ThemeManager::instance() ? ThemeManager::instance()->colors().error
                                              : QColor("#b3261e");
    m_text->setStyleSheet(QString("color: %1;").arg(c.name()));
    m_icon->setPixmap(Icons::pixmap("warning", Design::kIconInText, c, true, devicePixelRatioF()));
}

void FieldError::setMessage(const QString &message)
{
    m_text->setText(message);
    setVisible(!message.isEmpty());
}

QString FieldError::message() const { return m_text->text(); }
