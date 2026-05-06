#include "launchermainwindow.h"
#include "./ui_launchermainwindow.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QMenu>
#include <QMessageBox>
#include <QMovie>
#include <QObject>
#include <QProcess>
#include <QShortcut>
#include <QTimer>
#include <QUrl>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>

#include "apiclient.h"
#include "campaign.h"
#include "certificate.h"
#include "clickablelabel.h"
#include "copydkfilesdialog.h"
#include "directconnectdialog.h"
#include "dkfiles.h"
#include "downloadmusicdialog.h"
#include "enetservertestdialog.h"
#include "fileremover.h"
#include "fileremoverdialog.h"
#include "game.h"
#include "helper.h"
#include "imagehelper.h"
#include "installkfxdialog.h"
#include "kfxversion.h"
#include "launcheroptions.h"
#include "modmanager.h"
#include "modmanagerdialog.h"
#include "newsarticlewidget.h"
#include "runpacketfiledialog.h"
#include "savefile.h"
#include "scannetworkdialog.h"
#include "settings.h"
#include "settingsdialog.h"
#include "updatedialog.h"
#include "version.h"
#include "workshopitemwidget.h"

#define MAX_WORKSHOP_ITEMS_SHOWN 4
#define MAX_NEWS_ARTICLES_SHOWN 3

