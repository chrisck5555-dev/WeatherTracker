#include "SettingsView.h"

#include <algorithm>
#include <functional>

#include <QButtonGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

#include "AppLog.h"
#include "Card.h"
#include "ChoiceRow.h"
#include "EditorView.h"
#include "FlowLayout.h"
#include "Backups.h"
#include "Confirm.h"
#include "Display.h"
#include "ProblemReport.h"
#include "RestoreDialog.h"
#include "Repository.h"
#include "Theme.h"
#include "ThemeManager.h"
#include "UpdateChecker.h"
#include "Version.h"
#include "Typography.h"

namespace {

QLabel *heading(const QString &text)
{
    auto *l = new QLabel(text);
    l->setStyleSheet(textStyleSheet(Design::TextStyle::Heading));
    return l;
}

// A Semibold Body line that names a sub-group inside a card (H3: weight before size).
QLabel *subHeading(const QString &text)
{
    auto *l = new QLabel(text);
    QFont f = l->font();
    f.setWeight(QFont::DemiBold);
    l->setFont(f);
    return l;
}

// A paragraph at most 540 px wide (T4), filling from the left (an HBox with a stretch keeps
// its height-for-width; see the handoff's lessons).
QWidget *paragraph(QLabel *label)
{
    label->setWordWrap(true);
    label->setMaximumWidth(Design::kParagraphMaxWidth);
    auto *holder = new QWidget;
    auto *row = new QHBoxLayout(holder);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(label, 1);
    row->addStretch(0);
    return holder;
}

QWidget *paragraph(const QString &text)
{
    auto *l = new QLabel(text);
    l->setTextFormat(Qt::PlainText);
    return paragraph(l);
}

// An ordinary button: as wide as its words, 44 high (Z1, B6), with a tooltip (TP4).
QPushButton *actionButton(const QString &text, const QString &tooltip)
{
    auto *b = new QPushButton(text);
    b->setMinimumHeight(Design::kTarget);
    b->setToolTip(tooltip);
    return b;
}

// A row of buttons that wraps instead of scrolling sideways (RZ2, SC1).
QWidget *buttonRow(std::initializer_list<QPushButton *> buttons)
{
    auto *holder = new QWidget;
    auto *flow = new FlowLayout(holder, Design::kGapButtons, Design::kGapButtons);   // S2
    flow->setContentsMargins(0, 0, 0, 0);
    for (QPushButton *b : buttons)
        flow->addWidget(b);
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    holder->setSizePolicy(policy);
    return holder;
}

// The page under the cards: it paints their shadows (Card::paintShadows).
class SettingsPage : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        Card::paintShadows(p, this);
    }
};

} // namespace

