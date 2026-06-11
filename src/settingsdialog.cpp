#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include "version.h"
#include "kfxversion.h"
#include "popupsignalcombobox.h"
#include "settings.h"
#include "launcheroptions.h"
#include "cdn.h"

#include <QDesktopServices>
#include <QEvent>
#include <QFontDatabase>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    // Disable resizing and remove maximize button
    setFixedSize(size());
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    // Hide 'Multiplayer' tab until a future update requires it
    ui->tabWidget->tabBar()->setTabVisible(4, false);

    // Reset setting has changed variable
    settingHasChanged = false;

    // Make sure this widget starts at the first tab
    // When working in Qt's UI editor you can change it to another tab by accident
    ui->tabWidget->setCurrentIndex(0);

    // Setup the 'display monitor' dropdown
    setupDisplayMonitorDropdown();

    // Connect the dialog buttons
    connect(ui->buttonBox->button(QDialogButtonBox::Save),
        &QPushButton::clicked,
        this,
        &SettingsDialog::saveSettings);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel),
        &QPushButton::clicked,
        this,
        &SettingsDialog::cancel);
    connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults),
        &QPushButton::clicked,
        this,
        &SettingsDialog::restoreSettings);

    // Add a "Open config file" button to the buttonbox
    QPushButton *configButton = new QPushButton(tr("Open config file", "Dialog Button"), this);
    configButton->setIcon(QIcon::fromTheme("text-x-generic")); // typical "text file" icon
    ui->buttonBox->addButton(configButton, QDialogButtonBox::HelpRole);
    connect(configButton, &QPushButton::clicked, this, &SettingsDialog::onOpenConfigButtonClicked);

    // Disable Save button when nothing is changed
    ui->buttonBox->button(QDialogButtonBox::Save)->setDisabled(true);

    // Game languages dropdown
    ui->comboBoxLanguage->addItem("English", "ENG");    // English
    ui->comboBoxLanguage->addItem("Deutsch", "GER");    // German
    ui->comboBoxLanguage->addItem("Français", "FRE");   // French
    ui->comboBoxLanguage->addItem("Nederlands", "DUT"); // Dutch
    ui->comboBoxLanguage->addItem("Español", "SPA");    // Spanish
    ui->comboBoxLanguage->addItem("Italiano", "ITA");   // Italian
    ui->comboBoxLanguage->addItem("Polski", "POL");     // Polish
    ui->comboBoxLanguage->addItem("Svenska", "SWE");    // Swedish
    ui->comboBoxLanguage->addItem("Русский", "RUS");    // Russian
    ui->comboBoxLanguage->addItem("Čeština", "CZE");    // Czech
    ui->comboBoxLanguage->addItem("Latīna", "LAT");     // Latin
    ui->comboBoxLanguage->addItem("日本語", "JPN");     // Japanese
    ui->comboBoxLanguage->addItem("한국어", "KOR");     // Korean
    ui->comboBoxLanguage->addItem("简体中文", "CHI");   // Chinese (Simplified)
    ui->comboBoxLanguage->addItem("繁體中文", "CHT");   // Chinese (Traditional)

    // Add Ukrainian Game Language
    // We do this after the list of languages because this one was added later
    if (KfxVersion::hasFunctionality("ukrainian_game_language") == true) {
        ui->comboBoxLanguage->addItem("Українська", "UKR"); // Ukrainian
    }

    // Add Portuguese (Brazilian) Game Language
    // We do this after the list of languages because this one was added later
    if (KfxVersion::hasFunctionality("portuguese_game_language") == true) {
        ui->comboBoxLanguage->addItem("Português (Brasil)", "POR"); // Portuguese (Brazilian)
    }

    sortLanguageComboBox(ui->comboBoxLanguage);

    // Exit on LUA error
    if (KfxVersion::hasFunctionality("exit_on_lua_error") == false) {
        ui->checkBoxExitOnLuaError->setDisabled(true);
    }

    // Flee and Imprison button
    if (KfxVersion::hasFunctionality("flee_imprison_defaults") == false) {
        ui->checkBoxAutoEnableFlee->setDisabled(true);
        ui->checkBoxAutoEnableImprison->setDisabled(true);
    }

    // Max frames per second
    if (KfxVersion::hasFunctionality("max_frames_per_second") == false) {
        ui->labelMaxFps->setDisabled(true);
        ui->lineEditMaxFps->setDisabled(true);
    }

    // Auto determine max fps
    if (KfxVersion::hasFunctionality("auto_determine_monitor_refresh_rate") == false) {
        ui->checkBoxAutoDetermineMaxFps->setDisabled(true);
    }

    if (KfxVersion::hasFunctionality("gui_and_neutral_blink_speed") == false) {
        ui->labelGuiBlinkRate->setDisabled(true);
        ui->lineEditGuiBlinkRate->setDisabled(true);
        ui->labelNeutralFlashRate->setDisabled(true);
        ui->lineEditNeutralFlashRate->setDisabled(true);
    }

    // Tag Mode
    if (KfxVersion::hasFunctionality("tag_mode") == true) {
        // Add default tag mode dropdown options
        ui->comboBoxDefaultTagMode->addItem(tr("Single", "Default Tag Mode Dropdown"), "SINGLE");
        ui->comboBoxDefaultTagMode->addItem(tr("Drag", "Default Tag Mode Dropdown"), "DRAG");
        ui->comboBoxDefaultTagMode->addItem(tr("Remember Last", "Default Tag Mode Dropdown"), "REMEMBER");

        // Make "Remember Last" default tag mode enable tag mode toggle
        connect(ui->comboBoxDefaultTagMode, &QComboBox::currentIndexChanged, this, [this](int index) {
            QString mode = ui->comboBoxDefaultTagMode->itemData(index).toString();
            if (mode == "REMEMBER") {
                ui->checkBoxEnableTagModeToggle->setChecked(true);
            }
        });

        // Make disabling tag mode change "Remember Last" to "Single"
        connect(ui->checkBoxEnableTagModeToggle, &QCheckBox::toggled, this, [this](bool checked) {
            if (!checked && ui->comboBoxDefaultTagMode->currentIndex() == ui->comboBoxDefaultTagMode->findData("REMEMBER")) {
                ui->comboBoxDefaultTagMode->setCurrentIndex(ui->comboBoxDefaultTagMode->findData("SINGLE"));
            }
        });

    } else {
        ui->comboBoxDefaultTagMode->setDisabled(true);
        ui->labelTagMode->setDisabled(true);
        ui->checkBoxEnableTagModeToggle->setDisabled(true);
    }

    // Launcher language dropdown
    ui->comboBoxLauncherLanguage->addItem("English", "en"); // English
    ui->comboBoxLauncherLanguage->addItem("Deutsch", "de");     // German
    ui->comboBoxLauncherLanguage->addItem("Français", "fr");   // French
    ui->comboBoxLauncherLanguage->addItem("Nederlands", "nl"); // Dutch
    ui->comboBoxLauncherLanguage->addItem("Español", "es");    // Spanish
    ui->comboBoxLauncherLanguage->addItem("Português (Brasil)", "pt");      // Portuguese (Brazilian)
    ui->comboBoxLauncherLanguage->addItem("Italiano", "it");    // Italian
    ui->comboBoxLauncherLanguage->addItem("Polski", "pl");      // Polish
    //ui->comboBoxLauncherLanguage->addItem("Svenska", "sv");     // Swedish
    ui->comboBoxLauncherLanguage->addItem("日本語", "ja");       // Japanese
    ui->comboBoxLauncherLanguage->addItem("Русский", "ru");     // Russian
    ui->comboBoxLauncherLanguage->addItem("Українська", "uk");  // Ukrainian
    ui->comboBoxLauncherLanguage->addItem("한국어", "ko");        // Korean
    ui->comboBoxLauncherLanguage->addItem("简体中文", "zh-hans"); // Chinese (Simplified)
    //ui->comboBoxLauncherLanguage->addItem("繁體中文", "zh-hant"); // Chinese (Traditional)
    ui->comboBoxLauncherLanguage->addItem("Čeština", "cs"); // Czech
    //ui->comboBoxLauncherLanguage->addItem("Latīna", "la");      // Latin
    sortLanguageComboBox(ui->comboBoxLauncherLanguage);

    // Launcher play button theme dropdown
    ui->comboBoxPlayButtonTheme->addItem(tr("Qt Fusion Dark", "Play Button Theme Dropdown"), "qt-fusion-dark");
    ui->comboBoxPlayButtonTheme->addItem(tr("DK Orange (default)", "Play Button Theme Dropdown"), "dk-orange");

    // Launcher Download server
    for (auto [key, info] : CDN::getEndpointList()) {
        ui->comboBoxCDN->addItem(info.name, key);
    }

    // Screenshot type dropdown
    ui->comboBoxScreenshots->addItem("PNG (Portable Network Graphics)", "PNG");
    ui->comboBoxScreenshots->addItem("BMP (Windows bitmap)", "BMP");

    // Resize movies dropdown
    ui->comboBoxResizeMovies->addItem(tr("Fit (default)", "Resize Movies Dropdown"), "FIT");
    ui->comboBoxResizeMovies->addItem(tr("Off", "Resize Movies Dropdown"), "OFF");
    ui->comboBoxResizeMovies->addItem(tr("Stretch", "Resize Movies Dropdown"), "STRETCH");
    ui->comboBoxResizeMovies->addItem(tr("Crop", "Resize Movies Dropdown"), "CROP");
    ui->comboBoxResizeMovies->addItem(tr("Pixel perfect", "Resize Movies Dropdown"), "PIXELPERFECT");
    ui->comboBoxResizeMovies->addItem("4BY3", "4BY3");
    ui->comboBoxResizeMovies->addItem("4BY3PP", "4BY3PP");

    // Map: Resolutions
    QMap<QString, QString> resolutionsMap = {
        {tr("Match desktop", "Resolution Dropdown"), "MATCH_DESKTOP"},
        {tr("Custom...", "Resolution Dropdown"), "CUSTOM"},
        {"640 x 400 (8:5)", "640x400"},
        {"640 x 480 (4:3)", "640x480"},
        {"800 x 600 (4:3)", "800x600"},
        {"1024 x 768 (4:3)", "1024x768"},
        {"1280 x 720 (16:9)", "1280x720"},
        {"1280 x 800 (8:5)", "1280x800"},
        {"1280 x 1024 (5:4)", "1280x1024"},
        {"1366 x 768 (16:9)", "1366x768"},
        {"1440 x 900 (8:5)", "1440x900"},
        {"1536 x 864 (16:9)", "1536x864"},
        {"1600 x 900 (16:9)", "1600x900"},
        {"1600 x 1200 (4:3)", "1600x1200"},
        {"1920 x 1080 (16:9)", "1920x1080"},
        {"1920 x 1200 (8:5)", "1920x1200"},
        {"2560 x 1440 (16:9)", "2560x1440"},
        {"2560 x 1600 (16:10)", "2560x1600"},
        {"2880 x 1800 (16:10)", "2880x1800"},
        {"3440 x 1440 (21:9)", "3440x1440"},
        {"3840 x 2160 (16:9)", "3840x2160"},
        {"4096 x 2160 (17:9)", "4096x2160"},
    };

    // Add Resolutions
    for (auto it = resolutionsMap.begin(); it != resolutionsMap.end(); ++it) {
        // Ingame views
        ui->comboBoxResolution1->addItem(it.key(), it.value());
        ui->comboBoxResolution2->addItem(it.key(), it.value());
        ui->comboBoxResolution3->addItem(it.key(), it.value());
        // Front end views
        ui->comboBoxResolution1_2->addItem(it.key(), it.value());
        ui->comboBoxResolution2_2->addItem(it.key(), it.value());
        ui->comboBoxResolution3_2->addItem(it.key(), it.value());
    }

    // Connect custom resolution signals and slots
    connect(ui->comboBoxResolution1, &QComboBox::currentIndexChanged, this, &SettingsDialog::onResolutionChanged);
    connect(ui->comboBoxResolution2, &QComboBox::currentIndexChanged, this, &SettingsDialog::onResolutionChanged);
    connect(ui->comboBoxResolution3, &QComboBox::currentIndexChanged, this, &SettingsDialog::onResolutionChanged);
    connect(ui->comboBoxResolution1_2, &QComboBox::currentIndexChanged, this, &SettingsDialog::onResolutionChanged);
    connect(ui->comboBoxResolution2_2, &QComboBox::currentIndexChanged, this, &SettingsDialog::onResolutionChanged);
    connect(ui->comboBoxResolution3_2, &QComboBox::currentIndexChanged, this, &SettingsDialog::onResolutionChanged);

    // Map: Display mode
    QMap<QString, QString> displayModesMap = {
        {tr("Fullscreen", "Display Mode Dropdown"), "x32"},
        {tr("Windowed", "Display Mode Dropdown"), "w32"},
    };

    // Add display modes
    for (auto it = displayModesMap.begin(); it != displayModesMap.end(); ++it) {
        // Ingame views
        ui->comboBoxDisplayMode1->addItem(it.key(), it.value());
        ui->comboBoxDisplayMode2->addItem(it.key(), it.value());
        ui->comboBoxDisplayMode3->addItem(it.key(), it.value());
        // Front end views
        ui->comboBoxDisplayMode1_2->addItem(it.key(), it.value());
        ui->comboBoxDisplayMode2_2->addItem(it.key(), it.value());
        ui->comboBoxDisplayMode3_2->addItem(it.key(), it.value());
    }

    // Atmosphere Frequency dropdown
    ui->comboBoxAtmoFrequency->addItem(tr("Low", "Atmosphere Frequency Dropdown"), "LOW");
    ui->comboBoxAtmoFrequency->addItem(tr("Medium", "Atmosphere Frequency Dropdown"), "MEDIUM");
    ui->comboBoxAtmoFrequency->addItem(tr("High", "Atmosphere Frequency Dropdown"), "HIGH");

    // Atmosphere Volume dropdown
    ui->comboBoxAtmoVolume->addItem(tr("Low", "Atmosphere Volume Dropdown"), "LOW");
    ui->comboBoxAtmoVolume->addItem(tr("Medium", "Atmosphere Volume Dropdown"), "MEDIUM");
    ui->comboBoxAtmoVolume->addItem(tr("High", "Atmosphere Volume Dropdown"), "HIGH");

    // Release dropdown
    ui->comboBoxReleaseChannel->addItem(tr("Stable (Default)", "Game Release Channel"), "STABLE");
    ui->comboBoxReleaseChannel->addItem(tr("Alpha", "Game Release Channel"), "ALPHA");

    // Check if we need to allow bigger mouse sensitivity
    if (KfxVersion::hasFunctionality("mouse_sensitivity_no_multiplier") == true) {
        ui->horizontalSliderMouseSens->setMaximum(1000);
    }

    // Load the settings
    loadSettings();

    // Set input masks (number only textboxes)
    ui->lineEditApiPort->setValidator(new QIntValidator(0, 65535, this));
    ui->lineEditCreatureFlowerSize->setValidator(new QIntValidator(0, 512, this));
    ui->lineEditHandSize->setValidator(new QIntValidator(0, 500, this));
    ui->lineEditLineBoxSize->setValidator(new QIntValidator(0, 500, this));
    ui->lineEditGameturns->setValidator(new QIntValidator(0, 512, this));
    ui->lineEditMaxFps->setValidator(new QIntValidator(0, 65535, this));
    ui->lineEditGuiBlinkRate->setValidator(new QIntValidator(0, 65535, this));
    ui->lineEditNeutralFlashRate->setValidator(new QIntValidator(0, 65535, this));
    ui->lineEditUpdateInterval->setValidator(new QIntValidator(0, 365, this));

    // Set other input masks
    ui->lineEditCommandChar->setValidator(
        // Matches any printable ASCII character (from space to tilde)
        new QRegularExpressionValidator(QRegularExpression("[ -~]"), this));

    // Connect the raw mouse input checkbox
    connect(ui->checkBoxRawMouseInput, &QCheckBox::checkStateChanged, this, [this]() {
        bool isChecked = ui->checkBoxRawMouseInput->isChecked();
        ui->horizontalSliderMouseSens->setEnabled(!isChecked);
        ui->labelMouseSensPercentage->setEnabled(!isChecked);
        ui->labelMouseSens->setEnabled(!isChecked);
        ui->horizontalSliderMouseSens->setValue(isChecked ? 1 : 100);
        ui->labelMouseSensPercentage->setText(isChecked ? "" : QString("%1%").arg(ui->horizontalSliderMouseSens->value()));
    });

    // Connect the mouse sensitivity slider to update the label when moved
    connect(ui->horizontalSliderMouseSens, &QSlider::valueChanged, this, [this](int value) {
        if (!ui->checkBoxRawMouseInput->isChecked()) {
            ui->labelMouseSensPercentage->setText(QString("%1 %").arg(value));
        }
    });

    // Connect the API enabled checkbox
    connect(ui->checkBoxEnableAPI, &QCheckBox::checkStateChanged, this, [this]() {
        bool isChecked = ui->checkBoxEnableAPI->isChecked();
        ui->labelApiPort->setDisabled(!isChecked);
        ui->lineEditApiPort->setDisabled(!isChecked);
    });

    // Connect the game update checkbox
    connect(ui->checkBoxCheckForUpdates, &QCheckBox::checkStateChanged, this, [this]() {
        bool isChecked = ui->checkBoxCheckForUpdates->isChecked();
        ui->checkBoxAutoUpdate->setDisabled(!isChecked);
        ui->checkBoxBackupSaves->setDisabled(!isChecked);
        ui->lineEditUpdateInterval->setDisabled(!isChecked);
        ui->labelUpdateInterval->setDisabled(!isChecked);
    });

    // Connect the atmospheric sounds enabled checkbox
    connect(ui->checkBoxEnableAtmoSounds, &QCheckBox::checkStateChanged, this, [this]() {
        bool isChecked = ui->checkBoxEnableAtmoSounds->isChecked();
        ui->labelAtmoFrequency->setDisabled(!isChecked);
        ui->labelAtmoVolume->setDisabled(!isChecked);
        ui->comboBoxAtmoFrequency->setDisabled(!isChecked);
        ui->comboBoxAtmoVolume->setDisabled(!isChecked);
    });

    // Connect the API enabled checkbox
    connect(ui->checkBoxPacketSaveEnabled, &QCheckBox::checkStateChanged, this, [this]() {
        bool isChecked = ui->checkBoxPacketSaveEnabled->isChecked();
        ui->labelPacketSaveFileName->setDisabled(!isChecked);
        ui->lineEditPackSaveFileName->setDisabled(!isChecked);
    });

    // Connect 'Unlock cursor from game window' checkbox (alt input)
    connect(ui->checkBoxAltInput, &QCheckBox::checkStateChanged, this, [this]() {
        bool isChecked = ui->checkBoxAltInput->isChecked();
        ui->checkBoxUnlockCursorWhenPaused->setEnabled(!isChecked); // When alt input is DISABLED
        ui->checkBoxLockCursorPossession->setEnabled(isChecked); // When alt input is ENABLED
    });

    // Add handler to remember when a setting has changed
    // This should be executed at the end when the widgets and their contents are final
    addSettingsChangedHandler();

    // Map: Libraries for about page
    // We use QStringLiterals here so we can determine the order we want (Qt6 first)
    // The second string is the URL of the project and is used for when the user clicks the link
    QList<QPair<QString, QString>> aboutLibrariesMap = {
        { QStringLiteral("Qt6 (" QT_VERSION_STR ")"), "https://www.qt.io/product/qt6" },
        { QStringLiteral("libLIEF"), "https://github.com/lief-project/LIEF" },
        { QStringLiteral("bit7z"), "https://github.com/rikyoz/bit7z" },
        { QStringLiteral("7z"), "https://www.7-zip.org/" },
        { QStringLiteral("zlib"), "https://www.zlib.net/" }
    };

    // Create libraries string
    QStringList libraries;
    for (const auto &p : aboutLibrariesMap) {
        libraries << QStringLiteral("<a href=\"%1\">%2</a>").arg(p.second, p.first);
    }

    // Load about page string
    QString aboutString;
    aboutString += tr("KeeperFX v%1", "About Label").arg(KfxVersion::currentVersion.fullString);
    aboutString += "<br/>";
    aboutString += "<a href=\"https://keeperfx.net\">https://keeperfx.net</a>";
    aboutString += "<br/>";
    aboutString += "<br/>";
    aboutString += tr("KeeperFX Launcher v%1", "About Label").arg(LAUNCHER_VERSION);
    aboutString += "<br/>";
    aboutString += tr("Launcher Libraries:", "About Label");
    aboutString += "<br/>";
    aboutString += libraries.join(", ");
    aboutString += "<br/>";
    aboutString += "<br/>";
    aboutString += tr("Join us on Discord: %1", "About Label").arg(QString("<a href=\"%1\">%2</a>").arg("https://discord.gg/WxgE8WZBku", "Keeper Klan Discord"));

    // Load about page
    ui->labelAbout->setTextFormat(Qt::RichText);
    ui->labelAbout->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->labelAbout->setOpenExternalLinks(true);
    ui->labelAbout->setText(aboutString);

    // Load contributors
    QFile contributorsFile(":/res/contributors.txt");
    if (contributorsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QStringList contributors = QString::fromUtf8(contributorsFile.readAll()).split('\n', Qt::SkipEmptyParts);

        // Convert string of contributors in GitHub links
        for (QString &c : contributors) {
            c = QString("<a href=\"https://github.com/%1\">%1</a>").arg(c.trimmed());
        }

        // Setup label properties
        ui->labelContributors->setTextFormat(Qt::RichText);
        ui->labelContributors->setTextInteractionFlags(Qt::TextBrowserInteraction);
        ui->labelContributors->setOpenExternalLinks(true);

        // Set text
        ui->labelContributors->setText(tr("KeeperFX Contributors: %1", "Label Contributor List").arg(contributors.join(", ")));
    } else {
        ui->labelContributors->setText(""); // Hide
    }

    // Load launcher translators
    QFile launcherTranslatorsFile(":/res/launcher-translators.txt");
    if (launcherTranslatorsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {

        QStringList launcherTranslators = QString::fromUtf8(launcherTranslatorsFile.readAll()).split('\n', Qt::SkipEmptyParts);

        // Handle each translator entry
        for (QString &t : launcherTranslators) {
            t = t.trimmed();
            if(t.contains(',')){
                QStringList parts = t.split(",");
                if(parts.length() == 2){
                    t = QString("<a href=\"%1\">%2</a>").arg(parts[1], parts[0]);
                } else {
                    qWarning() << "Invalid launcher translator entry:" << t;
                }
            }
        }

        // Setup label properties
        ui->labelLauncherTranslators->setTextFormat(Qt::RichText);
        ui->labelLauncherTranslators->setTextInteractionFlags(Qt::TextBrowserInteraction);
        ui->labelLauncherTranslators->setOpenExternalLinks(true);

        // Set text
        ui->labelLauncherTranslators->setText(tr("KeeperFX Launcher Translators: %1", "Label Translator List").arg(launcherTranslators.join(", ")));
    } else {
        ui->labelLauncherTranslators->setText(""); // Hide
    }
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::loadSettings()
{
    // ========================================================================
    // ================================ GAME ==================================
    // ========================================================================

    ui->comboBoxLanguage->setCurrentIndex(
        ui->comboBoxLanguage->findData(Settings::getKfxSetting("LANGUAGE").toString()));

    if (KfxVersion::hasFunctionality("startup_config_option")) {
        ui->checkBoxDisplayIntro->setChecked(false);
        ui->checkBoxDisplaySplashScreens->setChecked(false);
        QString startupString = Settings::getKfxSetting("STARTUP").toString().trimmed();
        QStringList startupScreens = startupString.split(" ");
        for (const QString &startupScreen : std::as_const(startupScreens)) {
            if (startupScreen == "INTRO") {
                ui->checkBoxDisplayIntro->setChecked(true);
            } else if (startupScreen == "FX" || startupScreen == "LEGAL") {
                ui->checkBoxDisplaySplashScreens->setChecked(true);
            } else {
                this->hiddenStartupScreens << startupScreen;
            }
        }
    } else {
        ui->checkBoxDisplayIntro->setChecked(Settings::getLauncherSetting("GAME_PARAM_NO_INTRO") == false);
        ui->checkBoxDisplaySplashScreens->setChecked(Settings::getKfxSetting("DISABLE_SPLASH_SCREENS") == false);
    }

    ui->checkBoxCheats->setChecked(Settings::getLauncherSetting("GAME_PARAM_ALEX") == true);
    ui->checkBoxCensorship->setChecked(Settings::getKfxSetting("CENSORSHIP") == true);
    ui->comboBoxScreenshots->setCurrentIndex(
        ui->comboBoxScreenshots->findData(Settings::getKfxSetting("SCREENSHOT").toString()));
    ui->lineEditGameturns->setText(Settings::getLauncherSetting("GAME_PARAM_FPS").toString());
    ui->lineEditCommandChar->setText(Settings::getKfxSetting("COMMAND_CHAR").toString());
    ui->checkBoxDeltaTime->setChecked(Settings::getKfxSetting("DELTA_TIME") == true);
    ui->checkBoxFreezeGameNoFocus->setChecked(Settings::getKfxSetting("FREEZE_GAME_ON_FOCUS_LOST")
                                              == true);

    bool isPacketSaveEnabled = Settings::getLauncherSetting("GAME_PARAM_PACKET_SAVE_ENABLED") == true;
    ui->checkBoxPacketSaveEnabled->setChecked(isPacketSaveEnabled);
    ui->labelPacketSaveFileName->setDisabled(!isPacketSaveEnabled);
    ui->lineEditPackSaveFileName->setDisabled(!isPacketSaveEnabled);
    ui->lineEditPackSaveFileName->setText(Settings::getLauncherSetting("GAME_PARAM_PACKET_SAVE_FILE_NAME").toString());

    ui->checkBoxExitOnLuaError->setChecked(Settings::getKfxSetting("EXIT_ON_LUA_ERROR") == true);

    ui->checkBoxAutoEnableFlee->setChecked(Settings::getKfxSetting("FLEE_BUTTON_DEFAULT") == true);
    ui->checkBoxAutoEnableImprison->setChecked(Settings::getKfxSetting("IMPRISON_BUTTON_DEFAULT") == true);

    ui->lineEditLaunchOptions->setText(Settings::getLauncherSetting("EXTRA_GAME_LAUNCH_OPTIONS").toString());

    // ============================================================================
    // ================================ GRAPHICS ==================================
    // ============================================================================

    popupComboBoxMonitorDisplay->setCurrentIndex(popupComboBoxMonitorDisplay->findData(Settings::getKfxSetting("DISPLAY_NUMBER").toString()));
    ui->checkBoxSmoothenVideo->setChecked(Settings::getLauncherSetting("GAME_PARAM_VID_SMOOTH")
                                          == true);

    // Resize movies
    // Small fix because "ON" defaults to "FIT", but we just want "FIT" here
    QString resizeMoviesString = Settings::getKfxSetting("RESIZE_MOVIES").toString();
    if (resizeMoviesString == "ON") {
        resizeMoviesString = "FIT";
    }
    ui->comboBoxResizeMovies->setCurrentIndex(
        ui->comboBoxResizeMovies->findData(resizeMoviesString));

    // Loop trough the in-game resolutions
    int resolutionIndex = 0;
    for (const QString &resolutionString : Settings::getKfxSetting("INGAME_RES").toString().trimmed().split(" "))
    {
        // Vars
        QString res, mode;

        // Check for DESKTOP and DESKTOP_FULL
        if (resolutionString == "DESKTOP" || resolutionString == "DESKTOP_FULL") {
            res = "MATCH_DESKTOP";
            if (resolutionString == "DESKTOP") {
                mode = "w32";
            } else if (resolutionString == "DESKTOP_FULL") {
                mode = "x32";
            }
        } else {

            // Use regex to split res and mode
            QRegularExpressionMatch match = QRegularExpression("(x32|w32)$").match(resolutionString);
            if (match.hasMatch() == false) {
                qWarning() << "Invalid resolution in 'keeperfx.cfg':" << resolutionString;
                resolutionIndex++;
                continue;
            }

            // Get vars based on matches
            res = resolutionString.left(match.capturedStart(1)); // Part before x32/w32
            mode = match.captured(1);
        }

        // Select combobox widgets
        QComboBox *resolutionComboBox;
        QComboBox *displayModeComboBox;
        switch (resolutionIndex) {
            case 0:
                resolutionComboBox = ui->comboBoxResolution1;
                displayModeComboBox = ui->comboBoxDisplayMode1;
                break;
            case 1:
                resolutionComboBox = ui->comboBoxResolution2;
                displayModeComboBox = ui->comboBoxDisplayMode2;
                break;
            case 2:
                resolutionComboBox = ui->comboBoxResolution3;
                displayModeComboBox = ui->comboBoxDisplayMode3;
                break;
        }

        if(res.toLower().contains("x")) {

            // Split width and height
            QStringList sizes = res.toLower().split("x");
            int width = sizes[0].toInt();
            int height = sizes[1].toInt();

            // Add missing custom resolutions
            if (resolutionComboBox->findData(res) == -1) {
                addCustomResolution(width, height, nullptr);
            }
        }

        resolutionComboBox->setCurrentIndex(resolutionComboBox->findData(res));
        displayModeComboBox->setCurrentIndex(displayModeComboBox->findData(mode));
        resolutionComboBox->setProperty("previousIndex", resolutionComboBox->currentIndex());

        resolutionIndex++;
    }

    // Loop trough the front end resolutions
    resolutionIndex = 0;
    for (const QString &resolutionString : Settings::getKfxSetting("FRONTEND_RES").toString().trimmed().split(" ")) {
        // Vars
        QString res, mode;

        // Check for DESKTOP and DESKTOP_FULL
        if (resolutionString == "DESKTOP" || resolutionString == "DESKTOP_FULL") {
            res = "MATCH_DESKTOP";
            if (resolutionString == "DESKTOP") {
                mode = "w32";
            } else if (resolutionString == "DESKTOP_FULL") {
                mode = "x32";
            }
        } else {
            // Use regex to split res and mode
            QRegularExpressionMatch match = QRegularExpression("(x32|w32)$").match(resolutionString);
            if (match.hasMatch() == false) {
                qWarning() << "Invalid resolution in 'keeperfx.cfg':" << resolutionString;
                resolutionIndex++;
                continue;
            }

            // Get vars based on matches
            res = resolutionString.left(match.capturedStart(1)); // Part before x32/w32
            mode = match.captured(1);
        }

        // Select combobox widgets
        QComboBox *resolutionComboBox;
        QComboBox *displayModeComboBox;
        switch (resolutionIndex) {
            case 0:
                resolutionComboBox = ui->comboBoxResolution1_2;
                displayModeComboBox = ui->comboBoxDisplayMode1_2;
                break;
            case 1:
                resolutionComboBox = ui->comboBoxResolution2_2;
                displayModeComboBox = ui->comboBoxDisplayMode2_2;
                break;
            case 2:
                resolutionComboBox = ui->comboBoxResolution3_2;
                displayModeComboBox = ui->comboBoxDisplayMode3_2;
                break;
        }

        if(res.toLower().contains("x")) {

            // Split width and height
            QStringList sizes = res.toLower().split("x");
            int width = sizes[0].toInt();
            int height = sizes[1].toInt();

            // Add missing custom resolutions
            if (resolutionComboBox->findData(res) == -1) {
                addCustomResolution(width, height, nullptr);
            }
        }

        resolutionComboBox->setCurrentIndex(resolutionComboBox->findData(res));
        displayModeComboBox->setCurrentIndex(displayModeComboBox->findData(mode));
        resolutionComboBox->setProperty("previousIndex", resolutionComboBox->currentIndex());

        resolutionIndex++;
    }

    ui->lineEditCreatureFlowerSize->setText(Settings::getKfxSetting("CREATURE_STATUS_SIZE").toString());
    ui->lineEditLineBoxSize->setText(Settings::getKfxSetting("LINE_BOX_SIZE").toString());
    ui->lineEditHandSize->setText(Settings::getKfxSetting("HAND_SIZE").toString());

    if (KfxVersion::hasFunctionality("max_frames_per_second") == true) {
        if (KfxVersion::hasFunctionality("auto_determine_monitor_refresh_rate") == true) {
            QString maxFps = Settings::getKfxSetting("FRAMES_PER_SECOND").toString();
            if(maxFps.contains("AUTO")){
                ui->checkBoxAutoDetermineMaxFps->setChecked(true);
                QString maxFpsNumber = maxFps.split(" ").last();
                if(maxFpsNumber.isEmpty() == false){
                    ui->lineEditMaxFps->setText(maxFpsNumber);
                } else {
                    ui->lineEditMaxFps->setText("0");
                }
            } else {
                ui->checkBoxAutoDetermineMaxFps->setChecked(false);
                ui->lineEditMaxFps->setText(QString::number(Settings::getKfxSetting("FRAMES_PER_SECOND").toInt()));
            }
        } else {
            ui->lineEditMaxFps->setText(QString::number(Settings::getKfxSetting("FRAMES_PER_SECOND").toInt()));
        }
    }

    if (KfxVersion::hasFunctionality("gui_and_neutral_blink_speed") == true) {
        ui->lineEditGuiBlinkRate->setText(Settings::getKfxSetting("GUI_BLINK_RATE").toString());
        ui->lineEditNeutralFlashRate->setText(Settings::getKfxSetting("NEUTRAL_FLASH_RATE").toString());
    }

    // =========================================================================
    // ================================ SOUND ==================================
    // =========================================================================

    ui->checkBoxEnableSound->setChecked(Settings::getLauncherSetting("GAME_PARAM_NO_SOUND") == false);
    ui->checkBoxUseCDMusic->setChecked(Settings::getLauncherSetting("GAME_PARAM_USE_CD_MUSIC") == true);
    ui->checkBoxPauseMusicWhenPaused->setChecked(
        Settings::getKfxSetting("PAUSE_MUSIC_WHEN_GAME_PAUSED") == true);
    ui->checkBoxMuteAudioWhenNotFocused->setChecked(
        Settings::getKfxSetting("MUTE_AUDIO_ON_FOCUS_LOST") == true);

    // Atmospheric sounds
    ui->checkBoxEnableAtmoSounds->setChecked(
        Settings::getKfxSetting("ATMOSPHERIC_SOUNDS") == true);
    ui->comboBoxAtmoFrequency->setCurrentIndex(
        ui->comboBoxAtmoFrequency->findData(Settings::getKfxSetting("ATMOS_FREQUENCY").toString()));
    ui->comboBoxAtmoVolume->setCurrentIndex(
        ui->comboBoxAtmoVolume->findData(Settings::getKfxSetting("ATMOS_VOLUME").toString()));

    // Atmospheric checkbox disable / enable extra info
    bool isAtmoSoundsEnabled = Settings::getKfxSetting("ATMOSPHERIC_SOUNDS") == true;
    ui->labelAtmoFrequency->setDisabled(!isAtmoSoundsEnabled);
    ui->labelAtmoVolume->setDisabled(!isAtmoSoundsEnabled);
    ui->comboBoxAtmoFrequency->setDisabled(!isAtmoSoundsEnabled);
    ui->comboBoxAtmoVolume->setDisabled(!isAtmoSoundsEnabled);

    // =========================================================================
    // ================================ INPUT ==================================
    // =========================================================================

    int mouseSens = Settings::getKfxSetting("POINTER_SENSITIVITY").toInt();
    if (mouseSens == 0) {
        ui->checkBoxRawMouseInput->setChecked(true);
        ui->horizontalSliderMouseSens->setDisabled(true);
        ui->labelMouseSensPercentage->setDisabled(true);
        ui->labelMouseSens->setDisabled(true);
        ui->horizontalSliderMouseSens->setValue(1);
        ui->labelMouseSensPercentage->setText("");
    } else {
        ui->checkBoxRawMouseInput->setChecked(false);
        ui->horizontalSliderMouseSens->setDisabled(false);
        ui->labelMouseSensPercentage->setDisabled(false);
        ui->labelMouseSens->setDisabled(false);
        ui->horizontalSliderMouseSens->setValue(mouseSens);
        ui->labelMouseSensPercentage->setText(QString::number(mouseSens) + "%");
    }

    ui->checkBoxAltInput->setChecked(Settings::getLauncherSetting("GAME_PARAM_ALT_INPUT") == true);
    ui->checkBoxUnlockCursorWhenPaused->setChecked(Settings::getKfxSetting("UNLOCK_CURSOR_WHEN_GAME_PAUSED") == true);
    ui->checkBoxLockCursorPossession->setChecked(Settings::getKfxSetting("LOCK_CURSOR_IN_POSSESSION") == true);
    ui->checkBoxScreenEdgePanning->setChecked(Settings::getKfxSetting("CURSOR_EDGE_CAMERA_PANNING") == true);

    ui->checkBoxUnlockCursorWhenPaused->setEnabled(Settings::getLauncherSetting("GAME_PARAM_ALT_INPUT") == false); // When alt input is DISABLED
    ui->checkBoxLockCursorPossession->setEnabled(Settings::getLauncherSetting("GAME_PARAM_ALT_INPUT") == true); // When alt input is ENABLED

    ui->checkBoxEnableTagModeToggle->setChecked(Settings::getKfxSetting("TAG_MODE_TOGGLING") == true);
    ui->comboBoxDefaultTagMode->setCurrentIndex(ui->comboBoxDefaultTagMode->findData(Settings::getKfxSetting("DEFAULT_TAG_MODE").toString()));

    // ===============================================================================
    // ================================ MULTIPLAYER ==================================
    // ===============================================================================

    //ui->lineEditMasterServer->setText(Settings::getKfxSetting("MASTERSERVER_HOST").toString());

    // =======================================================================
    // ================================ API ==================================
    // =======================================================================

    bool isApiEnabled = Settings::getKfxSetting("API_ENABLED") == true;
    ui->checkBoxEnableAPI->setChecked(isApiEnabled);
    ui->lineEditApiPort->setText(Settings::getKfxSetting("API_PORT").toString());

    ui->labelApiPort->setDisabled(!isApiEnabled);
    ui->lineEditApiPort->setDisabled(!isApiEnabled);

    // ============================================================================
    // ================================ LAUNCHER ==================================
    // ============================================================================

    ui->comboBoxLauncherLanguage->setCurrentIndex(ui->comboBoxLauncherLanguage->findData(Settings::getLauncherSetting("LAUNCHER_LANGUAGE").toString()));
    ui->checkBoxWebsiteIntegration->setChecked(Settings::getLauncherSetting("WEBSITE_INTEGRATION_ENABLED") == true);
    ui->checkBoxCrashReporting->setChecked(Settings::getLauncherSetting("CRASH_REPORTING_ENABLED") == true);
    ui->checkBoxOpenOnGameScreen->setChecked(Settings::getLauncherSetting("OPEN_ON_GAME_SCREEN") == true);
    ui->checkBoxShowDirInWindowTitle->setChecked(Settings::getLauncherSetting("SHOW_DIR_NAME_IN_WINDOW_TITLE") == true);
    ui->comboBoxPlayButtonTheme->setCurrentIndex(ui->comboBoxPlayButtonTheme->findData(Settings::getLauncherSetting("PLAY_BUTTON_THEME").toString()));

    // Updates
    bool isUpdateCheckEnabled = Settings::getLauncherSetting("CHECK_FOR_UPDATES_ENABLED").toBool();
    ui->checkBoxCheckForUpdates->setChecked(isUpdateCheckEnabled);
    ui->comboBoxReleaseChannel->setCurrentIndex(ui->comboBoxReleaseChannel->findData(Settings::getLauncherSetting("CHECK_FOR_UPDATES_RELEASE").toString()));
    ui->checkBoxAutoUpdate->setDisabled(!isUpdateCheckEnabled);
    ui->checkBoxAutoUpdate->setChecked(Settings::getLauncherSetting("AUTO_UPDATE") == true);
    ui->checkBoxBackupSaves->setDisabled(!isUpdateCheckEnabled);
    ui->checkBoxBackupSaves->setChecked(Settings::getLauncherSetting("BACKUP_SAVES") == true);
    ui->lineEditUpdateInterval->setText(Settings::getLauncherSetting("CHECK_FOR_UPDATES_INTERVAL_DAYS").toString());
    ui->lineEditUpdateInterval->setDisabled(!isUpdateCheckEnabled);
    ui->labelUpdateInterval->setDisabled(!isUpdateCheckEnabled);

    ui->checkBoxAutoRemoveLeftoverFiles->setChecked(Settings::getLauncherSetting("AUTO_REMOVE_LEFTOVER_FILES") == true);

    ui->comboBoxCDN->setCurrentIndex(ui->comboBoxCDN->findData(Settings::getLauncherSetting("CDN_ENDPOINT").toString()));
}

void SettingsDialog::saveSettings()
{

    // ========================================================================
    // ================================ GAME ==================================
    // ========================================================================

    Settings::setKfxSetting("LANGUAGE", ui->comboBoxLanguage->currentData().toString());
    Settings::setLauncherSetting("GAME_PARAM_ALEX", ui->checkBoxCheats->isChecked());
    Settings::setKfxSetting("CENSORSHIP", ui->checkBoxCensorship->isChecked());
    Settings::setKfxSetting("SCREENSHOT", ui->comboBoxScreenshots->currentData().toString());
    Settings::setLauncherSetting("GAME_PARAM_FPS", ui->lineEditGameturns->text());
    Settings::setKfxSetting("DELTA_TIME", ui->checkBoxDeltaTime->isChecked());
    Settings::setKfxSetting("FREEZE_GAME_ON_FOCUS_LOST", ui->checkBoxFreezeGameNoFocus->isChecked());
    Settings::setLauncherSetting("GAME_PARAM_PACKET_SAVE_ENABLED", ui->checkBoxPacketSaveEnabled->isChecked() == true);
    Settings::setKfxSetting("EXIT_ON_LUA_ERROR", ui->checkBoxExitOnLuaError->isChecked());
    Settings::setKfxSetting("FLEE_BUTTON_DEFAULT", ui->checkBoxAutoEnableFlee->isChecked());
    Settings::setKfxSetting("IMPRISON_BUTTON_DEFAULT", ui->checkBoxAutoEnableImprison->isChecked());

    // Handle different ways of the startup screens depending on KFX version
    if (KfxVersion::hasFunctionality("startup_config_option")) {
        QStringList startupScreens;
        // Add legal and FX
        if (ui->checkBoxDisplaySplashScreens->isChecked() == true) {
            startupScreens << "LEGAL" << "FX";
        }
        // Add intro
        if (ui->checkBoxDisplayIntro->isChecked() == true) {
            startupScreens << "INTRO";
        }
        // Add any hidden startup screens
        if (this->hiddenStartupScreens.isEmpty() == false) {
            qDebug() << "Adding hidden startup screens to STARTUP:" << this->hiddenStartupScreens;
            startupScreens << this->hiddenStartupScreens;
        }
        Settings::setKfxSetting("STARTUP", startupScreens.join(" "));
        Settings::removeKfxSetting("DISABLE_SPLASH_SCREENS");
    } else {
        Settings::setLauncherSetting("GAME_PARAM_NO_INTRO", ui->checkBoxDisplayIntro->isChecked() == false);
        Settings::setKfxSetting("DISABLE_SPLASH_SCREENS", ui->checkBoxDisplaySplashScreens->isChecked() == false);
    }

    // Make sure command char is not empty
    if(ui->lineEditCommandChar->text().isEmpty() == true){
        ui->lineEditCommandChar->setText("!");
    }
    Settings::setKfxSetting("COMMAND_CHAR", ui->lineEditCommandChar->text());

    // Packet save
    QString packetSaveFileName = ui->lineEditPackSaveFileName->text();
    packetSaveFileName = packetSaveFileName.trimmed().replace(" ", "_");
    if (packetSaveFileName.isEmpty()) {
        packetSaveFileName = "packetsave.pck";
    }
    if (packetSaveFileName.toLower().endsWith(".pck") == false) {
        packetSaveFileName = packetSaveFileName + ".pck";
    }
    Settings::setLauncherSetting("GAME_PARAM_PACKET_SAVE_FILE_NAME", packetSaveFileName);

    Settings::setLauncherSetting("EXTRA_GAME_LAUNCH_OPTIONS", ui->lineEditLaunchOptions->text());

    // ============================================================================
    // ================================ GRAPHICS ==================================
    // ============================================================================

    Settings::setLauncherSetting("GAME_PARAM_VID_SMOOTH", ui->checkBoxSmoothenVideo->isChecked());
    Settings::setKfxSetting("DISPLAY_NUMBER", popupComboBoxMonitorDisplay->currentData().toString());
    Settings::setKfxSetting("RESIZE_MOVIES", ui->comboBoxResizeMovies->currentData().toString());

    // Save the in-game resolutions
    QString resolutionString = "";
    for (int i = 0; i < 3; i++) {
        // Get vars
        QString res, mode;
        switch (i) {
        case 0:
            res = ui->comboBoxResolution1->currentData().toString();
            mode = ui->comboBoxDisplayMode1->currentData().toString();
            break;
        case 1:
            res = ui->comboBoxResolution2->currentData().toString();
            mode = ui->comboBoxDisplayMode2->currentData().toString();
            break;
        case 2:
            res = ui->comboBoxResolution3->currentData().toString();
            mode = ui->comboBoxDisplayMode3->currentData().toString();
            break;
        }

        // Check if user wants to match their desktop size
        if (res == "MATCH_DESKTOP") {
            resolutionString += "DESKTOP";

            // Check for fullscreen
            if (mode == "x32") {
                resolutionString += "_FULL";
            }
        } else {
            // Add absolute resoltion size
            resolutionString += res;
            resolutionString += mode;
        }

        // Add spaces in between
        if (i != 2) {
            resolutionString += " ";
        }
    }
    Settings::setKfxSetting("INGAME_RES", resolutionString);

    // Save the front end resolutions
    resolutionString = "";
    for (int i = 0; i < 3; i++) {
        // Get vars
        QString res, mode;
        switch (i) {
        case 0:
            res = ui->comboBoxResolution1_2->currentData().toString();
            mode = ui->comboBoxDisplayMode1_2->currentData().toString();
            break;
        case 1:
            res = ui->comboBoxResolution2_2->currentData().toString();
            mode = ui->comboBoxDisplayMode2_2->currentData().toString();
            break;
        case 2:
            res = ui->comboBoxResolution3_2->currentData().toString();
            mode = ui->comboBoxDisplayMode3_2->currentData().toString();
            break;
        }

        // Check if user wants to match their desktop size
        if (res == "MATCH_DESKTOP") {
            resolutionString += "DESKTOP";

            // Check for fullscreen
            if (mode == "x32") {
                resolutionString += "_FULL";
            }
        } else {
            // Add absolute resoltion size
            resolutionString += res;
            resolutionString += mode;
        }

        // Add spaces in between
        if (i != 2) {
            resolutionString += " ";
        }
    }
    Settings::setKfxSetting("FRONTEND_RES", resolutionString);

    Settings::setKfxSetting("CREATURE_STATUS_SIZE", ui->lineEditCreatureFlowerSize->text());
    Settings::setKfxSetting("LINE_BOX_SIZE", ui->lineEditLineBoxSize->text());
    Settings::setKfxSetting("HAND_SIZE", ui->lineEditHandSize->text());

    if (KfxVersion::hasFunctionality("max_frames_per_second") == true) {
        if (KfxVersion::hasFunctionality("auto_determine_monitor_refresh_rate") == true && ui->checkBoxAutoDetermineMaxFps->isChecked()) {
            Settings::setKfxSetting("FRAMES_PER_SECOND", "AUTO " + ui->lineEditMaxFps->text());
        } else {
            Settings::setKfxSetting("FRAMES_PER_SECOND", ui->lineEditMaxFps->text());
        }
    }

    if (KfxVersion::hasFunctionality("gui_and_neutral_blink_speed") == true) {
        Settings::setKfxSetting("GUI_BLINK_RATE", ui->lineEditGuiBlinkRate->text());
        Settings::setKfxSetting("NEUTRAL_FLASH_RATE", ui->lineEditNeutralFlashRate->text());
    }

    // =========================================================================
    // ================================ SOUND ==================================
    // =========================================================================

    Settings::setLauncherSetting("GAME_PARAM_NO_SOUND", ui->checkBoxEnableSound->isChecked() == false);
    Settings::setLauncherSetting("GAME_PARAM_USE_CD_MUSIC", ui->checkBoxUseCDMusic->isChecked());
    Settings::setKfxSetting("PAUSE_MUSIC_WHEN_GAME_PAUSED", ui->checkBoxPauseMusicWhenPaused->isChecked());
    Settings::setKfxSetting("MUTE_AUDIO_ON_FOCUS_LOST", ui->checkBoxMuteAudioWhenNotFocused->isChecked());
    Settings::setKfxSetting("ATMOSPHERIC_SOUNDS", ui->checkBoxEnableAtmoSounds->isChecked());
    Settings::setKfxSetting("ATMOS_FREQUENCY", ui->comboBoxAtmoFrequency->currentData().toString());
    Settings::setKfxSetting("ATMOS_VOLUME", ui->comboBoxAtmoVolume->currentData().toString());

    // =========================================================================
    // ================================ INPUT ==================================
    // =========================================================================

    if(ui->checkBoxRawMouseInput->isChecked()){
        Settings::setKfxSetting("POINTER_SENSITIVITY", 0);
    } else {
        Settings::setKfxSetting("POINTER_SENSITIVITY", ui->horizontalSliderMouseSens->value());
    }

    Settings::setLauncherSetting("GAME_PARAM_ALT_INPUT", ui->checkBoxAltInput->isChecked() == true);
    Settings::setKfxSetting("UNLOCK_CURSOR_WHEN_GAME_PAUSED", ui->checkBoxUnlockCursorWhenPaused->isChecked() == true);
    Settings::setKfxSetting("LOCK_CURSOR_IN_POSSESSION", ui->checkBoxLockCursorPossession->isChecked() == true);
    Settings::setKfxSetting("CURSOR_EDGE_CAMERA_PANNING", ui->checkBoxScreenEdgePanning->isChecked() == true);

    Settings::setKfxSetting("TAG_MODE_TOGGLING", ui->checkBoxEnableTagModeToggle->isChecked() == true);
    Settings::setKfxSetting("DEFAULT_TAG_MODE", ui->comboBoxDefaultTagMode->currentData().toString());

    // ===============================================================================
    // ================================ MULTIPLAYER ==================================
    // ===============================================================================

    //Settings::setKfxSetting("MASTERSERVER_HOST", ui->lineEditMasterServer->text());

    // =======================================================================
    // ================================ API ==================================
    // =======================================================================

    Settings::setKfxSetting("API_ENABLED", ui->checkBoxEnableAPI->isChecked() == true);
    Settings::setKfxSetting("API_PORT", ui->lineEditApiPort->text());

    // ============================================================================
    // ================================ LAUNCHER ==================================
    // ============================================================================

    Settings::setLauncherSetting("LAUNCHER_LANGUAGE", ui->comboBoxLauncherLanguage->currentData().toString());
    Settings::setLauncherSetting("WEBSITE_INTEGRATION_ENABLED", ui->checkBoxWebsiteIntegration->isChecked() == true);
    Settings::setLauncherSetting("CRASH_REPORTING_ENABLED", ui->checkBoxCrashReporting->isChecked() == true);
    Settings::setLauncherSetting("OPEN_ON_GAME_SCREEN", ui->checkBoxOpenOnGameScreen->isChecked() == true);
    Settings::setLauncherSetting("SHOW_DIR_NAME_IN_WINDOW_TITLE", ui->checkBoxShowDirInWindowTitle->isChecked() == true);
    Settings::setLauncherSetting("PLAY_BUTTON_THEME", ui->comboBoxPlayButtonTheme->currentData().toString());

    // Updates
    Settings::setLauncherSetting("CHECK_FOR_UPDATES_ENABLED", ui->checkBoxCheckForUpdates->isChecked() == true);
    Settings::setLauncherSetting("CHECK_FOR_UPDATES_RELEASE", ui->comboBoxReleaseChannel->currentData().toString());
    Settings::setLauncherSetting("AUTO_UPDATE", ui->checkBoxAutoUpdate->isChecked() == true);
    Settings::setLauncherSetting("BACKUP_SAVES", ui->checkBoxBackupSaves->isChecked() == true);
    Settings::setLauncherSetting("CHECK_FOR_UPDATES_INTERVAL_DAYS", ui->lineEditUpdateInterval->text());

    Settings::setLauncherSetting("AUTO_REMOVE_LEFTOVER_FILES", ui->checkBoxAutoRemoveLeftoverFiles->isChecked() == true);

    Settings::setLauncherSetting("CDN_ENDPOINT", ui->comboBoxCDN->currentData().toString());

    // Close the settings screen
    this->close();
}

void SettingsDialog::restoreSettings()
{
    // Ask if the user is sure
    int result = QMessageBox::question(this,
        tr("KeeperFX Settings", "MessageBox Title"),
        tr("Do you want to restore the default settings?\n\nAll your custom settings will be lost.", "MessageBox Text"));

    // Cancel close if user is not sure
    if (result != QMessageBox::Yes) {
        return;
    }

    // Remember original KeeperFX release path because we don't want that to change
    QString kfxReleasePath = Settings::getLauncherSetting("CHECK_FOR_UPDATES_RELEASE").toString();

    // Get '_keeperfx.cfg' file
    QFile originalSettingsFile(QCoreApplication::applicationDirPath() + "/_keeperfx.cfg");
    if(originalSettingsFile.exists() == false){
        return;
    }

    // Get current keeperfx settings file
    QFile currentSettingsFile = Settings::getKfxConfigFile();
    if(currentSettingsFile.exists() == false){
        return;
    }

    // Get path
    QString currentSettingsFilePath = currentSettingsFile.fileName();
    if(currentSettingsFilePath.isEmpty()){
        return;
    }

    // Try to remove current settings file
    if(currentSettingsFile.remove() == false){
        return;
    }

    // Copy original settings file
    if(originalSettingsFile.copy(currentSettingsFilePath) == false){
        return;
    }

    // Reset launcher settings
    Settings::resetLauncherSettings();

    // Reload settings
    Settings::load();

    // Restore the original release path
    // We don't want the reset to change it and possibly trigger updates
    Settings::setLauncherSetting("CHECK_FOR_UPDATES_RELEASE", kfxReleasePath);

    QMessageBox::information(this,
        tr("KeeperFX Settings", "MessageBox Title"),
        tr("The default settings have been restored.", "MessageBox Text"));

    // Accept the dialog
    // It would be nicer to update the GUI, but we have already updated the settings files.
    // This means that a user might get confused if they click 'Cancel' thinking they didn't save the restore.
    // Eventually we would like to just update the GUI so the user can cancel it.
    this->accept();
}

void SettingsDialog::addSettingsChangedHandler()
{
    // Find all QCheckBox
    QList<QCheckBox *> checkBoxes = ui->tabWidget->findChildren<QCheckBox *>();
    for (QCheckBox *checkBox : std::as_const(checkBoxes)) {
        connect(checkBox, &QCheckBox::checkStateChanged, this, [this]() {
            this->settingHasChanged = true;
            ui->buttonBox->button(QDialogButtonBox::Save)->setDisabled(false);
        });
    }

    // Find all QComboBox
    QList<QComboBox *> comboBoxes = ui->tabWidget->findChildren<QComboBox *>();
    for (QComboBox *comboBox : std::as_const(comboBoxes)) {
        connect(comboBox, &QComboBox::currentIndexChanged, this, [this]() {
            this->settingHasChanged = true;
            ui->buttonBox->button(QDialogButtonBox::Save)->setDisabled(false);
        });
    }

    // Find all QLineEdit
    QList<QLineEdit *> lineEdits = ui->tabWidget->findChildren<QLineEdit *>();
    for (QLineEdit *lineEdit : std::as_const(lineEdits)) {
        connect(lineEdit, &QLineEdit::textChanged, this, [this]() {
            this->settingHasChanged = true;
            ui->buttonBox->button(QDialogButtonBox::Save)->setDisabled(false);
        });
    }

    // Find all PopupSignalComboBox
    QList<PopupSignalComboBox *> popupComboBoxes = ui->tabWidget->findChildren<PopupSignalComboBox *>();
    for (PopupSignalComboBox *popupComboBox : std::as_const(popupComboBoxes)) {
        connect(popupComboBox, &PopupSignalComboBox::currentIndexChanged, this, [this]() {
            this->settingHasChanged = true;
            ui->buttonBox->button(QDialogButtonBox::Save)->setDisabled(false);
        });
    }
}

void SettingsDialog::cancel()
{
    // Check if any settings have been changed
    if (settingHasChanged == true) {

        // Ask if the user is sure
        int result = QMessageBox::question(this,
                                           tr("KeeperFX Settings", "MessageBox Title"),
                                           tr("One or more settings have been changed. Are you sure you want to return without saving?", "MessageBox Text"));

        // Cancel close if user is not sure
        if (result != QMessageBox::Yes) {
            return;
        }
    }

    // Close the settings screen
    this->close();
}

void SettingsDialog::setupDisplayMonitorDropdown()
{
    // Get some details of the placeholder combo box and delete it
    QComboBox *oldComboBox = ui->comboBoxDisplayMonitor;
    QRect geometry = oldComboBox->geometry();
    QWidget *parentWidget = oldComboBox->parentWidget();
    delete ui->comboBoxDisplayMonitor;

    // Create the combobox that handles the popup signal
    // This signal is used to show and hide monitor display numers
    popupComboBoxMonitorDisplay = new PopupSignalComboBox(parentWidget);
    popupComboBoxMonitorDisplay->setGeometry(geometry);

    // Get the list of available screens
    QList<QScreen *> screens = QGuiApplication::screens();

    // Loop through each screen and print index and name
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens.at(i);

        int index = i;
        int number = index + 1;
        int roundedRefreshRate = qRound(screen->refreshRate());

        QString name = "#" + QString::number(number) + " " +
                       ((screen->model().isEmpty() == false) ? screen->model(): "-") + " "
                       + QString::number(roundedRefreshRate) + "hz "
                       + QString::number(screen->geometry().width()) + "x"
                       + QString::number(screen->geometry().height());

        popupComboBoxMonitorDisplay->addItem(name, QString::number(number));
    }

    // Connect slots to show and hide monitor overlays
    QObject::connect(popupComboBoxMonitorDisplay, &PopupSignalComboBox::popupOpened, this, &SettingsDialog::showMonitorNumberOverlays);
    QObject::connect(popupComboBoxMonitorDisplay, &PopupSignalComboBox::popupClosed, this, &SettingsDialog::hideMonitorNumberOverlays);
}

