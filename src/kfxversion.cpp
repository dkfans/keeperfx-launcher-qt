#include "kfxversion.h"
#include "apiclient.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QJsonObject>

#include <LIEF/PE.hpp>

#define MIN_VERSION_NEW_CONFIG

const QMap<QString, QPair<QString, QString>> KfxVersion::versionFunctionaltyMap = {

    // 1.1
    {"player_colors_purple_orange_black",       {"1.1.0", "1.0.0.3729"}},

    // 1.3 (build >= 4144)
    {"splash_screen_ordering",                  {"1.3.0", "1.2.0.4427"}},
    {"startup_config_option",                   {"1.3.0", "1.2.0.4427"}},
    {"ukrainian_game_language",                 {"1.3.0", "1.2.0.4471"}},
    {"exit_on_lua_error",                       {"1.3.0", "1.2.0.4479"}},
    {"save_file_struct_lua",                    {"1.3.0", "1.2.0.4479"}},
    {"portuguese_game_language",                {"1.3.0", "1.2.0.4625"}},
    {"max_frames_per_second",                   {"1.3.0", "1.2.0.4653"}},
    {"mod_support",                             {"1.3.0", "1.2.0.4658"}},
    {"flee_imprison_defaults",                  {"1.3.0", "1.2.0.4681"}},
    {"tag_mode",                                {"1.3.0", "1.2.0.4714"}},
    {"gui_and_neutral_blink_speed",             {"1.3.0", "1.2.0.4529"}},
    {"start_without_mods_param",                {"1.3.0", "1.2.0.4753"}},

    // 1.4 (build >= 4771)
    {"auto_determine_monitor_refresh_rate",     {"1.4.0", "1.3.0.4793"}},
    {"enet_ipv6_support",                       {"1.4.0", "1.3.1.4877"}},
    {"save_file_struct_30_char_name",           {"1.4.0", "1.3.1.4881"}},
    {"mouse_sensitivity_no_multiplier",         {"1.4.0", "1.3.2.5120"}},

    // 1.5 (build >= 5127)
    {"zoom_towards_mouse",                      {"",      "1.3.2.5134"}},



    // Absolute Config path is temporary disabled because we still want support for multiple KFX installations
    {"absolute_config_path", {"", ""}}, // '-config' absolute path was added in 1.2.0.4408

    // Not yet supported
    {"start_campaign_directly", {"", ""}},     // https://github.com/dkfans/keeperfx/issues/3924
    {"load_save_directly", {"", ""}},          // TODO: https://github.com/dkfans/keeperfx/issues/3481
    {"packetsave_while_packetload", {"", ""}}, // TODO

    // Use configuration files in the user appdata
    // Temporary disabled until KeeperFX can also handle this
    {"use_appdata_configs", {"", ""}}, // TODO
};

KfxVersion::VersionInfo KfxVersion::currentVersion;

QString KfxVersion::getVersionString(const QFile& binary){

    // Make sure this binary file exists
    if(binary.exists() == false){
        return QString();
    }

    // Get filepath
    QString filePath = binary.fileName();
    qDebug() << "Checking app version for file:" << filePath;

    // Parse the PE file using LIEF
    // We use a library here instead of Windows calls to be consistent accross platforms
    // TODO: When we release a unix version of KeeperFX we should parse ELF data instead
    //       But for now we use the Windows binary with Wine so we should just read the PE data
    std::unique_ptr<LIEF::PE::Binary> peFile = LIEF::PE::Parser::parse(filePath.toStdString());

    // Check if PE file parser is made
    if (!peFile) {
        qDebug() << "Error: Unable to parse PE file:" << filePath;
        return QString();
    }

    // Check if the PE file has resources
    if (!peFile->has_resources()) {
        qDebug() << "Error: No resources found in the PE file.";
        return QString();
    }

    // Get the resource manager
    auto manager = peFile->resources_manager();
    if(!manager){
        qDebug() << "Error: Failed to get PE file resource manager.";
        return QString();
    }

    // Get the versions
    auto versions = manager->version();
    if (versions.empty()) {
        qDebug() << "Error: No version information found in resources.";
        return QString();
    }

    // Capture the first version entry (assuming there's at least one)
    std::ostringstream oss;
    oss << versions.front(); // Get the first ResourceVersion

    // Get as string
    QString resourcesInfo = QString::fromStdString(oss.str());

    // Make sure string is not empty
    if(resourcesInfo.isEmpty()){
        return QString();
    }

    // Define regex pattern to match 'ProductVersion'
    QRegularExpression regex (R"(ProductVersion:\s*(.+?)\s*?[\\\n])");
    QRegularExpressionMatch match = regex.match(resourcesInfo);

    // Get regex match
    if (match.hasMatch()) {
        qDebug() << "Grabbed ProductVersion" << match.captured(1) << "from" << filePath;
        return match.captured(1);
    } else {
        qWarning() << "Error: Version not found in PE file";
    }

    return QString();
}