SettingsView::SettingsView(Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_repo(repo)
{
    QFont f = font();
    f = textFont(Design::TextStyle::Body, f);
    setFont(f);

    // --- Units ---
    m_unitRow = new ChoiceRow("Units");
    m_unitRow->addChoice(static_cast<int>(Units::System::Metric), "Metric",
                         "\u00B0C, km/h, mm and hPa: degrees Celsius, kilometres an hour, "
                         "millimetres and hectopascals.");
    m_unitRow->addChoice(static_cast<int>(Units::System::Imperial), "Imperial",
                         "\u00B0F, mph, inches and inHg: degrees Fahrenheit, miles an hour, "
                         "inches and inches of mercury.");

    // --- Text size: all three, on every screen (Z4) ---
    m_sizeRow = new ChoiceRow("Text size");
    for (auto s : TextSize::all())
        m_sizeRow->addChoice(static_cast<int>(s), TextSize::label(s),
                             "Makes everything in the window this size. Weather Tracker restarts to use it when you press Done.");
    m_sizeNote = new QLabel;
    m_sizeNote->setTextFormat(Qt::PlainText);

    // --- Look: style and brightness are independent choices ---
    m_styleRow = new ChoiceRow("Style");
    m_styleRow->addChoice(static_cast<int>(Theme::Style::Clay), "Soft",
                          "Soft colours and rounded shapes.");
    m_styleRow->addChoice(static_cast<int>(Theme::Style::HighContrast), "High contrast",
                          "Strong colours and thick outlines, easiest to see.");
    m_brightRow = new ChoiceRow("Brightness");
    m_brightRow->addChoice(static_cast<int>(Theme::Brightness::Light), "Light", "Dark text on a light background.");
    m_brightRow->addChoice(static_cast<int>(Theme::Brightness::Dark), "Dark", "Light text on a dark background.");
    m_brightRow->addChoice(static_cast<int>(Theme::Brightness::System), "Same as Windows",
                           "Light or dark, whichever Windows is set to.");

    m_motion = new QCheckBox("Short animations when the view changes");
    m_motion->setMinimumHeight(Design::kTarget);
    m_motion->setToolTip("They are also off whenever Windows' animation effects are turned off.");
    m_motion->setVisible(Display::kViewAnimationsInThisVersion);   // hidden while they are off

    // --- Help ---
    auto *guideButton = actionButton("Open the user guide", "How to use Weather Tracker, step by step (F1).");
    auto *tourButton = actionButton("Show the tour again", "The short tour from the first start.");

    // --- Your data and backups ---
    auto *dataNote = new QLabel(QString("Your weather data is stored in:\n%1")
                                    .arg(QDir::toNativeSeparators(m_repo.path())));
    dataNote->setTextFormat(Qt::PlainText);           // folder names are shown as-is
    dataNote->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_backupInfo = new QLabel;
    m_backupInfo->setTextFormat(Qt::PlainText);
    m_backupInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *backupNowButton = actionButton("Back up now", "Make a backup of your weather data now.");
    auto *restoreButton = actionButton("Restore from backup…", "Go back to the data in one of the backups.");
    auto *openFolderButton = actionButton("Open backups folder", "Show the backups folder in File Explorer.");
    auto *saveCopyButton = actionButton("Save a copy to…", "Save a copy of your data, for example on a USB stick.");
    auto *loadCopyButton = actionButton("Load a copy from…", "Use a copy saved on another computer.");
    auto *openDataButton = actionButton("Open data folder", "Show the folder with your weather data in File Explorer.");
    auto *copyNote = new QLabel("To move your data to another computer: press \"Save a copy to\", "
                                "choose the USB stick, then on the other computer press "
                                "\"Load a copy from\" and choose that file.");
    // B4: the erase buttons sit apart from the everyday ones, under their own name.
    auto *eraseButton = actionButton("Erase all weather data…", "Asks first. A safety copy is saved before anything is erased.");
    auto *deleteBackupsButton = actionButton("Delete all backups…", "Asks first. Your weather data is not changed.");

    // --- Your privacy (WR2: the program is "we") ---
    auto *privacyNote = new QLabel(
        "Everything you type stays on this computer, in the data folder shown under "
        "\"Your data and backups\". Backups are kept in your Documents folder.\n\n"
        "We use the internet only when you press \"Check for updates\" (under About), to ask "
        "GitHub for the newest version number. Nothing you have written is ever sent.");
    m_allowUpdates = new QCheckBox("Allow \"Check for updates\" to use the internet");
    m_allowUpdates->setMinimumHeight(Design::kTarget);
    m_allowUpdates->setToolTip("When this is off, the program never uses the internet.");

    // --- Having a problem? ---
    auto *reportButton = actionButton("Show my problem report", "See the report first; nothing is sent.");
    auto *reportNote = new QLabel("If something goes wrong, press this and attach the file "
                                  "it shows you to an email. It never contains your weather "
                                  "values or notes.");

    // --- About ---
    auto *about = new QLabel(QString("Weather Tracker version %1\nCreated by %2\n"
                                     "Built with Qt %3, used under the LGPL v3 license (qt.io).")
                                 .arg(QString::fromUtf8(APP_VERSION), QString::fromUtf8(APP_AUTHOR),
                                      QString::fromLatin1(qVersion())));
    m_updateButton = actionButton("Check for updates", "Asks GitHub for the newest version number.");
    auto *updateButton = m_updateButton;
    m_updates = new UpdateChecker(this);

    // --- The cards (plan 2.7), in 1.1.0's order ---
    auto card = [](const QString &title) {
        auto *c = new Card;
        c->body()->setSpacing(Design::kGapAreas);          // 12 inside, 16 between cards (S3)
        c->body()->addWidget(heading(title));
        c->body()->addSpacing(Design::kGapHeader - Design::kGapAreas);   // heading to content: 8
        return c;
    };
    auto *units = card("Units");
    units->body()->addWidget(m_unitRow);
    auto *size = card("Text size");
    size->body()->addWidget(m_sizeRow);
    size->body()->addWidget(paragraph(m_sizeNote));
    setSizeNote(QString());
    auto *look = card("Look");
    // Style and Brightness side by side, the second going under the first when they don't
    // fit (Chris, 2026-11-13).
    auto group = [](const QString &name, QWidget *row) {
        auto *g = new QWidget;
        auto *v = new QVBoxLayout(g);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(Design::kGapHeader);
        v->addWidget(subHeading(name));
        v->addWidget(row);
        return g;
    };
    auto *lookGroups = new QWidget;
    auto *lookFlow = new FlowLayout(lookGroups, Design::kGapCards * 3 / 2, Design::kGapAreas);  // 24, 12
    lookFlow->setContentsMargins(0, 0, 0, 0);
    lookFlow->addWidget(group("Style", m_styleRow));
    lookFlow->addWidget(group("Brightness", m_brightRow));
    QSizePolicy lookPolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    lookPolicy.setHeightForWidth(true);
    lookGroups->setSizePolicy(lookPolicy);
    look->body()->addWidget(lookGroups);
    look->body()->addWidget(m_motion, 0, Qt::AlignLeft);
    auto *help = card("Help");
    help->body()->addWidget(buttonRow({guideButton, tourButton}));
    auto *data = card("Your data and backups");
    data->body()->addWidget(paragraph(dataNote));
    data->body()->addWidget(paragraph(m_backupInfo));
    data->body()->addWidget(buttonRow({backupNowButton, restoreButton, openFolderButton,
                                       saveCopyButton, loadCopyButton, openDataButton}));
    data->body()->addWidget(paragraph(copyNote));
    data->body()->addSpacing(Design::kGapCards - Design::kGapAreas);   // B4: apart, 16 above
    data->body()->addWidget(subHeading("Erasing"));
    data->body()->addWidget(buttonRow({eraseButton, deleteBackupsButton}));
    auto *privacy = card("Your privacy");
    privacy->body()->addWidget(paragraph(privacyNote));
    privacy->body()->addWidget(m_allowUpdates, 0, Qt::AlignLeft);
    auto *problem = card("Having a problem?");
    problem->body()->addWidget(paragraph(reportNote));
    problem->body()->addWidget(buttonRow({reportButton}));
    auto *aboutCard = card("About");
    aboutCard->body()->addWidget(paragraph(about));
    aboutCard->body()->addWidget(buttonRow({updateButton}));
    m_cards = {units, size, look, help, data, privacy, problem, aboutCard};
    for (Card *c : m_cards)
        c->body()->addStretch(1);       // content at the top when a card is taller (beside another)
    units->installEventFilter(this);    // the pair keeps one height
    size->installEventFilter(this);

    // --- The page: the title, then the cards in one or two columns ---
    auto *title = new QLabel("Settings");
    title->setStyleSheet(textStyleSheet(Design::TextStyle::Title));
    m_page = new SettingsPage;
    for (Card *c : m_cards)
        c->setParent(m_page);
    auto *pageLayout = new QVBoxLayout(m_page);
    pageLayout->setContentsMargins(Design::kWindowMargin, Design::kWindowMargin,
                                   Design::kWindowMargin, Design::kWindowMargin);   // L8
    pageLayout->setSpacing(Design::kGapCards);
    pageLayout->addWidget(title);
    m_columns = new QHBoxLayout;
    m_columns->setContentsMargins(0, 0, 0, 0);
    m_columns->setSpacing(Design::kGapCards);
    pageLayout->addLayout(m_columns);
    pageLayout->addStretch(1);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);    // SC1: never sideways
    scroll->setWidget(m_page);
    scroll->viewport()->setAutoFillBackground(false);   // let the background show through
    m_page->setAutoFillBackground(false);

    // --- Done stays pinned at the bottom right, with SC4's hard edge (plan 2.7, K3) ---
    // [Use the default settings] on the left; [Done] (primary) and [Cancel] on the right,
    // the action before the safe answer, as in the editor (DLG3, K3). Chris, 2026-11-13:
    // "a cancel/revert changes button that leaves the settings window without changing any
    // settings", and "a Set to Default Settings button".
    m_footer = new EditorFooter;
    auto *footerRow = static_cast<QHBoxLayout *>(m_footer->layout());
    m_defaultsButton = actionButton("Use the default settings",
        "Units, text size, look and the internet setting go back to how they were when "
        "Weather Tracker was new. Your weather data is not changed. Cancel undoes this.");
    footerRow->insertWidget(0, m_defaultsButton);
    m_doneButton = actionButton("Done", "Keep the changes and go back to the calendar.");
    m_doneButton->setProperty("wtKind", "primary");
    m_cancelButton = actionButton("Cancel", "Go back to the calendar and undo the changes to these "
                                            "settings. Backups and erasing can't be undone here.");
    footerRow->addWidget(m_doneButton);
    footerRow->addWidget(m_cancelButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(scroll, 1);
    layout->addWidget(m_footer);
    arrange();

    // --- Wiring ---
    connect(m_unitRow, &ChoiceRow::chosenChanged, this, [this](int id) {
        m_repo.setSetting("units", Units::toSetting(static_cast<Units::System>(id)));
    });
    connect(m_sizeRow, &ChoiceRow::chosenChanged, this,
            [this](int id) { chooseSize(static_cast<TextSize::Size>(id)); });
    connect(m_styleRow, &ChoiceRow::chosenChanged, this, [](int id) {
        ThemeManager::instance()->setStyle(static_cast<Theme::Style>(id));
    });
    connect(m_brightRow, &ChoiceRow::chosenChanged, this, [](int id) {
        ThemeManager::instance()->setBrightness(static_cast<Theme::Brightness>(id));
    });
    connect(reportButton, &QPushButton::clicked, this, &SettingsView::showProblemReport);
    connect(backupNowButton, &QPushButton::clicked, this, [this] {
        QString error;
        if (!Backups::backupNow(m_repo, &error))
            QMessageBox::warning(this, "Weather Tracker",
                                 "Sorry, the backup could not be made.\n\n" + error);
        refreshBackupInfo();
    });
    connect(restoreButton, &QPushButton::clicked, this, [this] {
        RestoreDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted)
            replaceData(dialog.selectedPath(), "Your weather data will go back to "
                                               + dialog.selectedDescription() + ".");
    });
    connect(openFolderButton, &QPushButton::clicked, this, [this] {
        QDir().mkpath(Backups::defaultFolder());
        QDesktopServices::openUrl(QUrl::fromLocalFile(Backups::defaultFolder()));
    });
    connect(saveCopyButton, &QPushButton::clicked, this, [this] {
        const QString start = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
            + "/weather-tracker-copy-" + QDate::currentDate().toString("yyyy-MM-dd") + ".db";
        const QString file = QFileDialog::getSaveFileName(
            this, "Save a copy of your weather data", start, "Weather Tracker data (*.db)");
        if (file.isEmpty())
            return;
        if (m_repo.backupTo(file))
            QMessageBox::information(this, "Weather Tracker",
                                     "A copy of your weather data was saved to:\n"
                                     + QDir::toNativeSeparators(file));
        else
            QMessageBox::warning(this, "Weather Tracker",
                                 "Sorry, the copy could not be saved.\n\n" + m_repo.lastError());
    });
    connect(loadCopyButton, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, "Choose a copy of the weather data",
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
            "Weather Tracker data (*.db)");
        if (file.isEmpty())
            return;
        // The thorough check runs before asking, so she never confirms a file that would fail.
        const Repository::FileCheck check = Repository::checkFile(file, true);
        if (!check.ok) {
            QMessageBox::warning(this, "Weather Tracker", check.problem + "\n\nNothing was changed.");
            return;
        }
        replaceData(file, QString("The file you chose has %1 days of weather and %2 notes. "
                                  "It will replace the data in this program.")
                              .arg(check.counts.weatherDays).arg(check.counts.noteDays));
    });
    connect(guideButton, &QPushButton::clicked, this, &SettingsView::guideRequested);
    connect(tourButton, &QPushButton::clicked, this, &SettingsView::tourRequested);
    connect(openDataButton, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_repo.path()).absolutePath()));
    });
    connect(eraseButton, &QPushButton::clicked, this, &SettingsView::eraseAllData);
    connect(deleteBackupsButton, &QPushButton::clicked, this, &SettingsView::deleteAllBackups);

    connect(m_motion, &QCheckBox::toggled, this, [this](bool on) {
        m_repo.setSetting("motion", on ? "on" : "off");
    });
    connect(m_allowUpdates, &QCheckBox::toggled, this, [this](bool on) {
        m_repo.setSetting("update_check", on ? "on" : "off");
        refreshPrivacy();
    });

    connect(updateButton, &QPushButton::clicked, this, [this, updateButton] {
        // Turned off under "Your privacy": say so, and offer to turn it on (Chris, 2026-11-14).
        if (!m_allowUpdates->isChecked()) {
            if (!Confirm::destructive(this, "Checking for updates is turned off",
                                      "Checking needs the internet, which is turned off under "
                                      "\"Your privacy\". Turn it on and check now?",
                                      "Keep it off", "Turn on and check"))
                return;
            m_allowUpdates->setChecked(true);            // saved, and the checker is allowed
        }
        updateButton->setEnabled(false);
        updateButton->setText("Checking\u2026");
        m_updates->check();
    });
    connect(m_updates, &UpdateChecker::finished, this,
            [this, updateButton](const UpdateChecker::Result &r) {
        updateButton->setEnabled(true);
        updateButton->setText("Check for updates");
        using Status = UpdateChecker::Result;
        switch (r.status) {
        case Status::NotSetUp:
            QMessageBox::information(this, "Weather Tracker",
                                     "Checking for updates has not been set up yet.");
            break;
        case Status::Newest:
            QMessageBox::information(this, "Weather Tracker",
                                     QString("You have the newest version (%1).")
                                         .arg(QString::fromUtf8(APP_VERSION)));
            break;
        case Status::Available:
            if (Confirm::destructive(this, "A new version is available",
                                     QString("Version %1 is available. You have version %2.\n\n"
                                             "Open the download page?")
                                         .arg(r.latest, QString::fromUtf8(APP_VERSION)),
                                     "Not now", "Open the download page"))
                QDesktopServices::openUrl(QUrl(r.url));
            break;
        case Status::Failed:
            QMessageBox::warning(this, "Weather Tracker", r.error);
            break;
        }
    });
    connect(m_doneButton, &QPushButton::clicked, this, &SettingsView::finish);
    connect(m_cancelButton, &QPushButton::clicked, this, &SettingsView::cancel);
    connect(m_defaultsButton, &QPushButton::clicked, this, &SettingsView::useDefaults);
}

