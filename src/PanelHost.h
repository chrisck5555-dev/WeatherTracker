#pragma once

#include <QPointer>
#include <QSize>
#include <QWidget>

// A layer over the whole window. It dims and blocks everything behind it and shows
// one card in the middle. Esc, or a click on the dimmed area, closes it.
class PanelHost : public QWidget
{
    Q_OBJECT

public:
    explicit PanelHost(QWidget *parent);

    void setUnderlying(QWidget *widget);              // switched off while a panel is open
    void showPanel(QWidget *content, const QSize &preferred);
    void closePanel();
    bool isOpen() const { return m_content != nullptr; }
    void setAvoid(const QRect &area);                 // keep the card out of this rectangle

signals:
    void opened();
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void place();

    QWidget *m_card = nullptr;
    QWidget *m_content = nullptr;
    QPointer<QWidget> m_underlying;
    QPointer<QWidget> m_previousFocus;
    QSize m_preferred;
    QRect m_avoid;
    bool m_pressedOutside = false;
};
