#pragma once

#include <QDate>
#include <QWidget>

class AppState;
class QLabel;
class QPushButton;
class QShowEvent;
class QTextBrowser;
class Repository;

// Read-only summary of the current day: weather, note, events.
class DayView : public QWidget
{
    Q_OBJECT

public:
    DayView(AppState &state, Repository &repo, QWidget *parent = nullptr);

    void reload() { if (isVisible()) refresh(); }   // call after saving (Stage 7)

signals:
    void editRequested(const QDate &date);
    void deleteRequested(const QDate &date);   // "Delete this day" (the window offers Undo)

protected:
    void showEvent(QShowEvent *event) override;

private:
    void refresh();

    AppState &m_state;
    Repository &m_repo;
    QTextBrowser *m_text = nullptr;
    QPushButton *m_editButton = nullptr;   // top "Add / edit this day"
    QPushButton *m_deleteButton = nullptr; // under the text; only when the day has something
    QWidget *m_banner = nullptr;           // weather picture strip (clay looks only)
    QLabel *m_emptyPicture = nullptr;      // friendly picture above the empty-day message
    QWidget *m_empty = nullptr;            // shown instead of m_text when nothing is recorded
};