void SettingsView::refreshBackupInfo()
{
    const auto backups = Backups::list();
    if (backups.isEmpty()) {
        m_backupInfo->setText("No backup has been made yet. We make one every day.");
        return;
    }
    const auto &latest = backups.first();
    m_backupInfo->setText(QString("Last backup: %1. We keep the newest %2 days in:\n%3")
        .arg(QLocale().toString(latest.saved, "MMMM d, yyyy 'at' h:mm AP"))
        .arg(Backups::kKeep)
        .arg(QDir::toNativeSeparators(Backups::defaultFolder())));
}

void SettingsView::replaceData(const QString &file, const QString &what)
{
    if (!Confirm::destructive(this, "Replace your current data?",
                              what + "\n\nYour current data is saved in a safety copy first, "
                              "so nothing is lost. The program will restart.",
                              "Keep my current data", "Replace my data"))
        return;

    QString error;
    const QString safety = Backups::safetyCopy(m_repo, &error);
    if (safety.isEmpty()) {
        QMessageBox::warning(this, "Weather Tracker",
                             "The safety copy could not be made, so nothing was changed.\n\n" + error);
        return;
    }
    if (!m_repo.replaceWith(file)) {
        QMessageBox::warning(this, "Weather Tracker",
                             "Sorry, the data could not be replaced. Your data was not changed.\n\n"
                             + m_repo.lastError());
        return;
    }
    AppLog::write("Weather data was replaced from a backup or a copy");
    // After the restart the window offers Undo, which goes back to this safety copy.
    m_repo.setSetting("undo_replace", safety);
    QMessageBox::information(this, "Weather Tracker",
                             "Done. Weather Tracker will now restart.");
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    QCoreApplication::quit();
}