LauncherMainWindow::LauncherMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LauncherMainWindow)
    , game(new Game(parent))
{
    ui->setupUi(this);

    // Connect signals and slots
    connect(this, &LauncherMainWindow::kfxNetRetrieval, this, &LauncherMainWindow::onKfxNetRetrieval, Qt::QueuedConnection);
    connect(this, &LauncherMainWindow::kfxNetImagesLoaded, this, &LauncherMainWindow::onKfxNetImagesLoaded, Qt::QueuedConnection);
    connect(this, &LauncherMainWindow::updateFound, this, &LauncherMainWindow::onUpdateFound);
    connect(this, &LauncherMainWindow::showUpdateIcon, this, &LauncherMainWindow::onShowUpdateIcon);
    connect(this, &LauncherMainWindow::filesToRemoveFound, this, &LauncherMainWindow::onFilesToRemoveFound);
    connect(game, &Game::gameEnded, this, &LauncherMainWindow::onGameEnded);

    // Clear placeholders
    ui->versionLabel->setText("");
    ui->spinnerLabel->setText("");
    clearLatestFromKfxNet();

    // Add enabled and disabled caret icon to "play extra" button
    // Note: The normal icon file is also set in the .ui file
    QIcon caretDownIcon;
    caretDownIcon.addFile("://res/img/caret-down.png", QSize(), QIcon::Normal);
    caretDownIcon.addFile("://res/img/caret-down-disabled.png", QSize(), QIcon::Disabled);
    ui->playExtraButton->setIcon(caretDownIcon);

    // Disable resizing and remove maximize button
    // This does not work on Wayland (for now)
    setFixedSize(size());
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    // Raise and activate window
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();

    // Create clickable version label
    ClickableLabel *clickableVersionLabel = new ClickableLabel(this);
    clickableVersionLabel->setBaseColor(QColor(0x99, 0x99, 0x99));
    clickableVersionLabel->setFont(ui->versionLabel->font());
    connect(clickableVersionLabel, &ClickableLabel::clicked, this, &LauncherMainWindow::forceKfxUpdateCheck);

    // Replace version label with clickable one
    ui->versionLabel->parentWidget()->layout()->replaceWidget(ui->versionLabel, clickableVersionLabel);
    ui->versionLabel->deleteLater();
    ui->versionLabel = clickableVersionLabel;

    // Load animated loading spinner GIF
    QMovie* movie = new QMovie(":/res/img/spinner.gif");
    if (movie->isValid()) {
        // Load GIF into the placeholder label
        ui->spinnerLabel->setMovie(movie);
        movie->start();
    } else {
        // Fallback to text
        ui->spinnerLabel->setText(tr("Loading...", "Fallback text label"));
    }

    // Clear version update spinner label
    ui->versionUpdateSpinnerLabel->setText("");

    // Show the loading spinner during startup
    showLoadingSpinner();

    // Load the latest workshop items and news from the website
    // We do this a bit early for performance reasons which
    // also makes it so everything is already loaded if the user is installing
    loadLatestFromKfxNet();

    // Create a refresh shortcut (F5) for refreshing the main panel
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, &LauncherMainWindow::loadLatestFromKfxNet);

    // Move window to main screen
    QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty() == false) {
        int screenIndex = 0;

        // Set launcher to same screen as game
        if (Settings::getLauncherSetting("OPEN_ON_GAME_SCREEN").toBool() == true) {
            screenIndex = Settings::getKfxSetting("DISPLAY_NUMBER").toInt() - 1;
            if (screenIndex >= screens.size()) {
                screenIndex = 0;
            }
        }

        this->setScreen(screens[screenIndex]);
    }

    // Set in middle of screen if not on Wayland
    if (QGuiApplication::platformName() != "wayland") {
        QRect geometry = this->screen()->geometry();
        this->move(
            // We use left() and top() here because the position is absolute and not relative to the screen
            geometry.left() + ((geometry.width() - this->width()) / 2),
            geometry.top() + ((geometry.height() - this->height()) / 2) - 50); // minus 50 to put it a bit higher
    }

    // Let user know if this launcher is a prototype
    if(LAUNCHER_IS_PROTOTYPE){
        QMessageBox::information(this,
            tr("KeeperFX Launcher", "MessageBox Title"),
            tr("This launcher version is a prototype and should only be used for testing.", "MessageBox Text"));
    }

    // Check if an original DK executable is found
    // This should not be the case and probably means the user installed KeeperFX into
    // the Dungeon Keeper directory which might cause problems.
    // It's also possible for the user to surpress the messagebox.
    if (DkFiles::isOriginalDkExecutableFound() && Settings::getLauncherSetting("SUPPRESS_ORIGINAL_DK_FOUND_MESSAGEBOX").toBool() == false) {
        qDebug() << "Original DK executable(s) found: Asking if user is wants to ignore or abort";

        // Create messagebox
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("KeeperFX Error", "MessageBox Title"));
        msgBox.setText(tr("The launcher has detected that it might have been installed in the original Dungeon Keeper directory. "
                          "There is a chance you will encounter issues and will be unable to play the game."
                          "\n\n"
                          "KeeperFX should be installed in its own directory as it is a standalone game.",
                          "MessageBox Text"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ignore | QMessageBox::Abort);
        msgBox.setDefaultButton(QMessageBox::Abort);

        // Add "Don't show this again" checkbox
        QCheckBox dontShowAgain(tr("Don't show this again", "Messagebox Checkbox label"));
        dontShowAgain.setStyleSheet("margin-top: 12px;");
        msgBox.setCheckBox(&dontShowAgain);

        // Show the dialog and get the result
        int result = msgBox.exec();

        // Handle the result
        if (result == QMessageBox::Abort) {
            qDebug() << "Original DK executable(s) found: User chose to abort";

            // Exit the application
            this->hide();
            QTimer::singleShot(0, []() { QCoreApplication::exit(1); });
            return;
        } else {
            qDebug() << "Original DK executable(s) found: User chose to ignore";

            // Check if user wants to not show the dialog again
            if (dontShowAgain.isChecked()) {
                Settings::setLauncherSetting("SUPPRESS_ORIGINAL_DK_FOUND_MESSAGEBOX", true);
            }
        }
    }

    // Autostart music download procedure when '--download-music' launcher option is set
    // We do this before the --install one because the installer might already move them over
    if (LauncherOptions::isSet("download-music")) {
        DownloadMusicDialog downloadMusicDialog(this);
        downloadMusicDialog.exec();

        // Remove --download-music from parameter list because the user might trigger a launcher restart after installation
        LauncherOptions::removeArgumentOption("download-music");
    }

    // Autostart install procedure when '--install' launcher option is set
    if(LauncherOptions::isSet("install")){
        InstallKfxDialog installKfxDialog(this);
        installKfxDialog.exec();

        // Remove --install from parameter list because the user might trigger a launcher restart after installation
        LauncherOptions::removeArgumentOption("install");

    } else {
        // If '--install' is not forced we check if we need to install
        if (Helper::isKeeperFxInstalled() == false) {
            qDebug() << "'keeperfx.exe' seems to be missing, asking if user wants a fresh install";
            if (askForKeeperFxInstall() == true) {
                qDebug() << "User wants fresh install, opening kfx install dialog";
                InstallKfxDialog installKfxDialog(this);
                installKfxDialog.exec();
            }
        }
    }

    // Check if we need to copy over DK files
    // Only do this if keeperfx is installed
    if (Helper::isKeeperFxInstalled() == true && DkFiles::isCurrentAppDirValidDkDir() == false) {
        qDebug() << "One or more original DK files not found, opening copy dialog";
        CopyDkFilesDialog copyDkFilesWindow(this);
        copyDkFilesWindow.exec();
    }

    // Load keeperfx version if keeperfx is installed
    if (Helper::isKeeperFxInstalled()) {
        if (KfxVersion::loadCurrentVersion() == true) {
            // Refresh GUI
            refreshKfxVersionInGui();

        } else {
            // Failed to get KeeperFX version
            // Asking the user if they want to reinstall
            qDebug() << "Failed to load KeeperFX version";
            int result = QMessageBox::question(this,
                                               tr("KeeperFX Error", "MessageBox Title"),
                                               tr("The launcher failed to determine the version of KeeperFX. "
                                                  "Your installation may be corrupted or outdated.\n\n"
                                                  "Would you like to reinstall KeeperFX?",
                                                  "MessageBox Text"));

            if (result == QMessageBox::Yes) {
                // Start Automatic KeeperFX (web) installation
                qDebug() << "User wants to reinstall KeeperFX";
                InstallKfxDialog installKfxDialog(this);
                installKfxDialog.exec();

                // Try and get the version again
                if (KfxVersion::loadCurrentVersion() == true) {
                    // Refresh GUI
                    refreshKfxVersionInGui();

                } else {
                    // Still an error even after reinstalling KeeperFX
                    // We can't fix this so we'll tell the user to report it
                    QMessageBox::warning(this,
                                         tr("KeeperFX Error", "MessageBox Title"),
                                         tr("The launcher failed to grab the version of KeeperFX. "
                                            "Please report this error to the KeeperFX team.",
                                            "MessageBox Text"));
                }
            }
        }
    }

    // Load the extra menu for the button next to the play button
    setupPlayExtraMenu();

    // Handle buttons that should be aware of the current installation
    // This will enable/disable specific buttons whether the logfile and the KFX binary exist
    refreshInstallationAwareButtons();
    refreshLogfileButton();

    // Start checking periodically if the logfile exists
    // This continious checking is not required but it's an interesting little gimmick
    QTimer *logfileButtonRefreshTimer = new QTimer();
    connect(logfileButtonRefreshTimer, &QTimer::timeout, this, &LauncherMainWindow::refreshLogfileButton);
    logfileButtonRefreshTimer->start(2500);

    // Verify the binaries against known certificates
    verifyBinaryCertificates();

    // Automatically check for updates if that's enabled
    if (Settings::getLauncherSetting("CHECK_FOR_UPDATES_ENABLED") == true) {
        checkForKfxUpdate();
    } else {
        // Check if there are any files that should be removed
        // We do this if we're not checking for updates because when checking for updates we do this as well
        checkForFileRemoval();
    }
}

LauncherMainWindow::~LauncherMainWindow()
{
    delete ui;
}

void LauncherMainWindow::showLoadingSpinner()
{
    // Hide the elements in the main panel view
    ui->KfxNewsList->hide();
    ui->KfxWorkshopItemList->hide();
    ui->latestWorkshopItemsLabel->hide();
    ui->latestNewsLabel->hide();
    ui->LogoWidget->hide();

    // Show the spinner
    // Because all the rest is hidden this will nicely show in the middle
    ui->spinnerLabel->show();
}

void LauncherMainWindow::hideLoadingSpinner(bool showOnlineContent)
{
    // Hide the spinner
    ui->spinnerLabel->hide();

    if(showOnlineContent){

        // Show workshop items if there are some
        if(ui->KfxWorkshopItemList->children().length() > 0){
            ui->latestWorkshopItemsLabel->show();
            ui->KfxWorkshopItemList->show();
        }

        // Show news articles if there are some
        if(ui->KfxNewsList->children().length() > 0){
            ui->latestNewsLabel->show();
            ui->KfxNewsList->show();
        }
    }
    else
    {
        ui->LogoWidget->show();
    }
}