QString KfxVersion::getVersionStringFromAppDir()
{
    return KfxVersion::getVersionString(
        QFile(QCoreApplication::applicationDirPath() + "/keeperfx.exe")
    );
}

KfxVersion::VersionInfo KfxVersion::getVersionFromString(QString versionString)
{
    VersionInfo versionInfo;
    versionInfo.fullString = versionString;

    // Use regex to get the version from the string
    // Catches 1.2.3 and 1.2.3.4
    QRegularExpression regex (R"([0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?)");
    QRegularExpressionMatch match = regex.match(versionString);

    // Check if regex has a match
    if (match.hasMatch() == false) {
        return versionInfo;
    }

    // Get version
    versionInfo.version = match.captured(0);

    // Get the type of the release
    if (versionInfo.version == versionString) {
        versionInfo.type = KfxVersion::ReleaseType::STABLE;
        // Make sure only x.y.z are taken from stable version
        versionInfo.version = versionInfo.version.split('.').mid(0, 3).join('.');
        versionInfo.fullString = versionInfo.version;
    } else if (versionString.toLower().contains("alpha")) {
        versionInfo.type = KfxVersion::ReleaseType::ALPHA;
    } else if (versionString.toLower().contains("prototype")) {
        versionInfo.type = KfxVersion::ReleaseType::PROTOTYPE;
    } else {
        versionInfo.type = KfxVersion::ReleaseType::UNKNOWN;
    }

    return versionInfo;
}

bool KfxVersion::loadCurrentVersion()
{
    // Get the version
    QString versionString = getVersionStringFromAppDir();
    VersionInfo version = getVersionFromString(versionString);

    // Check if version is valid
    if(version.type == ReleaseType::UNKNOWN){
        return false;
    }

    // Remember the current Kfx version
    currentVersion = version;

    return true;
}

bool KfxVersion::isVersionLowerOrEqual(const QString &version1, const QString &version2) {

    // Get version parts
    QStringList version1Parts = version1.split(".");
    QStringList version2Parts = version2.split(".");

    // Normalize version parts to equal sizes
    int maxLength = qMax(version1Parts.size(), version2Parts.size());
    while (version1Parts.size() < maxLength) version1Parts.append("0");
    while (version2Parts.size() < maxLength) version2Parts.append("0");

    // Loop trough the version parts
    for (int i = 0; i < maxLength; ++i) {

        // Check if version is newer or older
        if (version1Parts[i].toInt() < version2Parts[i].toInt()) {
            return true;
        } else if (version1Parts[i].toInt() > version2Parts[i].toInt()) {
            return false;
        }
    }

    // Versions are equal
    return true;
}

bool KfxVersion::isVersionHigherOrEqual(const QString &version1, const QString &version2)
{
    // Get version parts
    QStringList version1Parts = version1.split(".");
    QStringList version2Parts = version2.split(".");

    // Normalize version parts to equal sizes
    int maxLength = qMax(version1Parts.size(), version2Parts.size());
    while (version1Parts.size() < maxLength)
        version1Parts.append("0");
    while (version2Parts.size() < maxLength)
        version2Parts.append("0");

    // Loop trough the version parts
    for (int i = 0; i < maxLength; ++i) {
        // Check if version is newer or older
        if (version1Parts[i].toInt() > version2Parts[i].toInt()) {
            return true;
        } else if (version1Parts[i].toInt() < version2Parts[i].toInt()) {
            return false;
        }
    }

    // Versions are equal
    return true;
}

bool KfxVersion::isNewerVersion(const QString &version1, const QString &version2)
{
    // Get version parts
    QStringList version1Parts = version1.split(".");
    QStringList version2Parts = version2.split(".");

    // Normalize version parts to equal sizes
    int maxLength = qMax(version1Parts.size(), version2Parts.size());
    while (version1Parts.size() < maxLength) version1Parts.append("0");
    while (version2Parts.size() < maxLength) version2Parts.append("0");

    // Loop trough the version parts
    for (int i = 0; i < maxLength; ++i) {

        // Check if version is newer
        if (version1Parts[i].toInt() > version2Parts[i].toInt()) {
            return true;
        }
    }

    return false;
}