// The note under the text sizes takes no room while it's empty.
void SettingsView::setSizeNote(const QString &text)
{
    m_sizeNote->setText(text);
    m_sizeNote->parentWidget()->setVisible(!text.isEmpty());
}

void SettingsView::open()
{
    m_before.units = m_repo.setting("units", "metric");
    m_before.motion = m_repo.setting("motion", "on");
    m_before.updates = m_repo.setting("update_check", "on");
    m_before.style = ThemeManager::instance()->style();
    m_before.brightness = ThemeManager::instance()->brightness();
    m_unitRow->setChosen(static_cast<int>(Units::fromSetting(m_before.units)));
    m_pendingSize = TextSize::fromSetting(m_repo.setting("text_size", "normal"));
    m_sizeRow->setChosen(static_cast<int>(m_pendingSize));
    m_styleRow->setChosen(static_cast<int>(ThemeManager::instance()->style()));
    m_brightRow->setChosen(static_cast<int>(ThemeManager::instance()->brightness()));

    {
        const QSignalBlocker quiet(m_motion);
        m_motion->setChecked(m_repo.setting("motion", "on") != "off");
    }
    setSizeNote(QString());
    refreshBackupInfo();
    refreshPrivacy();
}

// Plan 2.7: two columns of cards when the window is at least 1,100 px wide, one otherwise.
// Each column stacks on its own, so a tall card never leaves a gap beside it. Tab follows
// the reading order: down the left column, then the right (KB1).
void SettingsView::arrange()
{
    const bool two = width() >= kTwoColumnWidth;
    const int page = width() - 2 * Design::kWindowMargin;
    const int column = two ? (page - Design::kGapCards) / 2 : page;
    // Units and Text size stay two cards of the same width, side by side only when each
    // half holds its buttons on one line, stacked otherwise (Chris, 2026-11-13 and 11-14).
    const int half = std::max(m_cards[0]->sizeHint().width(), m_cards[1]->sizeHint().width());
    const bool pair = 2 * half + Design::kGapCards <= column;
    if (m_columns->count() > 0 && two == m_twoColumns && pair == m_pairUnits)
        return;
    m_twoColumns = two;
    m_pairUnits = pair;
    matchPairHeights();
    std::function<void(QLayout *)> empty = [&empty](QLayout *l) {
        while (QLayoutItem *item = l->takeAt(0)) {
            if (QLayout *inner = item->layout())
                empty(inner);
            delete item;
        }
    };
    empty(m_columns);
    // m_cards: units, size, look, help, data, privacy, problem, about.
    auto stack = [this, pair](const QList<Card *> &cards) {
        auto *v = new QVBoxLayout;
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(Design::kGapCards);
        for (Card *c : cards) {
            if (c == m_cards[1] && pair)
                continue;                                  // beside Units
            if (c == m_cards[0] && pair) {
                auto *h = new QHBoxLayout;
                h->setContentsMargins(0, 0, 0, 0);
                h->setSpacing(Design::kGapCards);
                h->addWidget(m_cards[0], 1);                 // the same width
                h->addWidget(m_cards[1], 1);
                v->addLayout(h);
                continue;
            }
            v->addWidget(c);
        }
        v->addStretch(1);
        return v;
    };
    // Two columns: the settings she changes on the left, with About at the bottom; her data
    // on the right (Chris, 2026-11-13).
    const QList<Card *> left = {m_cards[0], m_cards[1], m_cards[2], m_cards[3], m_cards[6], m_cards[7]};
    const QList<Card *> right = {m_cards[4], m_cards[5]};
    if (two) {
        m_columns->addLayout(stack(left), 1);
        m_columns->addLayout(stack(right), 1);
    } else {
        m_columns->addLayout(stack(m_cards), 1);
    }
    const QList<Card *> order = two ? left + right : m_cards;
    QWidget *previous = nullptr;
    for (Card *c : order) {
        for (QWidget *w : c->findChildren<QWidget *>()) {
            if (w->focusPolicy() == Qt::NoFocus)
                continue;
            if (previous)
                QWidget::setTabOrder(previous, w);
            previous = w;
        }
    }
    for (QWidget *w : {static_cast<QWidget *>(m_defaultsButton), static_cast<QWidget *>(m_doneButton),
                       static_cast<QWidget *>(m_cancelButton)}) {
        if (previous)
            QWidget::setTabOrder(previous, w);
        previous = w;
    }
}