void LauncherMainWindow::setupPlayExtraMenu()
{
    QMenu *menu = new QMenu(this);

    // Play map action
    // TODO: disabled until implemented
    /*menu->addAction(tr("Play map"),
                    [this]() {
                        // Handle play map logic here
                        qDebug() << "Play map selected!";
                    })
        ->setDisabled(true); // TODO: disabled until implemented*/

    // Play campaign action
    if (KfxVersion::hasFunctionality("start_campaign_directly") == true) {
        this->campaignMenu = menu->addMenu(tr("Play campaign", "Menu"));
        menu->addMenu(campaignMenu);
        refreshCampaignMenu();
    }

    // Add 'Load game'
    // We store this menu in the main window so we can reload the saves when the game ends
    if (KfxVersion::hasFunctionality("load_save_directly") == true) {
        this->saveFilesMenu = menu->addMenu(tr("Load save game", "Menu"));
        menu->addMenu(saveFilesMenu);
        refreshSaveFilesMenu();
    }

    // Direct connect (MP) action
    if (KfxVersion::hasFunctionality("direct_enet_connect") == true) {
        menu->addAction(tr("Direct connect (MP)", "Menu"), [this]() {
            qDebug() << "Direct connect (MP) selected!";
            // Open the dialog
            DirectConnectDialog dialog(this);
            if (dialog.exec() == QDialog::Accepted) {
                startGame(Game::StartType::DIRECT_CONNECT, dialog.getIp(), dialog.getPort());
            }
        });
    }

    // Scan local network (MP)
    menu->addAction(tr("Scan local network (MP)", "Menu"), [this]() {
        qDebug() << "Scan local network (MP) selected!";
        // Open the scan dialog
        ScanNetworkDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            // Start the game
            startGame(Game::StartType::DIRECT_CONNECT, dialog.getIp(), dialog.getPort());
        }
    });

    // Scan local network (MP)
    menu->addAction(tr("Test internet lobby (MP)", "Menu"), [this]() {
        qDebug() << "Test internet lobby (MP) selected!";
        // Open the dialog
        EnetServerTestDialog dialog(this);
        dialog.exec();
    });

    // Run packetsave action
    menu->addAction(tr("Run packetfile", "Menu"), [this]() {
        qDebug() << "Run packetsave selected!";
        // Open the dialog
        RunPacketFileDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            // Start the game
            startGame(Game::StartType::LOAD_PACKETSAVE, dialog.getPacketFileName());
        }
    });

    // Start without mods action
    if (KfxVersion::hasFunctionality("start_without_mods_param") == true) {
        menu->addAction(tr("Start without mods", "Menu"), [this]() {
            qDebug() << "Start without mods selected!";
            // Start the game
            startGame(Game::StartType::START_WITHOUT_MODS);
        });
    }

    // Heavylog toggle
    QFile heavyLogBin(QCoreApplication::applicationDirPath() + "/keeperfx_hvlog.exe");
    if (heavyLogBin.exists()) {
        // Create menu item
        QAction *heavyLogAction = menu->addAction(tr("Heavylog", "Menu"));
        heavyLogAction->setCheckable(true);
        // Store if heavylog is enabled
        bool heavyLogEnabled = Settings::getLauncherSetting("GAME_HEAVY_LOG_ENABLED").toBool();
        // Set toggle state of menu item
        heavyLogAction->setChecked(heavyLogEnabled);
        // Update play button
        ui->playButton->setText(heavyLogEnabled ? "  " + tr("Play (hvlog)", "Button text") : tr("Play", "Button text"));
        // Connect heavylog toggle
        connect(heavyLogAction, &QAction::triggered, this, [this, heavyLogAction]() {
            bool enabled = heavyLogAction->isChecked();
            ui->playButton->setText(enabled ? "  " + tr("Play (hvlog)", "Button text") : tr("Play", "Button text"));
            // Update settings to remember this
            Settings::setLauncherSetting("GAME_HEAVY_LOG_ENABLED", enabled);
        });
    }

    // Attach the menu to the button
    ui->playExtraButton->setMenu(menu);
}

void LauncherMainWindow::refreshSaveFilesMenu()
{
    if (KfxVersion::hasFunctionality("load_save_directly") == false) {
        return;
    }

    // Clear any old loaded saves
    this->saveFilesMenu->clear();

    // Add saves to 'Load game'
    QList<SaveFile *> saveFileList = SaveFile::getAll();
    if (saveFileList.empty()) {
        this->saveFilesMenu->setDisabled(true);
    } else {
        this->saveFilesMenu->setDisabled(false);
        for (auto &saveFile : saveFileList) {
            this->saveFilesMenu->addAction(saveFile->toString(), [this, saveFile]() {
                // Handle loading the save file
                qDebug() << "Loading save file:" << saveFile;
                // TODO: startGame(Game::StartType::LOAD_SAVE, saveFile->saveName);
            });
        }
    }
}

void LauncherMainWindow::refreshCampaignMenu()
{
    if (KfxVersion::hasFunctionality("start_campaign_directly") == false) {
        return;
    }

    // Clear any old loaded campaigns in the list
    this->campaignMenu->clear();

    // Add campaigns to 'Start campaign'
    QList<Campaign *> campaignList = Campaign::getAll();
    if (campaignList.empty()) {
        this->campaignMenu->setDisabled(true);
    } else {
        this->campaignMenu->setDisabled(false);
        for (auto &campaign : campaignList) {
            this->campaignMenu->addAction(campaign->toString(), [this, campaign]() {
                // Start campaign
                qDebug() << "Starting campaign:" << campaign->toString();
                startGame(Game::StartType::CAMPAIGN, campaign->campaignShortName);
            });
        }
    }
}