bool KfxVersion::checkIfAlphaUpdateNeedsNewStable(const QString &version1, const QString &version2)
{
    // Split versions
    QStringList v1 = version1.split(".");
    QStringList v2 = version2.split(".");

    // Compare only first 3 version parts (major, minor, patch)
    for (int i = 0; i < 3; ++i)
    {
        // Check if they are different
        if (v1[i].toInt() != v2[i].toInt()) {
            return true;
        }
    }

    return false;
}

std::optional<KfxVersion::VersionInfo> KfxVersion::getLatestVersion(KfxVersion::ReleaseType type)
{
    // Only check version for stable and alpha
    if (type != KfxVersion::ReleaseType::STABLE && type != KfxVersion::ReleaseType::ALPHA) {
        return std::nullopt;
    }

    // Variables
    QString version;
    QString downloadUrl;
    QString fullVersionString;

    // Handle stable
    if (type == KfxVersion::ReleaseType::STABLE) {
        // Get release
        QJsonObject stableRelease = ApiClient::getLatestStable();
        if (stableRelease.isEmpty()) {
            return std::nullopt;
        }

        // Set vars
        version = stableRelease["version"].toString();
        downloadUrl = stableRelease["download_url"].toString();
        fullVersionString = version;
    }

    // Handle alpha
    if (type == KfxVersion::ReleaseType::ALPHA) {
        // Get release
        QJsonObject alphaRelease = ApiClient::getLatestAlpha();
        if (alphaRelease.isEmpty()) {
            return std::nullopt;
        }

        // Set vars
        version = alphaRelease["version"].toString();
        downloadUrl = alphaRelease["download_url"].toString();
        fullVersionString = version + " Alpha";
    }

    // Return latest version information
    return VersionInfo{
        .type = type,
        .version = version,
        .fullString = fullVersionString,
        .downloadUrl = downloadUrl
    };
}

std::optional<QMap<QString, QString>> KfxVersion::getGameFileMap(KfxVersion::ReleaseType type, QString version)
{
    // Only check version for stable and alpha
    if (type != KfxVersion::ReleaseType::STABLE && type != KfxVersion::ReleaseType::ALPHA) {
        return std::nullopt;
    }

    // Get release file map
    auto fileMap = ApiClient::getGameFileList(type, version);
    if (!fileMap) {
        return std::nullopt;
    }

    return fileMap;
}

bool KfxVersion::hasFunctionality(QString functionalityString)
{
    // If type is UNKNOWN or PROTOTYPE, always return true
    if (currentVersion.type != ReleaseType::STABLE && currentVersion.type != ReleaseType::ALPHA) {
        return true;
    }

    if (!versionFunctionaltyMap.contains(functionalityString)) {
        qWarning() << "Invalid functionality check:" << functionalityString;
        return false; // Feature not in map
    }

    // Get the version from the functionality map based on release type
    auto versions = versionFunctionaltyMap.value(functionalityString);
    QString targetVersion = (currentVersion.type == STABLE) ? versions.first : versions.second;

    // Make sure the functionality is available for this release type
    if(targetVersion.isEmpty()){
        return false;
    }

    // Get version parts
    QStringList version1Parts = currentVersion.version.split(".");
    QStringList version2Parts = targetVersion.split(".");

    // Normalize version parts to equal sizes
    int maxLength = qMax(version1Parts.size(), version2Parts.size());
    while (version1Parts.size() < maxLength)
        version1Parts.append("0");
    while (version2Parts.size() < maxLength)
        version2Parts.append("0");

    // Loop trough the version parts
    for (int i = 0; i < maxLength; ++i) {

        // Skip patch version (<major>.<minor>.<PATCH>.<build>)
        // We do this so the launcher does not think a patch has any
        // new functionality that was introduced after the latest minor version.
        // For example:
        // - 1.3.0
        // - 1.3.0.1 -> bugfix
        // - 1.3.0.2 -> new functionality
        // - 1.3.1 -> only contains the bugfix from 1.3.0.1
        if(maxLength >= 2 && i == 2){
            continue;
        }

        // Check if version is newer or older
        if (version1Parts[i].toInt() > version2Parts[i].toInt()) {
            return true;
        } else if (version1Parts[i].toInt() < version2Parts[i].toInt()) {
            return false;
        }
    }

    // Versions are equal
    return true;
}
