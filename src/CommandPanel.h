#pragma once

#include <QList>
#include <QWidget>
#include <functional>

class QLineEdit;
class QTreeWidget;

// One thing the command menu can do.
struct Command {
    QString label;                  // "Search notes and events"
    QString keys;                   // "Ctrl+F", or "" when it has no key
    QString words;                  // extra words to find it by: "find look"
    std::function<void()> run;
};

// The command menu (Ctrl+K): type a few letters, press Enter. Every action and its key,
// in one list, so the keys are easy to learn. Shown inside the main window by PanelHost.
class CommandPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CommandPanel(QList<Command> commands, QWidget *parent = nullptr);

signals:
    void chosen(const Command &command);
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void filter(const QString &text);
    void runCurrent();

    QList<Command> m_commands;
    QLineEdit *m_input = nullptr;
    QTreeWidget *m_list = nullptr;
};