void SettingsView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    arrange();
}

// Side by side, Units and Text size are always the same height (Chris, 2026-11-14): each
// is held at least as tall as the other needs at its width.
bool SettingsView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize && (watched == m_cards[0] || watched == m_cards[1]))
        matchPairHeights();
    return QWidget::eventFilter(watched, event);
}

void SettingsView::matchPairHeights()
{
    int h = 0;
    if (m_pairUnits)
        for (Card *c : {m_cards[0], m_cards[1]})
            h = std::max(h, c->hasHeightForWidth() ? c->heightForWidth(c->width())
                                                   : c->sizeHint().height());
    for (Card *c : {m_cards[0], m_cards[1]})
        if (c->minimumHeight() != h)
            c->setMinimumHeight(h);
}

void SettingsView::refreshPrivacy()
{
    const bool updates = m_repo.setting("update_check", "on") != "off";
    {
        const QSignalBlocker quiet(m_allowUpdates);    // showing the state is not a change
        m_allowUpdates->setChecked(updates);
    }
    m_updates->setAllowed(updates);
}

void SettingsView::showProblemReport()
{
    const QString text = ProblemReport::build(m_repo, AppLog::tail(40));

    // Show exactly what will be in the file before anything is saved.
    QDialog preview(this);
    preview.setWindowTitle("Your problem report");
    preview.resize(760, 600);
    auto *intro = new QLabel("This is everything in the report. It has no weather values, "
                             "and no notes, and your Windows user name is replaced "
                             "with <user>. Press \"Save the report\" to save it as a file you "
                             "can attach to an email.");
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::PlainText);
    auto *view = new QPlainTextEdit(text);
    view->setReadOnly(true);
    auto *cancel = new QPushButton("Cancel");
    auto *save = new QPushButton("Save the report");
    for (auto *b : {cancel, save})
        b->setMinimumSize(160, 52);
    save->setDefault(true);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(cancel);
    buttons->addStretch(1);
    buttons->addWidget(save);
    auto *layout = new QVBoxLayout(&preview);
    layout->addWidget(intro);
    layout->addWidget(view, 1);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &preview, &QDialog::reject);
    connect(save, &QPushButton::clicked, &preview, &QDialog::accept);
    if (preview.exec() != QDialog::Accepted)
        return;

    const QString folder = QFileInfo(AppLog::path()).absolutePath();
    const QString file = ProblemReport::write(folder, text);
    if (file.isEmpty()) {
        QMessageBox::warning(this, "Weather Tracker",
                             "Sorry, the problem report could not be created.");
        return;
    }
    // Open the folder with the report highlighted. /select needs backslashes.
    QProcess::startDetached("explorer.exe", {"/select," + QDir::toNativeSeparators(file)});
}