void SettingsDialog::showMonitorNumberOverlays()
{
    qDebug() << "Show monitor number overlays";

    // We can't show the overlays on wayland
    // They will stack like windows and we can't change their position
    // TODO: monitor when this is possible on wayland and allow it again
    if (QGuiApplication::platformName() == "wayland") {
        qDebug() << "Monitor overlays not shown because running under 'wayland'";
        return;
    }

    // Get the list of all screens
    QList<QScreen *> screens = QGuiApplication::screens();

    // Create a label on each screen at the top-right corner
    for (int i = 0; i < screens.size(); ++i) {

        QScreen *screen = screens[i];

        // Create the overlay widget
        QWidget *overlay = new QWidget;
        overlay->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                                | Qt::BypassWindowManagerHint | Qt::X11BypassWindowManagerHint);
        overlay->setScreen(screen);

        // Set background color to black with
        QPalette palette = overlay->palette();
        palette.setColor(QPalette::Window, QColor(0, 0, 0));
        overlay->setAutoFillBackground(true);
        overlay->setPalette(palette);

        // Set size and position to top-right of the respective screen
        QRect screenGeometry = screen->geometry();
        overlay->setGeometry(screenGeometry.right() - 120, screenGeometry.top() + 20, 100, 100);

        // Create label showing the screen number
        QLabel *label = new QLabel(QString::number(i + 1), overlay);
        label->setStyleSheet("color: white; font-size: 40px;"); // Set text color and size
        label->setAlignment(Qt::AlignCenter);

        QVBoxLayout *layout = new QVBoxLayout(overlay);
        layout->addWidget(label);
        overlay->setLayout(layout);

        overlay->show();
        monitorNumberOverlays.append(overlay); // Keep track of the overlays to remove later

        overlay->setScreen(screen);
    }
}