void LauncherMainWindow::refreshInstallationAwareButtons() {
    bool isKfxInstalled = Helper::isKeeperFxInstalled();
    ui->settingsButton->setDisabled(isKfxInstalled == false);
    ui->playButton->setDisabled(isKfxInstalled == false);
    ui->playExtraButton->setDisabled(isKfxInstalled == false);

    // Play button theme
    QString playButtonTheme = Settings::getLauncherSetting("PLAY_BUTTON_THEME").toString();
    if (playButtonTheme == "qt-fusion-dark") {
        ui->playButton->setStyleSheet("");
        ui->playExtraButton->setStyleSheet("QPushButton::menu-indicator{image:none;width:0px;height:0px;}");
    } else {
        QFile styleSheetFile(QString(":/theme/res/play-button-theme/%1.css").arg(playButtonTheme));
        if (styleSheetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString styleSheetString = QString::fromUtf8(styleSheetFile.readAll());
            if (styleSheetString.isEmpty() == false) {
                ui->playButton->setStyleSheet(styleSheetString);
                ui->playExtraButton->setStyleSheet(styleSheetString);
            }
        } else {
            qWarning() << "Failed to load play button theme stylesheet:" << styleSheetFile.fileName();
        }
    }

    // Unearth button visibility
    QFile unearthBinary = Helper::getUnearthBinary();
    ui->unearthButton->setVisible(unearthBinary.exists());

    // Mods button visibility
    //ui->modsButton->setVisible(ModManager::isModsFunctionalityAvailable());
    ui->modsButton->setVisible(false);
}

void LauncherMainWindow::refreshLogfileButton() {
    QFile logFile(QCoreApplication::applicationDirPath() + "/keeperfx.log");
    ui->logFileButton->setDisabled(logFile.exists() == false);
}

void LauncherMainWindow::on_logFileButton_clicked()
{
    // Use default text editor to open keeperfx.log file
    QString logFilePath = QCoreApplication::applicationDirPath() + "/keeperfx.log";
    QFile logFile(logFilePath);
    if (logFile.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(logFilePath));
    } else {
        qWarning() << "File does not exist: " << logFilePath;
    }
}

void LauncherMainWindow::on_workshopButton_clicked()
{
    // Use default browser to open the workshop URL
    QUrl url("https://keeperfx.net/workshop");
    QDesktopServices::openUrl(url);
}

bool LauncherMainWindow::askForKeeperFxInstall()
{
    // Ask if user wants to install KeeperFX
    int result = QMessageBox::question(this, "KeeperFX", tr("The launcher is unable to find 'keeperfx.exe'.\n\nDo you want to download and install KeeperFX?", "MessageBox Text"));

    // Check if user declines
    if(result != QMessageBox::Yes){

        // Ask if they are sure
        result = QMessageBox::question(this, tr("Confirmation", "MessageBox Title"), tr("Are you sure?\n\nYou will be unable to play KeeperFX.", "MessageBox Text"));
        if(result == QMessageBox::Yes){

            // User does not want to install KeeperFX
            return false;
        }
    }

    // User wants to install KeeperFX
    return true;
}

void LauncherMainWindow::on_settingsButton_clicked() {

    // Remember
    QString oldReleaseVersion = Settings::getLauncherSetting("CHECK_FOR_UPDATES_RELEASE").toString();
    bool oldUpdateCheckEnabled = Settings::getLauncherSetting("CHECK_FOR_UPDATES_ENABLED").toBool();
    QString oldLauncherLanguage = Settings::getLauncherSetting("LAUNCHER_LANGUAGE").toString();
    bool websiteIntegration = Settings::getLauncherSetting("WEBSITE_INTEGRATION_ENABLED").toBool();

    // Open settings dialog
    SettingsDialog settingsDialog(this);
    settingsDialog.exec();

    // Refresh buttons and version UI
    refreshInstallationAwareButtons();
    refreshKfxVersionInGui(); // This also updates the window title

    // Check if launcher language has changed
    if(oldLauncherLanguage != Settings::getLauncherSetting("LAUNCHER_LANGUAGE").toString()){

        // Ask user if they want to restart their launcher
        qDebug() << "Launcher language has changed so asking for launcher restart";
        int result = QMessageBox::question(this,
                                           tr("Language has changed", "MessageBox Title"),
                                           tr("The launcher has to restart to change its language. Do you want to do that now?", "MessageBox Text"));
        if (result == QMessageBox::Yes) {

            qDebug() << "Restarting launcher to change the language";

            // Remove possible --language parameter because it would be weird if it were still active after changing the language
            LauncherOptions::removeArgumentOption("language");

            // Hide this launcher's window and pipe the exit code from the new launcher process to the exit of this one
            // We do this because we want to allow users to keep a handle on the original process
            this->hide();
            QCoreApplication::exit(QProcess::execute(QCoreApplication::applicationFilePath(), LauncherOptions::getArguments()));
            return;
        }
    }

    // Check for updates when settings are closed
    if(Settings::getLauncherSetting("CHECK_FOR_UPDATES_ENABLED").toBool() == true && (
            // Check for updates was disabled and has been enabled now
            oldUpdateCheckEnabled == false ||
            // Version has changed
            oldReleaseVersion != Settings::getLauncherSetting("CHECK_FOR_UPDATES_RELEASE").toString()
    )){
        qDebug() << "Settings regarding updates have been enabled or changed so asking for update";
        checkForKfxUpdate(true);
    }

    // Website integration
    if (websiteIntegration != Settings::getLauncherSetting("WEBSITE_INTEGRATION_ENABLED").toBool()) {
        showLoadingSpinner(); // This also hides everything
        loadLatestFromKfxNet();
    }
}

void LauncherMainWindow::clearLatestFromKfxNet()
{
    // Delete all news articles and labels within the parent widget.
    // We don't delete all childeren in the main window because the layout is part of it too.
    // We also don't use "delete later" because we check if the list is empty afterwards.

    for (NewsArticleWidget *widget : ui->KfxNewsList->findChildren<NewsArticleWidget*>()) {
        delete widget;
    }
    for (WorkshopItemWidget *widget : ui->KfxWorkshopItemList->findChildren<WorkshopItemWidget*>()) {
        delete widget;
    }
    for (QLabel *label : ui->KfxNewsList->findChildren<QLabel*>()) {
        delete label;
    }
    for (QLabel *label : ui->KfxWorkshopItemList->findChildren<QLabel*>()) {
        delete label;
    }
}

