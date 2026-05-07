#include "settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QScreen>
#include <QSettings>
#include <QWidget>
#include <QWindow>

#include "kfxversion.h"
#include "settingscfgformat.h"

QSettings *Settings::kfxSettings;
QSettings *Settings::launcherSettings;

// clang-format off

QMap<QString, QVariant> Settings::defaultLauncherSettingsMap = {

    // Launcher settings
    // "CHECK_FOR_UPDATES_RELEASE" is hardcoded in 'copyMissingLauncherSettings()'
    {"CHECK_FOR_UPDATES_ENABLED", true},
    {"CHECK_FOR_UPDATES_INTERVAL_DAYS", 0},
    {"CHECK_FOR_UPDATES_LAST_TIMESTAMP", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
    {"AUTO_UPDATE", false},
    {"BACKUP_SAVES", false},
    {"WEBSITE_INTEGRATION_ENABLED", true},
    {"CRASH_REPORTING_ENABLED", true},
    {"CRASH_REPORTING_CONTACT", ""},
    {"OPEN_ON_GAME_SCREEN", false},
    {"GAME_HEAVY_LOG_ENABLED", false},
    {"PLAY_BUTTON_THEME", "dk-orange"},
    {"LAUNCHER_LANGUAGE", QLocale(QLocale::system().uiLanguages().value(0, QLocale::system().name())).name().left(2)}, // Get 2 letter language identifier
    {"SHOW_DIR_NAME_IN_WINDOW_TITLE", false},
    {"AUTO_REMOVE_LEFTOVER_FILES", false},

    // Stuff to remember
    {"SUPPRESS_ORIGINAL_DK_FOUND_MESSAGEBOX", false},

    // Game executable parameters
    // These also go in the launcher config
    {"GAME_PARAM_NO_SOUND", false},
    {"GAME_PARAM_USE_CD_MUSIC", false},
    {"GAME_PARAM_NO_INTRO", false},
    {"GAME_PARAM_ALEX", false},
    {"GAME_PARAM_FPS", "20"},
    {"GAME_PARAM_HUMAN_PLAYER", "0"},
    {"GAME_PARAM_VID_SMOOTH", false},
    {"GAME_PARAM_PACKET_SAVE_ENABLED", false},
    {"GAME_PARAM_PACKET_SAVE_FILE_NAME", "packetsave.pck"},
};

QMap<QString, QString> Settings::gameSettingsParameterMap = {
    {"GAME_PARAM_NO_SOUND", "-nosound"},
    {"GAME_PARAM_USE_CD_MUSIC", "-cd"},
    {"GAME_PARAM_ALEX", "-alex"},
    {"GAME_PARAM_VID_SMOOTH", "-vidsmooth"},
    // {"GAME_PARAM_FPS", "-fps %d"}, // Hardcoded
    // {"GAME_PARAM_HUMAN_PLAYER", "-human %d"}, // Hardcoded
    // {"GAME_PARAM_PACKET_SAVE_FILE_NAME", "-packetsave %s"}, // Hardcoded
    // {"GAME_PARAM_NO_INTRO", "-nointro"}, // Hardcoded
};

// clang-format on

QMap<QString, QString> Settings::localeToGameLanguageMap = {
    {"en", "ENG"},
    {"it", "ITA"},
    {"fr", "FRE"},
    {"es", "SPA"},
    {"nl", "DUT"},
    {"de", "GER"},
    {"pl", "POL"},
    {"sv", "SWE"},
    {"ja", "JPN"},
    {"ru", "RUS"},
    {"ko", "KOR"},
    {"cs", "CZE"},
    {"la", "LAT"},
    {"uk", "UKR"},
    {"pt", "POR"},
    {"zh-Hans", "CHI"}, // Simplified Chinese
    {"zh-Hant", "CHT"}, // Traditional Chinese
};

QVariant Settings::getKfxSetting(QAnyStringView key)
{
    // Get value as a string
    QVariant value = kfxSettings->value(key);
    QString valueString = value.toString();

    // Fix for 'RESIZE_MOVIES' KeeperFX setting
    // This setting is set to "ON" by default which would make this a boolean
    // However, ON should be a string here and is treated different than all the rest
    // Therefor we simply return the value as a string
    if (key.toString() == "RESIZE_MOVIES") {
        return valueString;
    }

    // True strings
    if (valueString == "ON" || valueString == "YES" || valueString == "TRUE") {
        return true;
    }

    // False strings
    if (valueString == "OFF" || valueString == "NO" || valueString == "FALSE") {
        return false;
    }

    // Return the string value directly
    return value;
}

void Settings::setKfxSetting(QAnyStringView key, const QVariant &value)
{
    if (value.typeId() == QMetaType::Type::Bool && value == true) {
        kfxSettings->setValue(key, "TRUE");
        return;
    }

    if (value.typeId() == QMetaType::Type::Bool && value == false) {
        kfxSettings->setValue(key, "FALSE");
        return;
    }

    kfxSettings->setValue(key, value);
}

void Settings::removeKfxSetting(QAnyStringView key)
{
    if(kfxSettings->contains(key)){
        kfxSettings->remove(key);
    }
}

QVariant Settings::getLauncherSetting(QAnyStringView key)
{
    return launcherSettings->value(key);
}

void Settings::setLauncherSetting(QAnyStringView key, const QVariant &value)
{
    launcherSettings->setValue(key, value);
}

QFile Settings::getKfxConfigFile()
{
    return QFile(kfxSettings->fileName());
}

void Settings::load()
{
    // Get the CFG format used by the original keeperfx.cfg
    QSettings::Format settingsCfgFormat = SettingsCfgFormat::registerFormat();

    // Check if we need to load config files from the user appdata config
    if(KfxVersion::hasFunctionality("use_appdata_configs")){

        qInfo() << "Using AppData configs";

        kfxSettings = new QSettings(settingsCfgFormat, QSettings::UserScope, "keeperfx", "keeperfx");
        launcherSettings = new QSettings(settingsCfgFormat, QSettings::UserScope, "keeperfx", "launcher");

        // Load default settings from config file in application directory
        // This is only done when we use config files in the appdata
        // Otherwise this file would be the one where we update our own settings
        copyMissingDefaultSettings();

    } else {

        kfxSettings = new QSettings(QCoreApplication::applicationDirPath() + "/keeperfx.cfg", settingsCfgFormat);
        launcherSettings = new QSettings(QCoreApplication::applicationDirPath() + "/keeperfx-launcher-qt.cfg", settingsCfgFormat);
    }

    // Copy missing alpha settings from the '_keeperfx.cfg' file
    copyMissingAlphaSettings();

    // Log the paths
    qInfo() << "KeeperFX Settings File:" << kfxSettings->fileName();
    qInfo() << "Launcher Settings File:" << launcherSettings->fileName();

    // Copy missing launcher settings
    Settings::copyMissingLauncherSettings();
}

void Settings::copyMissingSettings(QSettings *fromSettingsFile, QSettings *toSettingsFile)
{
    // File not loaded
    if (fromSettingsFile->allKeys().isEmpty()) {
        qDebug() << "New settings file not loaded. File not found or it contains no keys";
        return;
    }

    // Loop through all keys in the new settings file default 'keeperfx.cfg'
    foreach (const QString &key, fromSettingsFile->allKeys()) {
        // Get the value
        QVariant value = fromSettingsFile->value(key);
        QString valueString = value.toString();

        // Fix RESIZE_MOVIES
        if (key == "RESIZE_MOVIES") {
            if (valueString == "ON" || valueString == "YES" || valueString == "TRUE") {
                value = "FIT";
            }
        }

        // Check if user kfx settings misses this key
        if (!toSettingsFile->contains(key)) {
            // Copy the setting
            toSettingsFile->setValue(key, value);
            qDebug() << "Copied setting:" << key << "=" << value.toString();
        }
    }
}

void Settings::copyMissingDefaultSettings()
{
    // This is only useful when we use config files in the user appdata dir
    if(KfxVersion::hasFunctionality("use_appdata_configs") == false){
        return;
    }

    // Try and load default KFX settings from 'keeperfx.cfg' in the app dir
    QSettings *defaultKfxSettings = new QSettings(
        QCoreApplication::applicationDirPath() + "/keeperfx.cfg",
        SettingsCfgFormat::registerFormat()
    );

    copyMissingSettings(defaultKfxSettings, kfxSettings);
}

void Settings::copyMissingAlphaSettings()
{
    QString alphaSettingsFilePath(QCoreApplication::applicationDirPath() + "/_keeperfx.cfg");

    // Check if settings file exists
    QFile alphaSettingsFile(alphaSettingsFilePath);
    if(alphaSettingsFile.exists() == false){
        return;
    }

    // Copy new settings
    QSettings *newAlphaSettings = new QSettings(alphaSettingsFilePath, SettingsCfgFormat::registerFormat());
    copyMissingSettings(newAlphaSettings, kfxSettings);
}

void Settings::copyMissingLauncherSettings()
{
    // Loop trough default launcher settings
    for (auto it = defaultLauncherSettingsMap.begin(); it != defaultLauncherSettingsMap.end(); ++it) {

        // Check if we are missing this setting
        if (!launcherSettings->contains(it.key())) {

            // Copy the setting
            launcherSettings->setValue(it.key(), it.value());
            qDebug() << "Copied launcher setting from defaults:" << it.key() << "=" << it.value().toString();
        }
    }

    // Set correct release type path
    QString updateReleaseTypeKey("CHECK_FOR_UPDATES_RELEASE");
    QString updateReleaseTypeValue;
    if(!launcherSettings->contains(updateReleaseTypeKey)){

        // Get current version from KeeperFX binary
        if(KfxVersion::loadCurrentVersion() == true){
            updateReleaseTypeValue = KfxVersion::currentVersion.type == KfxVersion::ReleaseType::ALPHA ? "ALPHA" : "STABLE";
        } else {
            updateReleaseTypeValue = "STABLE";
        }

        // Set key
        launcherSettings->setValue(updateReleaseTypeKey, updateReleaseTypeValue);
        qDebug() << "Set launcher setting automatically:" << updateReleaseTypeKey << "=" << updateReleaseTypeValue;
    }
}

QStringList Settings::getGameSettingsParameters()
{
    QStringList paramList;

    // Loop trough parameters
    for (auto it = Settings::gameSettingsParameterMap.begin(); it != Settings::gameSettingsParameterMap.end(); ++it) {
        if (Settings::getLauncherSetting(it.key()).toBool() == true) {
            paramList << it.value();
        }
    }

    // Add no intro
    // We hardcode this one because it's different for game versions that have the STARTUP cfg option
    if (KfxVersion::hasFunctionality("startup_config_option") == false) {
        if (Settings::getLauncherSetting("GAME_PARAM_NO_INTRO").toBool() == true) {
            paramList << "-nointro";
        }
    }

    // Add FPS
    QString fps = Settings::getLauncherSetting("GAME_PARAM_FPS").toString();
    if (fps != Settings::defaultLauncherSettingsMap["GAME_PARAM_FPS"]) {
        paramList << "-fps" << fps;
    }

    // Add human player
    QString humanPlayer = Settings::getLauncherSetting("GAME_PARAM_HUMAN_PLAYER").toString();
    if (humanPlayer != Settings::defaultLauncherSettingsMap["GAME_PARAM_HUMAN_PLAYER"]) {
        paramList << "-human" << humanPlayer;
    }

    // Add packetsave
    if (Settings::getLauncherSetting("GAME_PARAM_PACKET_SAVE_ENABLED") == true) {
        QString packetSaveFileName = Settings::getLauncherSetting("GAME_PARAM_PACKET_SAVE_FILE_NAME").toString();
        if (packetSaveFileName.isEmpty() == false) {
            paramList << "-packetsave" << packetSaveFileName;
        }
    }

    return paramList;
}

bool Settings::autoSetGameLanguageToLocaleLanguage()
{
    // Get system locale language as string
    QString localeLanguage = QLocale::system().bcp47Name();

    // Check if we have this language in our map
    if (localeToGameLanguageMap[localeLanguage].isEmpty()) {
        return false;
    }

    // Update game language
    Settings::setKfxSetting("LANGUAGE", localeToGameLanguageMap[localeLanguage]);

    return true;
}

bool Settings::autoSetMaxFpsToScreenRefreshRate(QWidget *widget)
{
    // Make sure the current game version supports it
    if (KfxVersion::hasFunctionality("max_frames_per_second") == false) {
        return false;
    }

    QScreen *screen = nullptr;

    // Try to get screen from optionally passed widget
    if (widget && widget->windowHandle()){
        screen = widget->windowHandle()->screen();
    }

    // Otherwise try to get primary screen
    if(!screen){
        screen = QGuiApplication::primaryScreen();
    }

    // No screen found
    if(!screen){
        qWarning() << "Failed to find screen when trying to find refresh rate for automatically setting max fps";
        return false;
    }

    // Try to get refresh rate of screen
    int refreshRate = qRound(screen->refreshRate());
    if(refreshRate == 0){
        return false;
    }

    // Make sure we at least have the FPS of the default game speed
    if (refreshRate < 20) {
        refreshRate = 20;
    }

    // Let's also limit it to something reasonable
    if(refreshRate > 200) {
        refreshRate = 200;
    }

    // Add automatic monitor refresh rate determinitation built into KeeperFX
    if (KfxVersion::hasFunctionality("auto_determine_monitor_refresh_rate") == true) {
        Settings::setKfxSetting("FRAMES_PER_SECOND", QString("AUTO ") + QString::number(refreshRate));
        return true;
    }

    Settings::setKfxSetting("FRAMES_PER_SECOND", refreshRate);
    return true;
}

void Settings::resetKfxSettings()
{
    if (Settings::kfxSettings) {
        Settings::kfxSettings->clear();
        Settings::copyMissingDefaultSettings();
    }
}

void Settings::resetLauncherSettings()
{
    if (Settings::launcherSettings) {
        Settings::launcherSettings->clear();
        Settings::copyMissingLauncherSettings();
    }
}
