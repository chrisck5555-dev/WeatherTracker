#include "SettingsView.h"

#include <QButtonGroup>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

#include "AppLog.h"
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

namespace {

QLabel *heading(const QString &text)
{
    auto *l = new QLabel(text);
    l->setStyleSheet("font-size: 16pt; font-weight: bold;");
    return l;
}

QPushButton *choiceButton(const QString &text)
{
    auto *b = new QPushButton(text);
    b->setCheckable(true);
    b->setMinimumSize(180, 60);
    return b;
}

QPushButton *smallButton(const QString &text)
{
    auto *b = new QPushButton(text);
    b->setMinimumSize(200, 46);
    return b;
}

} // namespace

SettingsView::SettingsView(Repository &repo, QWidget *parent)
    : QWidget(parent)
    , m_repo(repo)
    , m_unitGroup(new QButtonGroup(this))
    , m_sizeGroup(new QButtonGroup(this))
    , m_styleGroup(new QButtonGroup(this))
    , m_brightGroup(new QButtonGroup(this))
{
    QFont f = font();
    f.setPointSize(13);
    setFont(f);

    auto *title = new QLabel("Settings");
    title->setStyleSheet("font-size: 20pt; font-weight: bold;");

    // --- Units: ids are the enum values ---
    auto *unitRow = new QHBoxLayout;
    auto *metric = choiceButton("Metric\n\u00B0C, km/h, mm, hPa");
    auto *imperial = choiceButton("Imperial\n\u00B0F, mph, inches, inHg");
    m_unitGroup->addButton(metric, static_cast<int>(Units::System::Metric));
    m_unitGroup->addButton(imperial, static_cast<int>(Units::System::Imperial));
    unitRow->addWidget(metric);
    unitRow->addWidget(imperial);
    unitRow->addStretch(1);

    // --- Text size: all three exist; open() hides the ones that don't fit ---
    auto *sizeRow = new QHBoxLayout;
    for (auto s : {TextSize::Size::Normal, TextSize::Size::Large, TextSize::Size::ExtraLarge}) {
        auto *b = choiceButton(TextSize::label(s));
        m_sizeGroup->addButton(b, static_cast<int>(s));
        sizeRow->addWidget(b);
    }
    sizeRow->addStretch(1);

    m_sizeNote = new QLabel;
    m_sizeNote->setWordWrap(true);

    // --- Look: style and brightness are independent choices ---
    auto *styleRow = new QHBoxLayout;
    auto *clay = choiceButton("Soft\n(clay look)");
    auto *contrast = choiceButton("High contrast\n(bold outlines)");
    m_styleGroup->addButton(clay, static_cast<int>(Theme::Style::Clay));
    m_styleGroup->addButton(contrast, static_cast<int>(Theme::Style::HighContrast));
    styleRow->addWidget(clay);
    styleRow->addWidget(contrast);
    styleRow->addStretch(1);

    auto *brightRow = new QHBoxLayout;
    for (auto b : {Theme::Brightness::Light, Theme::Brightness::Dark, Theme::Brightness::System}) {
        const QString text = b == Theme::Brightness::Light ? "Light"
                           : b == Theme::Brightness::Dark  ? "Dark" : "Same as Windows";
        auto *btn = choiceButton(text);
        m_brightGroup->addButton(btn, static_cast<int>(b));
        brightRow->addWidget(btn);
    }
    brightRow->addStretch(1);

    // --- Problem report ---
    auto *reportButton = new QPushButton("Show my problem report");
    reportButton->setMinimumSize(240, 46);
    auto *reportNote = new QLabel("If something goes wrong, press this and attach the file "
                                  "it shows you to an email. It never contains your weather "
                                  "values or notes.");
    reportNote->setWordWrap(true);

    // --- Data and backups ---
    auto *dataNote = new QLabel(QString("Your weather data is stored in:\n%1")
                                    .arg(QDir::toNativeSeparators(m_repo.path())));
    dataNote->setWordWrap(true);
    dataNote->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_backupInfo = new QLabel;
    m_backupInfo->setWordWrap(true);
    m_backupInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *backupNowButton = smallButton("Back up now");
    auto *restoreButton = smallButton("Restore from backup\u2026");
    auto *openFolderButton = smallButton("Open backups folder");
    auto *saveCopyButton = smallButton("Save a copy to\u2026");
    auto *loadCopyButton = smallButton("Load a copy from\u2026");
    auto *dataGrid = new QGridLayout;
    dataGrid->addWidget(backupNowButton, 0, 0);
    dataGrid->addWidget(restoreButton, 0, 1);
    dataGrid->addWidget(openFolderButton, 0, 2);
    dataGrid->addWidget(saveCopyButton, 1, 0);
    dataGrid->addWidget(loadCopyButton, 1, 1);
    dataGrid->setColumnStretch(3, 1);
    auto *copyNote = new QLabel("To move your data to another computer: press \"Save a copy to\", "
                                "choose the USB stick, then on the other computer press "
                                "\"Load a copy from\" and choose that file.");
    copyNote->setWordWrap(true);

    // --- About ---
    auto *about = new QLabel(QString("Weather Tracker version %1\nCreated by %2\n"
                                     "Built with Qt %3, used under the LGPL v3 license (qt.io).")
                                 .arg(QString::fromUtf8(APP_VERSION), QString::fromUtf8(APP_AUTHOR),
                                      QString::fromLatin1(qVersion())));
    about->setWordWrap(true);
    auto *updateButton = smallButton("Check for updates");
    m_updates = new UpdateChecker(this);

    // --- Help ---
    auto *guideButton = smallButton("Open the user guide");
    auto *tourButton = smallButton("Show the tour again");
    auto *helpRow = new QHBoxLayout;
    helpRow->addWidget(guideButton);
    helpRow->addWidget(tourButton);
    helpRow->addStretch(1);

    // --- Scrollable content ---
    auto *content = new QWidget;
    auto *body = new QVBoxLayout(content);
    body->addWidget(title);
    body->addSpacing(12);
    body->addWidget(heading("Units"));
    body->addLayout(unitRow);
    body->addSpacing(16);
    body->addWidget(heading("Text size"));
    body->addLayout(sizeRow);
    body->addWidget(m_sizeNote);
    body->addSpacing(16);
    body->addWidget(heading("Look"));
    body->addLayout(styleRow);
    body->addLayout(brightRow);
    body->addSpacing(16);
    body->addWidget(heading("Your data and backups"));
    body->addWidget(dataNote);
    body->addWidget(m_backupInfo);
    body->addLayout(dataGrid);
    body->addWidget(copyNote);
    body->addSpacing(16);
    body->addWidget(heading("Help"));
    body->addLayout(helpRow);
    body->addSpacing(16);
    body->addWidget(heading("Having a problem?"));
    body->addWidget(reportButton, 0, Qt::AlignLeft);
    body->addWidget(reportNote);
    body->addSpacing(16);
    body->addWidget(heading("About"));
    body->addWidget(about);
    body->addWidget(updateButton, 0, Qt::AlignLeft);
    body->addStretch(1);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    scroll->viewport()->setAutoFillBackground(false);   // let the background show through
    content->setAutoFillBackground(false);

    // --- Done stays pinned at the bottom ---
    auto *doneButton = new QPushButton("Done");
    doneButton->setMinimumSize(120, 44);
    auto *doneRow = new QHBoxLayout;
    doneRow->addStretch(1);
    doneRow->addWidget(doneButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(scroll, 1);
    layout->addLayout(doneRow);

    // --- Wiring ---
    connect(m_unitGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_repo.setSetting("units", Units::toSetting(static_cast<Units::System>(id)));
    });
    connect(m_sizeGroup, &QButtonGroup::idClicked, this,
            [this](int id) { chooseSize(static_cast<TextSize::Size>(id)); });
    connect(m_styleGroup, &QButtonGroup::idClicked, this, [](int id) {
        ThemeManager::instance()->setStyle(static_cast<Theme::Style>(id));
    });
    connect(m_brightGroup, &QButtonGroup::idClicked, this, [](int id) {
        ThemeManager::instance()->setBrightness(static_cast<Theme::Brightness>(id));
    });
    connect(reportButton, &QPushButton::clicked, this, [this] {
        const QString folder = QFileInfo(AppLog::path()).absolutePath();
        const QString file = ProblemReport::write(m_repo, folder, AppLog::tail(40));
        if (file.isEmpty()) {
            QMessageBox::warning(this, "Weather Tracker",
                                 "Sorry, the problem report could not be created.");
            return;
        }
        // Open the folder with the report highlighted. /select needs backslashes.
        QProcess::startDetached("explorer.exe",
                                {"/select," + QDir::toNativeSeparators(file)});
    });
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
        const auto counts = Repository::inspect(file);
        if (!counts) {
            QMessageBox::warning(this, "Weather Tracker",
                                 "That file is not Weather Tracker data, so nothing was changed.");
            return;
        }
        replaceData(file, QString("The file you chose has %1 days of weather and %2 notes. "
                                  "It will replace the data in this program.")
                              .arg(counts->weatherDays).arg(counts->noteDays));
    });
    connect(guideButton, &QPushButton::clicked, this, &SettingsView::guideRequested);
    connect(tourButton, &QPushButton::clicked, this, &SettingsView::tourRequested);
    connect(updateButton, &QPushButton::clicked, this, [this, updateButton] {
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
    connect(doneButton, &QPushButton::clicked, this, &SettingsView::done);
}

void SettingsView::refreshBackupInfo()
{
    const auto backups = Backups::list();
    if (backups.isEmpty()) {
        m_backupInfo->setText("No backup has been made yet. The program makes one every day.");
        return;
    }
    const auto &latest = backups.first();
    m_backupInfo->setText(QString("Last backup: %1. The program keeps the newest %2 days in:\n%3")
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
    if (Backups::safetyCopy(m_repo, &error).isEmpty()) {
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
    QMessageBox::information(this, "Weather Tracker",
                             "Done. Weather Tracker will now restart.");
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    QCoreApplication::quit();
}

void SettingsView::open()
{
    const auto units = Units::fromSetting(m_repo.setting("units", "metric"));
    m_unitGroup->button(static_cast<int>(units))->setChecked(true);

    const auto allowed = Display::allowedTextSizes();
    const auto saved = TextSize::fromSetting(m_repo.setting("text_size", "normal"));
    for (auto s : {TextSize::Size::Normal, TextSize::Size::Large, TextSize::Size::ExtraLarge})
        m_sizeGroup->button(static_cast<int>(s))->setVisible(allowed.contains(s));
    m_sizeGroup->button(static_cast<int>(TextSize::effective(saved, allowed)))->setChecked(true);

    m_styleGroup->button(static_cast<int>(ThemeManager::instance()->style()))->setChecked(true);
    m_brightGroup->button(static_cast<int>(ThemeManager::instance()->brightness()))->setChecked(true);

    m_sizeNote->clear();
    refreshBackupInfo();
}

void SettingsView::chooseSize(TextSize::Size size)
{
    m_repo.setSetting("text_size", TextSize::toSetting(size));

    const double wanted = TextSize::factor(size);
    if (std::abs(wanted - Display::currentScaleFactor()) < 0.001) {
        m_sizeNote->clear();                     // already showing at this size
        return;
    }

    if (Confirm::destructive(this, "Change the text size?",
                             "The program needs to restart to change the text size.\n"
                             "Your weather records are safe.",
                             "Not now", "Restart now")) {
        if (Display::relaunchWithScale(wanted)) {
            QCoreApplication::quit();
            return;
        }
        QMessageBox::warning(this, "Weather Tracker",
                             "Sorry, the program could not restart itself.\n"
                             "Please close it and open it again.");
    }
    m_sizeNote->setText("The new text size will be used the next time you open the program.");
}