void LauncherMainWindow::loadLatestFromKfxNet()
{
    // Check if website integration is disabled
    if (Settings::getLauncherSetting("WEBSITE_INTEGRATION_ENABLED") == false) {
        hideLoadingSpinner(false);
        return;
    }

    // Make sure we're not already loading
    if (isLoadingLatestFromKfxNet) {
        return;
    }

    // Remember that we are loading
    isLoadingLatestFromKfxNet = true;

    // Make sure the loading spinner is shown
    showLoadingSpinner();

    // Clear the existing data in the lists
    clearLatestFromKfxNet();

    // Create thread so we don't block the main thread
    QThread::create([this]() {
        // Get latest workshop items and news from the website
        // We use QtConcurrent so they are loaded in separate threads
        auto fetchWorkshopItems = QtConcurrent::run([this]() { return ApiClient::getJsonResponse(QUrl("/v1/workshop/latest")); });
        auto fetchLatestNews = QtConcurrent::run([this]() { return ApiClient::getJsonResponse(QUrl("/v1/news/latest")); });

        // Wait for both threads to finish
        fetchWorkshopItems.waitForFinished();
        fetchLatestNews.waitForFinished();

        // Emit a signal to pass the data to the main thread
        QMetaObject::invokeMethod(
            this, [this, fetchWorkshopItems, fetchLatestNews]() { emit kfxNetRetrieval(fetchWorkshopItems.result(), fetchLatestNews.result()); }, Qt::QueuedConnection);
        
    })->start();
}

void LauncherMainWindow::onKfxNetRetrieval(QJsonDocument workshopItems, QJsonDocument latestNews)
{
    // Create a thread so we don't lock the main thread while grabbing the images
    QThread::create([this, workshopItems, latestNews]() {
        QMutex mapMutex;
        QThreadPool *threadPool = QThreadPool::globalInstance();

        QList<QJsonObject> workshopItemList;
        QList<QJsonObject> newsArticleList;

        QMap<QString, QPixmap> pixmapMap;

        QSize workshopItemThumbnailSize(80, 80);
        QSize newsArticleThumbnailSize(100, 100);

        if (workshopItems.isEmpty() == false) {
            QJsonObject workshopItemsObj = workshopItems.object();
            QJsonArray workshopItemsArray = workshopItemsObj["workshop_items"].toArray();

            // Loop trough workshop items
            int currentLoopCount = 0;
            for (int i = 0; i < workshopItemsArray.size(); ++i) {

                const QJsonValue workshopItemValue = workshopItemsArray[i];

                // Only allow the max amount of items
                if (currentLoopCount++ >= MAX_WORKSHOP_ITEMS_SHOWN) {
                    break;
                }

                // Get workshop item as object
                QJsonObject workshopItem = workshopItemValue.toObject();
                workshopItemList.append(workshopItem);

                // Get thumbnail URL
                QString thumbnailUrl;
                if (workshopItem["thumbnail"].isNull() == false) {
                    thumbnailUrl = workshopItem["thumbnail"].toString();
                } else {
                    // The API returns a default image for items without one so we can just use it
                    thumbnailUrl = workshopItem["image"].toString();
                }

                // Start a thread to get the pixmap and convert it to an image
                threadPool->start([thumbnailUrl, workshopItemThumbnailSize, &pixmapMap, &mapMutex]() {
                    QPixmap pixmap = ImageHelper::getOnlineScaledPixmap(QUrl(thumbnailUrl), workshopItemThumbnailSize);

                    // Use a mutex to protect shared access to the QMap
                    QMutexLocker locker(&mapMutex);
                    pixmapMap[thumbnailUrl] = pixmap;
                });
            }
        }

        if (latestNews.isEmpty() == false) {
            QJsonObject latestNewsArticleObj = latestNews.object();
            QJsonArray newsArticlesArray = latestNewsArticleObj["articles"].toArray();

            // Loop trough news articles
            int currentLoopCount = 0;
            for (int i = 0; i < newsArticlesArray.size(); ++i) {

                const QJsonValue newsArticleValue = newsArticlesArray[i];

                // Only allow the max amount of items
                if (currentLoopCount++ >= MAX_NEWS_ARTICLES_SHOWN) {
                    break;
                }

                // Get workshop item as object
                QJsonObject newsArticle = newsArticleValue.toObject();
                newsArticleList.append(newsArticle);

                // Get thumbnail URL
                QString thumbnailUrl = newsArticle["image"].toString();

                // Start a thread to get the pixmap and convert it to an image
                threadPool->start([thumbnailUrl, newsArticleThumbnailSize, &pixmapMap, &mapMutex]() {
                    QPixmap pixmap = ImageHelper::getOnlineScaledPixmap(QUrl(thumbnailUrl), newsArticleThumbnailSize);

                    // Use a mutex to protect shared access to the QMap
                    QMutexLocker locker(&mapMutex);
                    pixmapMap[thumbnailUrl] = pixmap;
                });
            }
        }

        // Wait for all tasks to finish
        threadPool->waitForDone();

        // Emit signal so we can update the GUI
        QMetaObject::invokeMethod(
            this, [this, workshopItemList, newsArticleList, pixmapMap]() { emit kfxNetImagesLoaded(workshopItemList, newsArticleList, pixmapMap); }, Qt::QueuedConnection);
    })->start();
}