void SettingsView::eraseAllData()
{
    const Repository::Counts c = m_repo.counts();
    if (c.weatherDays + c.noteDays + c.eventDays == 0) {
        QMessageBox::information(this, "Weather Tracker", "There is no weather data to erase.");
        return;
    }
    if (!Confirm::destructive(this, "Erase all your weather data?",
            QString("This erases %1 days of weather, %2 notes and the events on %3 days.\n\n"
                    "Your settings and backups are kept, and a safety copy of the data is "
                    "saved in the backups folder first.")
                .arg(c.weatherDays).arg(c.noteDays).arg(c.eventDays),
            "Keep my data", "Erase everything"))
        return;

    QString error;
    const QString safety = Backups::safetyCopy(m_repo, &error);
    if (safety.isEmpty()) {
        QMessageBox::warning(this, "Weather Tracker",
                             "The safety copy could not be made, so nothing was erased.\n\n" + error);
        return;
    }
    if (!m_repo.eraseAll()) {
        QMessageBox::warning(this, "Weather Tracker",
                             "Sorry, the data could not be erased. Nothing was changed.\n\n"
                             + m_repo.lastError());
        return;
    }
    AppLog::write("All weather data was erased (a safety copy was made first)");
    emit dataErased(safety);                            // the window offers Undo
}

