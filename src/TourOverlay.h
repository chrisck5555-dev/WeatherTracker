#pragma once

#include <QList>
#include <QPointer>
#include <QRect>
#include <QSet>
#include <QWidget>
#include <functional>

class PanelHost;
class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

// A widget the current tour step wants the user to use. `important` ones are kept
// uncovered by the tour card.
struct TourTarget {
    QWidget *widget = nullptr;
    bool important = false;
};

// The interactive tour. It greys out the whole window except the parts the current step
// is about (they get an outline and stay clickable), and shows a card with the
// instructions and a checklist. "Next" stays off until everything is ticked.
class TourOverlay : public QWidget
{
    Q_OBJECT

public:
    struct Hooks {
        std::function<QList<TourTarget>(int step)> targets;   // what is usable right now
        std::function<void()> resetScreen;                    // back to a known screen
        std::function<bool()> editorDirty;                    // something typed in the editor
    };

    TourOverlay(QWidget *root, PanelHost *host, Hooks hooks);
    ~TourOverlay() override;

    void start();
    void report(const QString &event);       // the window tells us what the user did
    void cardToFront();

signals:
    void finished();                         // finished or skipped

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Item {
        QString text;
        QString event;
    };
    struct Step {
        QString title;
        QString text;
        QList<Item> items;
    };

    void buildCard();
    void showStep(int index);
    void updateChecklist();
    void refreshTargets(bool force);
    void placeCard();
    bool keyAllowed(QObject *receiver) const;
    void tick();
    void finish();

    QWidget *m_root;
    PanelHost *m_host;
    Hooks m_hooks;
    QList<Step> m_steps;
    int m_step = -1;
    bool m_finished = false;
    bool m_placing = false;
    QSet<QString> m_events;                  // what was done in this step

    QPointer<QWidget> m_card;
    QLabel *m_counter = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_text = nullptr;
    QLabel *m_hint = nullptr;
    QVBoxLayout *m_checks = nullptr;
    QList<QLabel *> m_checkLabels;
    QPushButton *m_skip = nullptr;
    QPushButton *m_back = nullptr;
    QPushButton *m_next = nullptr;

    QList<TourTarget> m_targets;
    QList<QRect> m_holes;
    QList<QRect> m_important;
    QTimer *m_timer = nullptr;
};