void LauncherMainWindow::onKfxNetImagesLoaded(QList<QJsonObject> workshopItemList, QList<QJsonObject> newsArticleList, QMap<QString, QPixmap> pixmapMap)
{
    // Create widget lists
    QList<QWidget *> workshopItemWidgets;
    QList<QWidget *> newsArticleWidgets;

    if (workshopItemList.isEmpty() == false) {
        for (const QJsonObject &workshopItem : workshopItemList) {
            // Create the widget
            WorkshopItemWidget *workshopItemWidget = new WorkshopItemWidget(this);
            workshopItemWidget->setTitle(workshopItem["name"].toString());
            workshopItemWidget->setType(workshopItem["category"].toString());
            workshopItemWidget->setDate(workshopItem["created_timestamp"].toString());
            workshopItemWidget->setAuthor(workshopItem["submitter"].toObject()["username"].toString());
            workshopItemWidget->setTargetUrl(workshopItem["url"].toString());

            // Set the size of the widget
            // TODO: make this dynamic (as it doesn't look good with a bigger window)
            workshopItemWidget->setMaximumWidth(250);
            workshopItemWidget->setMaximumHeight(110);

            // Set image
            if (workshopItem["thumbnail"].isNull() == false) {
                workshopItemWidget->setImagePixmap(pixmapMap[workshopItem["thumbnail"].toString()]);
            } else {
                workshopItemWidget->setImagePixmap(pixmapMap[workshopItem["image"].toString()]);
            }

            // Add the widget to the list
            workshopItemWidgets.append(workshopItemWidget);
        }
    }

    if (newsArticleList.isEmpty() == false) {
        for (const QJsonObject &newsArticle : newsArticleList) {
            // Create the widget
            NewsArticleWidget *newsArticleWidget = new NewsArticleWidget(this);
            newsArticleWidget->setTitle(newsArticle["title"].toString());
            newsArticleWidget->setDate(newsArticle["created_timestamp"].toString());
            newsArticleWidget->setTargetUrl(newsArticle["url"].toString());

            if (newsArticle["excerpt"].isNull() == false) {
                newsArticleWidget->setExcerpt(newsArticle["excerpt"].toString());
            } else {
                newsArticleWidget->setExcerpt("");
            }

            // Set the size of the widget
            // TODO: make this dynamic (as it doesn't look good with a bigger window)
            newsArticleWidget->setMaximumWidth(510);
            newsArticleWidget->setMaximumHeight(110);

            // The API returns a default image for items without one so we can just pass it
            newsArticleWidget->setImagePixmap(pixmapMap[newsArticle["image"].toString()]);

            // Add the widget to the list
            newsArticleWidgets.append(newsArticleWidget);
        }
    }

    // Disable updating UI widgets while adding child widgets
    ui->KfxWorkshopItemList->setUpdatesEnabled(false);
    ui->KfxNewsList->setUpdatesEnabled(false);

    // Add widgets
    for (QWidget *widget : workshopItemWidgets) {
        ui->KfxWorkshopItemList->layout()->addWidget(widget);
    }

    for (QWidget *widget : newsArticleWidgets) {
        ui->KfxNewsList->layout()->addWidget(widget);
    }

    // Enable updating UI widgets after adding child widgets
    ui->KfxWorkshopItemList->setUpdatesEnabled(true);
    ui->KfxNewsList->setUpdatesEnabled(true);

    this->hideLoadingSpinner(true);
    isLoadingLatestFromKfxNet = false;
}

void LauncherMainWindow::checkForFileRemoval()
{
    // Check if we need to skip leftover file removal
    if (LauncherOptions::isSet("skip-file-removal") == true) {
        qDebug() << "Skipping leftover file removal (skip-file-removal)";
        return;
    }

    // Remove 'save/deleteme.txt'
    // We remove it manually because this will almost always be present after an install (or update)
    QFile saveDeleteMeFile(QCoreApplication::applicationDirPath() + "/save/deleteme.txt");
    if (saveDeleteMeFile.exists()) {
        saveDeleteMeFile.remove();
    }

    // Spawn a thread for the file removal
    // We do this in a thread so we can already show the launcher main window in the meanwhile
    QThread::create([this]() {

        // Check if file exists that contains files that should be removed
        QString fileRemovalFilename = QString("launcher-auto-file-removal.txt");
        QFile fileRemovalFile(QCoreApplication::applicationDirPath() + "/" + fileRemovalFilename);
        if (fileRemovalFile.exists()) {

            // Get files to remove based on KfxVersion
            QStringList filesToRemove = FileRemover::processFile(fileRemovalFile, KfxVersion::currentVersion.version);

            // If there are files that should be removed
            if(filesToRemove.length() > 0){
                qDebug() << "Files found that should be removed:" << filesToRemove;

                // Emit signal for file removal
                emit this->filesToRemoveFound(filesToRemove);

            } else {
                qDebug() << "No files to remove found.";
            }
        } else {
            qInfo() << "File-removal file not found:" << fileRemovalFilename;
        }

    })->start();
}

void LauncherMainWindow::onFilesToRemoveFound(QStringList filesToRemove)
{
    // Workaround to make sure main launcher window is visible
    if (!this->isVisible()) {
        QTimer::singleShot(100, this, [this, filesToRemove]() {
            emit this->filesToRemoveFound(filesToRemove);
        });
        return;
    }

    // Check if user wants to remove leftover files automatically (silently)
    if(Settings::getLauncherSetting("AUTO_REMOVE_LEFTOVER_FILES") == true){
        qDebug() << "Removing leftover files automatically without user interaction";

        // Loop through the list of files
        for (const QString &filePath : std::as_const(filesToRemove)) {

            // Get the file
            QFile file(QCoreApplication::applicationDirPath() + "/" + filePath);
            if (file.exists()) {

                // Remove the file
                if (file.remove()) {
                    qDebug() << "Removed leftover file:" << filePath;
                } else {
                    // There isn't really a need to tell the user here
                    // If they get into trouble the logs will tell us there's a problem here
                    qWarning() << "Failed to remove leftover file:" << filePath;
                }
            }
        }
        return;
    }

    // Show file removal dialog
    FileRemoverDialog fileRemoverDialog(this, filesToRemove);
    fileRemoverDialog.exec();
}

