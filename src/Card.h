#pragma once

#include <QPainterPath>
#include <QWidget>

class QPainter;
class QVBoxLayout;

// A card (E1, E2, R2, S2): the surface colour with a 1 px outline (3 in high contrast),
// 16 px corners (4 in high contrast) and 16 px padding.
// - Its short, soft shadow (elevation 8) falls outside it, so the widget it sits on paints
//   it: call Card::paintShadows from that widget's paintEvent. The shadow stays within the
//   card's width, so a card at a page edge needs no room beside it.
// - A picture can run edge to edge across its top (setTopPicture); it takes the card's
//   corners (R3, R5) and keeps the outline around it.
class Card : public QWidget
{
    Q_OBJECT

public:
    explicit Card(QWidget *parent = nullptr);

    QVBoxLayout *body() const { return m_body; }   // inside the 16 px padding
    void setTopPicture(QWidget *picture);          // above the padding, edge to edge

    // The clip for a top picture, in that picture's own coordinates.
    QPainterPath topPictureClip(const QWidget *picture) const;

    static void paintShadows(QPainter &p, QWidget *under);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVBoxLayout *m_outer = nullptr;
    QVBoxLayout *m_body = nullptr;
};
