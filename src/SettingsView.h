#pragma once

#include <QList>
#include <QWidget>

#include "TextSize.h"
#include "Theme.h"
#include "Units.h"

class Card;
class ChoiceRow;
class EditorFooter;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class Repository;
class UpdateChecker;

// The Settings screen (plan 2.7, D40): the whole window without the bars. The title, then
// cards (Units, Text size, Look, Help, Your data and backups, Your privacy, Having a
// problem?, About) in two columns when the window is at least 1,100 px wide, one otherwise.
// The strip pinned at the bottom (SC4's hard edge, like the editor's) holds [Use the default
// settings] on the left and [Done] and [Cancel] on the right. Changes show at once; Done
// keeps them (and restarts for a new text size), Cancel puts every setting back.
class SettingsView : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsView(Repository &repo, QWidget *parent = nullptr);

    void open();                        // load current settings into the buttons
    QPushButton *doneButton() const { return m_doneButton; }
    QPushButton *cancelButton() const { return m_cancelButton; }
    EditorFooter *footer() const { return m_footer; }
    static constexpr int kTwoColumnWidth = 1100;

signals:
    void done();
    void guideRequested();
    void tourRequested();
    // "Erase all my weather data" finished; `safetyCopy` holds the data from just before.
    void dataErased(const QString &safetyCopy);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void arrange();
    void matchPairHeights();
    void chooseSize(TextSize::Size size);
    void finish();                      // Done
    void cancel();                      // Cancel: back to how Settings opened
    void useDefaults();
    void setSizeNote(const QString &text);
    void refreshBackupInfo();
    void refreshPrivacy();              // the update-check switch follows the saved setting
    void showProblemReport();           // preview, then save and show the file
    void eraseAllData();
    void deleteAllBackups();
    // Asks, makes a safety copy, swaps the data for `file`, then restarts the program.
    void replaceData(const QString &file, const QString &what);

    Repository &m_repo;
    ChoiceRow *m_unitRow = nullptr;
    ChoiceRow *m_sizeRow = nullptr;
    ChoiceRow *m_styleRow = nullptr;
    ChoiceRow *m_brightRow = nullptr;
    QList<Card *> m_cards;
    QWidget *m_page = nullptr;
    QHBoxLayout *m_columns = nullptr;
    bool m_twoColumns = false;
    bool m_pairUnits = false;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_defaultsButton = nullptr;
    TextSize::Size m_pendingSize = TextSize::Size::Normal;
    struct Before {                     // the settings when Settings opened, for Cancel
        QString units, motion, updates;
        Theme::Style style = Theme::Style::Clay;
        Theme::Brightness brightness = Theme::Brightness::System;
    } m_before;
    EditorFooter *m_footer = nullptr;
    QPushButton *m_doneButton = nullptr;
    QLabel *m_sizeNote = nullptr;
    QLabel *m_backupInfo = nullptr;
    UpdateChecker *m_updates = nullptr;
    QPushButton *m_updateButton = nullptr;
    QCheckBox *m_allowUpdates = nullptr;
    QCheckBox *m_motion = nullptr;
};