void LauncherMainWindow::onUpdateFound(KfxVersion::VersionInfo versionInfo)
{
    // Hide update icon
    this->onShowUpdateIcon(false);

    // Start updater
    bool autoUpdate = Settings::getLauncherSetting("AUTO_UPDATE") == true;
    UpdateDialog updateDialog(this, versionInfo, autoUpdate);
    updateDialog.exec();

    // Reload current version
    if (KfxVersion::loadCurrentVersion() == true) {
        refreshKfxVersionInGui();
    }

    // Next we'll check for a possible updated launcher
    // On Windows we can query the file info of a new launcher binary
    // But on Linux we just check if a new launcher binary exists
    // TODO: figure out if we can check the launcher version on linux?

#ifdef Q_OS_WINDOWS

    // Windows binary
    QString newAppBinString(QCoreApplication::applicationDirPath() + "/keeperfx-launcher-qt-new.exe");

    // Check if a new launcher binary exists
    QFile newAppBin(newAppBinString);
    if (newAppBin.exists()) {

        // Get the version of the new launcher
        QString newLauncherVersion = KfxVersion::getVersionString(newAppBin);
        if(newLauncherVersion.isEmpty()){

            qDebug() << "A new launcher binary was found but we failed to grab its version";
            qDebug() << "We'll start it just in case";

            // Start the new launcher
            // This needs to be detached because we are going the remove the current running launcher
            QProcess::startDetached(newAppBinString, LauncherOptions::getArguments());
            QApplication::quit();
            return;

        } else {

            qDebug() << "New launcher found:" << newAppBinString << QString("v" + newLauncherVersion);
            qDebug() << "Current launcher version:" << LAUNCHER_VERSION;

            // Check if new launcher is newer
            if(KfxVersion::isNewerVersion(newLauncherVersion, LAUNCHER_VERSION)){

                qDebug() << "Starting new launcher because it is newer";
                // Start the new launcher
                // This needs to be detached because we are going the remove the current running launcher
                QProcess::startDetached(newAppBinString, LauncherOptions::getArguments());
                QApplication::quit();
                return;

            } else {

                // The new launcher can be removed because it is not newer than our one
                newAppBin.remove();
                qDebug() << "New launcher is removed because it is older";
            }
        }
    }

#else
    // Linux binary
    QString newAppBinString(QCoreApplication::applicationDirPath() + "/keeperfx-launcher-qt-new");

    // Check if a new launcher binary exists
    QFile newAppBin(newAppBinString);
    if (newAppBin.exists()) {

        qDebug() << "New launcher found. Starting it now";

        // Start the new launcher
        // This needs to be detached because we are going the remove the current running launcher
        QProcess::startDetached(newAppBinString, LauncherOptions::getArguments());
        QApplication::quit();
        return;
    }

#endif

    // Verify the binaries against known certificates
    verifyBinaryCertificates();

    // Check if there are any files that should be removed
    checkForFileRemoval();

    // Refresh the installation-aware buttons
    // It's possible an update is done that fixes the installation and adds the binary again
    refreshInstallationAwareButtons();
}

void LauncherMainWindow::onShowUpdateIcon(bool show)
{
    if(show == false){
        if (QMovie* movie = ui->versionUpdateSpinnerLabel->movie()) {
            movie->stop();
            movie->deleteLater();
        }
        ui->versionUpdateSpinnerLabel->clear();
        return;
    }

    // Load animated loading spinner GIF
    QMovie* movie = new QMovie(":/res/img/spinner.gif");
    movie->setScaledSize(QSize(16, 16));
    if (movie->isValid()) {
        // Load GIF into the placeholder label
        ui->versionUpdateSpinnerLabel->setMovie(movie);
        movie->start();
    } else {
        // Fallback to text
        ui->versionUpdateSpinnerLabel->setText("...");
    }
}

// Wrapper function because we can't use the 'ignoreInterval' parameter in slots
void LauncherMainWindow::forceKfxUpdateCheck() {
    this->checkForKfxUpdate(true);
}

void LauncherMainWindow::checkForKfxUpdate(bool ignoreInterval)
{
    qDebug() << "Checking for KeeperFX update";

    // Only update from stable and alpha
    if (KfxVersion::currentVersion.type != KfxVersion::ReleaseType::STABLE &&
        KfxVersion::currentVersion.type != KfxVersion::ReleaseType::ALPHA) {
        qDebug() << "Not updating because we are not on stable or alpha version";
        checkForFileRemoval(); // Check if there are any files that should be removed
        return;
    }

    // Get the current timestamp
    QDateTime currentTimestamp = QDateTime::currentDateTimeUtc();

    // Check if we need to handle the interval
    // This depends on whether it's an automatic or manual check
    if(ignoreInterval == false){

        // Get interval and check if we need to use intervals
        // Anything below 1 means we'll not use an interval for checking
        int intervalDays = Settings::getLauncherSetting("CHECK_FOR_UPDATES_INTERVAL_DAYS").toInt();
        if(intervalDays > 0){

            // Get last timestamp of update
            QString lastTimestampString = Settings::getLauncherSetting("CHECK_FOR_UPDATES_LAST_TIMESTAMP").toString();
            QDateTime lastTimestamp = QDateTime::fromString(lastTimestampString, Qt::ISODate);

            // Make sure timestamp is valid
            if(lastTimestamp.isValid() == false){
                qWarning() << "Invalid timestamp for 'CHECK_FOR_UPDATES_LAST_TIMESTAMP' launcher setting:" << lastTimestampString;
                return;
            }

            // Check if we need to update
            if(lastTimestamp.addDays(intervalDays) > currentTimestamp) {
                qDebug() << "Not updating because we have not passed the interval for updates yet:" << QString(QString::number(intervalDays) + " day");
                checkForFileRemoval(); // Check if there are any files that should be removed
                return;
            } else {
                qDebug() << "Update interval is passed, checking for update";
            }
        } else {
            qDebug() << "Interval is disabled for this update check";
        }
    } else {
        qDebug() << "Bypassing possible interval for this update check";
    }

    // Remember current timestamp for interval checks
    Settings::setLauncherSetting("CHECK_FOR_UPDATES_LAST_TIMESTAMP", currentTimestamp.toString(Qt::ISODate));

    // Spawn a thread
    // We don't want any slow internet connections block our main thread
    QThread::create([this]() {

        // Get release type
        QString typeString = Settings::getLauncherSetting("CHECK_FOR_UPDATES_RELEASE").toString();
        KfxVersion::ReleaseType type = KfxVersion::getReleaseTypefromString(typeString);

        // Only update to stable and alpha
        if (type != KfxVersion::ReleaseType::STABLE && type != KfxVersion::ReleaseType::ALPHA) {
            qDebug() << "Invalid auto update release type:" << typeString;
            checkForFileRemoval(); // Check if there are any files that should be removed
            return;
        }

        // Show update icon
        emit this->showUpdateIcon(true);

        // Get latest version for this release type
        auto latestVersionInfo = KfxVersion::getLatestVersion(type);
        if (latestVersionInfo) {

            // Check if type of release is different or version is newer
            if (type != KfxVersion::currentVersion.type
                || KfxVersion::isNewerVersion(latestVersionInfo->version,
                                              KfxVersion::currentVersion.version)) {
                qDebug() << "Update found:" << latestVersionInfo->version;

                // Emit signal for update
                emit this->updateFound(latestVersionInfo.value());
                return;

            } else {
                qDebug() << "No updates found";
            }
        }

        // Hide update icon
        emit this->showUpdateIcon(false);

        // Check if there are any files that should be removed
        checkForFileRemoval();

    })->start();
}

