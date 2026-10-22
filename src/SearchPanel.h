#pragma once

#include <QDate>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QTimer;
class Repository;

// Finds days by what is written in their notes or by their events.
// Clicking a result emits dayChosen(). Shown inside the main window by PanelHost.
class SearchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanel(Repository &repo, QWidget *parent = nullptr);

signals:
    void dayChosen(const QDate &date);
    void closeRequested();

private:
    void runSearch();

    Repository &m_repo;
    QLineEdit *m_input = nullptr;
    QLabel *m_status = nullptr;
    QListWidget *m_list = nullptr;
    QTimer *m_delay = nullptr;
    bool m_done = false;
};