void SettingsDialog::hideMonitorNumberOverlays()
{
    qDebug() << "Hide monitor number overlays";

    // Remove all the overlays
    for (QWidget *overlay : std::as_const(monitorNumberOverlays)) {
        overlay->hide();
        delete overlay;
    }
    monitorNumberOverlays.clear();
}

void SettingsDialog::sortLanguageComboBox(QComboBox *comboBox)
{
    QList<QPair<QString, QVariant>> items;

    // Collect all items
    for (int i = 0; i < comboBox->count(); ++i) {
        items.append(qMakePair(comboBox->itemText(i), comboBox->itemData(i)));
    }

    // Sort items alphabetically by text
    std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });

    // Clear and repopulate comboBox
    comboBox->clear();
    for (const auto &item : items) {
        comboBox->addItem(item.first, item.second);
    }
}

void SettingsDialog::onOpenConfigButtonClicked()
{
    QString configFilePath = Settings::getKfxConfigFile().fileName();

    qDebug() << "Trying to open KeeperFX settings config file:" << configFilePath;

    // Open file using OS functionality
    QDesktopServices::openUrl(QUrl::fromLocalFile(configFilePath));
}

void SettingsDialog::on_pushButtonShowLauncherParams_clicked()
{
    // Get original Qt help text of the command line options
    QString help = LauncherOptions::parser.helpText();

    // Modal dialog
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Launcher command line options", "MessageBox Title"));
    dlg.setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    // Monospaced font
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QFontMetrics fm(mono);

    // Measure longest line
    int maxWidth = 0;
    const QStringList lines = help.split('\n');
    for (const QString &line : lines) {
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(line));
    }

    // Measure total height for lines
    int totalHeight = fm.lineSpacing() * lines.size();

    // Add plain text viewer box
    QPlainTextEdit *edit = new QPlainTextEdit(&dlg);
    edit->setReadOnly(true);
    edit->setPlainText(help);
    edit->setFont(mono);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(edit);

    // Add close button
    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    // Manual sizing based on content
    // Add reasonable padding for margins + scrollbar
    const int paddingW = 50;
    const int paddingH = 100;
    int targetWidth = maxWidth + paddingW;
    int targetHeight = totalHeight + paddingH;

    // Limit to available screen space
    QSize screen = this->screen()->availableGeometry().size();
    targetWidth = qMin(targetWidth, screen.width() - 150);
    targetHeight = qMin(targetHeight, screen.height() - 150);

    // Set messagebox size
    dlg.resize(targetWidth, targetHeight);

    // Show messagebox
    dlg.exec();
}