void SettingsView::deleteAllBackups()
{
    const int n = Backups::countAll();
    if (n == 0) {
        QMessageBox::information(this, "Weather Tracker", "There are no backups to delete.");
        return;
    }
    if (!Confirm::destructive(this, "Delete all backups?",
            QString("This deletes %1 backup %2 from:\n%3\n\nYour weather data in the program "
                    "is not changed. The program makes a new backup each day.")
                .arg(n).arg(n == 1 ? "file" : "files")
                .arg(QDir::toNativeSeparators(Backups::defaultFolder())),
            "Keep the backups", "Delete the backups"))
        return;
    const bool ok = Backups::deleteAll();
    AppLog::write(ok ? "All backups were deleted" : "Some backups could not be deleted");
    refreshBackupInfo();
    if (!ok)
        QMessageBox::warning(this, "Weather Tracker",
                             "Some backups could not be deleted. They may be open in another "
                             "program. Please try again.");
}

void SettingsView::chooseSize(TextSize::Size size)
{
    // Chosen now, used when she presses Done (Cancel forgets it).
    m_pendingSize = size;
    const bool same = std::abs(TextSize::factor(size) - Display::currentScaleFactor()) < 0.001;
    setSizeNote(same ? QString()
                     : QString("Weather Tracker restarts to use this size when you press Done."));
}