void LauncherMainWindow::verifyBinaryCertificates()
{
    // Check if we need to skip verification
    if (LauncherOptions::isSet("skip-verify") == true) {
        qDebug() << "Skipping certificate file verification (skip-verify)";
        return;
    }

    // List of files to check
    QStringList filesToCheck = {
        // "keeperfx.exe", "keeperfx_hvlog.exe",
        // "keeperfx-launcher-qt.exe",
    };

    QStringList failedFiles;

    // Loop through the files we need to verify
    for (const QString &filePath : filesToCheck) {

        // Load file
        QFile file(QApplication::applicationDirPath() + "/" + filePath);
        if (file.exists() == false) {
            continue;
        }

        // Verify
        if (Certificate::verify(file) == false) {
            failedFiles.append(filePath);
        }
    }

    // If any files have been failed to verify
    if (failedFiles.empty() == false) {

        // Create file list
        QString fileListString;
        for (const QString &filePath : failedFiles) {
            fileListString.append(filePath + "\n");
        }

        // Show messagebox alerting the user
        QMessageBox::warning(this,
                             tr("KeeperFX Verification Error", "MessageBox Title"),
                             tr("The launcher failed to verify the signature of:\n\n"
                                "%1"
                                "\n"
                                "It is highly suggested to only use official KeeperFX files.",
                                "MessageBox Text")
                                 .arg(fileListString));
    }
}

void LauncherMainWindow::startGame(Game::StartType startType, QVariant data1, QVariant data2, QVariant data3)
{
    // Disable the play buttons
    ui->playButton->setDisabled(true);
    ui->playExtraButton->setDisabled(true);

    // Start the game
    bool startStatus = game->start(startType, data1, data2, data3);

    // Make sure game is started
    if (startStatus == false) {
        qDebug() << "Game failed to start";

        // Refresh the installation-aware and logfile buttons
        refreshInstallationAwareButtons();
        refreshLogfileButton();

        // Get the error
        QString errorString = game->getErrorString();

        // Show messagebox alerting the user
        if (errorString.isEmpty() == false) {
            qDebug() << "Game start error:" << errorString;
            QMessageBox::warning(this,
                                 tr("KeeperFX Error", "MessageBox Title"),
                                 tr("Failed to start KeeperFX.\n\n"
                                    "Error:\n%1",
                                    "MessageBox Text")
                                     .arg(errorString));
        } else {
            QMessageBox::warning(this, tr("KeeperFX Error", "MessageBox Title"), tr("Failed to start KeeperFX. Unknown error.", "MessageBox Text"));
        }
    }
}

void LauncherMainWindow::on_playButton_clicked()
{
    startGame(Game::StartType::NORMAL);
}

void LauncherMainWindow::onGameEnded(int exitCode, QProcess::ExitStatus exitStatus)
{
    refreshInstallationAwareButtons();
    refreshLogfileButton();
    refreshSaveFilesMenu();

    // Not really required but good to occasionally refresh
    refreshCampaignMenu();
}

void LauncherMainWindow::refreshKfxVersionInGui()
{
    qInfo() << "KeeperFX version:" << KfxVersion::currentVersion.fullString;
    ui->versionLabel->setText("v" + KfxVersion::currentVersion.fullString);

    QString windowTitle = tr("KeeperFX Launcher", "Window Title") + " - v" + KfxVersion::currentVersion.fullString;

    if(Settings::getLauncherSetting("SHOW_DIR_NAME_IN_WINDOW_TITLE") == true){
        windowTitle.prepend("[ " + QDir(QCoreApplication::applicationDirPath()).dirName() + " ] - ");
    }

    this->setWindowTitle(windowTitle);
}

void LauncherMainWindow::on_openFolderButton_clicked()
{
    // Use default file browser to open Application Folder
    QUrl url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QDir::separator());
    QDesktopServices::openUrl(url);
}

void LauncherMainWindow::on_unearthButton_clicked()
{
    // Get file
    QFile unearthBinary = Helper::getUnearthBinary();

    // Make sure it is accessible
    if (unearthBinary.exists() == false) {
        qWarning() << "Unearth Binary can not be started because it does not exist or is not accessible:" << unearthBinary.fileName();
        return;
    }

    qDebug() << "Starting Unearth:" << unearthBinary.fileName();

    // Start the process
    if(QProcess::startDetached(unearthBinary.fileName()) == true){
        qDebug() << "Unearth process started";
        return;
    }

    // At this point the binary would have failed to start
    qWarning() << "Failed to start Unearth";

#ifdef Q_OS_UNIX
        qDebug() << "Unearth binary might not be executable. Trying to set permission (UNIX)";

        if(Helper::makeBinaryExecutable(unearthBinary.fileName()) == false){
            qWarning() << "Failed to update Unearth binary file permissions";
            return;
        }

        qDebug() << "Unearth binary file permissions updated";

        if(QProcess::startDetached(unearthBinary.fileName()) == false){
            qWarning() << "Failed to start Unearth binary even after updating file permissions";
            return;
        }

        qDebug() << "Unearth process started";
#endif
}

void LauncherMainWindow::on_modsButton_clicked()
{
    qDebug() << "Mods button clicked. Opening mods dialog";

    ModManagerDialog modManagerDialog(this);
    modManagerDialog.exec();
}

void LauncherMainWindow::on_discordButton_clicked()
{
    // Use default browser to open Discord invite
    QUrl url("https://discord.gg/WxgE8WZBku");
    QDesktopServices::openUrl(url);
}

void LauncherMainWindow::on_websiteButton_clicked()
{
    // Use default browser to open KeeperFX.net
    QUrl url("https://keeperfx.net");
    QDesktopServices::openUrl(url);

}