void SettingsDialog::onResolutionChanged(int index)
{
    // Get sender combobox
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo) {
        return;
    }

    // Check if selected item is the custom resolution
    if (combo->itemData(index).toString() != "CUSTOM") {
        combo->setProperty("previousIndex", index);
        return;
    }

    showCustomResolutionDialog(combo);
}

void SettingsDialog::showCustomResolutionDialog(QComboBox *sourceCombo)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Custom resolution", "Custom resoltuion dialog title"));

    auto *layout = new QGridLayout(&dialog);

    auto *labelWidth  = new QLabel(tr("Width:", "Custom resolution dialog label"), &dialog);
    auto *labelHeight = new QLabel(tr("Height:", "Custom resolution dialog label"), &dialog);

    auto *editWidth  = new QLineEdit(&dialog);
    auto *editHeight = new QLineEdit(&dialog);

    editWidth->setValidator(new QIntValidator(1, 65535, editWidth));
    editHeight->setValidator(new QIntValidator(1, 65535, editHeight));

    layout->addWidget(labelWidth,  0, 0);
    layout->addWidget(editWidth,  0, 1);
    layout->addWidget(labelHeight, 1, 0);
    layout->addWidget(editHeight, 1, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
        );

    layout->addWidget(buttons, 2, 0, 1, 2);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    int previousIndex = sourceCombo->property("previousIndex").toInt();

    if (dialog.exec() != QDialog::Accepted) {
        sourceCombo->setCurrentIndex(previousIndex != -1 ? previousIndex : sourceCombo->findData(QString("MATCH_DESKTOP")));
        return;
    }

    int width  = editWidth->text().toInt();
    int height = editHeight->text().toInt();

    if (width <= 0 || height <= 0) {
        sourceCombo->setCurrentIndex(previousIndex != -1 ? previousIndex : sourceCombo->findData(QString("MATCH_DESKTOP")));
        return;
    }

    addCustomResolution(width, height, sourceCombo);
}

void SettingsDialog::addCustomResolution(int width, int height, QComboBox *sourceCombo)
{
    const QString value = QString("%1x%2").arg(width).arg(height);
    const QString text  = tr("%1 x %2 (Custom)", "Resolution Dropdown").arg(width).arg(height);

    QList<QComboBox *> combos = {
        ui->comboBoxResolution1,
        ui->comboBoxResolution2,
        ui->comboBoxResolution3,
        ui->comboBoxResolution1_2,
        ui->comboBoxResolution2_2,
        ui->comboBoxResolution3_2,
    };

    for (QComboBox *combo : combos) {
        if (combo->findData(value) == -1) {
            combo->addItem(text, value);
        }
    }

    // Select only in the combo that triggered it
    if(sourceCombo){
        int index = sourceCombo->findData(value);
        if (index != -1) {
            sourceCombo->setCurrentIndex(index);
            sourceCombo->setProperty("previousIndex", index);
        }
    }
}