// Done: keep everything. A new text size needs a restart, so ask about that now.
void SettingsView::finish()
{
    const auto saved = TextSize::fromSetting(m_repo.setting("text_size", "normal"));
    if (m_pendingSize != saved)
        m_repo.setSetting("text_size", TextSize::toSetting(m_pendingSize));
    const double wanted = TextSize::factor(m_pendingSize);
    if (std::abs(wanted - Display::currentScaleFactor()) >= 0.001) {
        if (Confirm::destructive(this, "Restart to change the text size?",
                                 "Weather Tracker needs to restart to change the text size. "
                                 "Your weather records are safe.",
                                 "Not now", "Restart now")) {
            if (Display::relaunchWithScale(wanted)) {
                QCoreApplication::quit();
                return;
            }
            QMessageBox::warning(this, "Weather Tracker",
                                 "Sorry, Weather Tracker could not restart itself.\n"
                                 "Please close it and open it again.");
        }
    }
    emit done();
}

// Cancel: every setting goes back to how it was when Settings opened.
void SettingsView::cancel()
{
    m_repo.setSetting("units", m_before.units);
    m_repo.setSetting("motion", m_before.motion);
    m_repo.setSetting("update_check", m_before.updates);
    auto *theme = ThemeManager::instance();
    if (theme->style() != m_before.style)
        theme->setStyle(m_before.style);
    if (theme->brightness() != m_before.brightness)
        theme->setBrightness(m_before.brightness);
    emit done();
}

// The settings a new Weather Tracker starts with. Cancel still undoes this.
void SettingsView::useDefaults()
{
    m_unitRow->setChosen(static_cast<int>(Units::System::Metric));
    m_repo.setSetting("units", Units::toSetting(Units::System::Metric));
    m_sizeRow->setChosen(static_cast<int>(TextSize::Size::Normal));
    chooseSize(TextSize::Size::Normal);
    m_styleRow->setChosen(static_cast<int>(Theme::Style::Clay));
    m_brightRow->setChosen(static_cast<int>(Theme::Brightness::System));
    auto *theme = ThemeManager::instance();
    if (theme->style() != Theme::Style::Clay)
        theme->setStyle(Theme::Style::Clay);
    if (theme->brightness() != Theme::Brightness::System)
        theme->setBrightness(Theme::Brightness::System);
    m_motion->setChecked(true);                 // its toggled() saves it
    m_allowUpdates->setChecked(true);
}
