#pragma once

#include <QWidget>

#include "TextSize.h"
#include "Units.h"

class QButtonGroup;
class QCheckBox;
class QLabel;
class QPushButton;
class Repository;
class UpdateChecker;

// The in-window Settings screen.
class SettingsView : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsView(Repository &repo, QWidget *parent = nullptr);

    void open();                        // load current settings into the buttons

signals:
    void done();
    void guideRequested();
    void tourRequested();
    void dataErased();                  // "Erase all my weather data": the views reload

private:
    void chooseSize(TextSize::Size size);
    void refreshBackupInfo();
    void refreshPrivacy();              // the update-check switch follows the saved setting
    void showProblemReport();           // preview, then save and show the file
    void eraseAllData();
    void deleteAllBackups();
    // Asks, makes a safety copy, swaps the data for `file`, then restarts the program.
    void replaceData(const QString &file, const QString &what);

    Repository &m_repo;
    QButtonGroup *m_unitGroup = nullptr;
    QButtonGroup *m_sizeGroup = nullptr;
    QButtonGroup *m_styleGroup = nullptr;
    QButtonGroup *m_brightGroup = nullptr;
    QLabel *m_sizeNote = nullptr;
    QLabel *m_backupInfo = nullptr;
    UpdateChecker *m_updates = nullptr;
    QPushButton *m_updateButton = nullptr;
    QLabel *m_updateNote = nullptr;
    QCheckBox *m_allowUpdates = nullptr;
};