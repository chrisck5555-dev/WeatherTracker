#include "Card.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include "ClayPaint.h"
#include "Design.h"
#include "Glass.h"
#include "ThemeManager.h"

Card::Card(QWidget *parent)
    : QWidget(parent)
{
    m_outer = new QVBoxLayout(this);
    m_outer->setContentsMargins(0, 0, 0, 0);
    m_outer->setSpacing(0);
    auto *inside = new QWidget;
    m_body = new QVBoxLayout(inside);
    m_body->setContentsMargins(Design::kPadCard, Design::kPadCard, Design::kPadCard, Design::kPadCard);
    m_body->setSpacing(Design::kGapHeader);
    m_outer->addWidget(inside);
}

void Card::setTopPicture(QWidget *picture)
{
    m_outer->insertWidget(0, picture);
}

QPainterPath Card::topPictureClip(const QWidget *picture) const
{
    // Inside the outline, so the outline still shows around the picture.
    const double ow = ClayPaint::outlineWidth();
    const QRectF inner = QRectF(rect()).adjusted(ow, ow, -ow, -ow);
    QPainterPath path = ClayPaint::shape(inner, std::max(0.0, ClayPaint::cardRadius() - ow));
    return path.translated(-QPointF(picture->mapTo(this, QPoint(0, 0))));
}

void Card::paintEvent(QPaintEvent *)
{
    const auto c = ThemeManager::instance()->colors();
    const double ow = ClayPaint::outlineWidth();
    const double radius = ClayPaint::cardRadius();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c.surface);
    p.drawPath(ClayPaint::shape(QRectF(rect()), radius));
    const double half = ow / 2.0;
    p.setPen(QPen(c.border, ow));
    p.setBrush(Qt::NoBrush);
    p.drawPath(ClayPaint::shape(QRectF(rect()).adjusted(half, half, -half, -half), radius - half));
}

void Card::paintShadows(QPainter &p, QWidget *under)
{
    const int reach = Glass::shadowReach(Design::kElevationCard);
    for (Card *card : under->findChildren<Card *>()) {
        if (!card->isVisibleTo(under))
            continue;
        const QRectF r(QPointF(card->mapTo(under, QPoint(0, 0))), QSizeF(card->size()));
        // Narrowed by its reach, so the blur ends at the card's sides: only the part below
        // the card shows.
        Glass::drawShadow(p, r.adjusted(reach, 0, -reach, 0), ClayPaint::cardRadius(),
                          Design::kElevationCard);
    }
}
