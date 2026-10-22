#pragma once

#include <QDialog>

class QListWidget;
class QPushButton;

// Lists the saved backups and lets her pick one.
class RestoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RestoreDialog(QWidget *parent = nullptr);

    QString selectedPath() const { return m_selected; }
    QString selectedDescription() const { return m_description; }

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_restore = nullptr;
    QString m_selected;
    QString m_description;
};
