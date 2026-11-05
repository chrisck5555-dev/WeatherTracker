#pragma once

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QVariantAnimation>

class QWidget;

// A short "push" when the view changes, like turning a page: the new view slides in over
// the old one, which moves a little the other way and dims. Both are solid pictures, so
// nothing is see-through and the real views are not redrawn on every frame.
//
// Use: capture(area) BEFORE the change, then play(direction) right after it. A new capture
// while one is still playing ends the old one at once, so clicking fast never waits.
class ViewTransition : public QObject
{
    Q_OBJECT

public:
    enum class Kind {
        Forward,     // next, later, deeper (open a day, Settings): new view comes from the right
        Backward     // previous, earlier, Back, Done: new view comes from the left
    };

    explicit ViewTransition(QWidget *host);     // `host`: the window area that holds the views

    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    bool isPlaying() const;

    // Widgets that float above the views (the undo bar): kept out of the pictures and
    // kept on top while the animation plays.
    void setFloating(const QList<QWidget *> &widgets);

    void capture(QWidget *area);
    void play(Kind kind);
    void finish();                              // jump to the end

    static constexpr int kDurationMs = 240;

private:
    class Cover;
    QPixmap grab();                             // the host as shown, inside m_rect

    QWidget *m_host;
    Cover *m_cover = nullptr;
    QList<QPointer<QWidget>> m_floating;
    QVariantAnimation m_anim;
    QRect m_rect;
    bool m_enabled = true;
    bool m_captured = false;
};
