#include "kfxversion.h"
#include "apiclient.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QJsonObject>

#define MIN_VERSION_NEW_CONFIG

// Structure: {"<functionality name>", {"<stable version>", "<alpha version>"}}
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
    {"vsync",                                   {"",      "1.4.0.5295"}},
    {"relative_mouse_mode_toggle",              {"",      "1.4.0.5300"}},
    {"rotate_around_mouse",                     {"",      "1.4.0.5323"}},
    {"capture_cursor_config_option",            {"",      "1.4.0.5332"}},
    {"matchmaking_server",                      {"",      "1.4.0.5339"}},
    {"multiplayer_port",                        {"",      "1.4.0.5350"}},
    {"opengl_renderer",                         {"",      "1.4.0.5389"}},
    {"viewport_mode",                           {"",      "1.4.0.5391"}},
    {"map_fade_animation",                      {"",      "1.4.0.5415"}},
    {"packetsave_max_filesize",                 {"",      "1.4.0.5416"}},



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

/**
 * Get the ProductVersion string of a Windows binary.
 *
 * @brief KfxVersion::getVersionString
 * @param filePath
 * @author Yani, Gemini
 * @return
 */
QString KfxVersion::getVersionString(const QString& filePath){

    QFile file(filePath);

    // Make sure the file exists
    if(file.exists() == false){
        qWarning() << "Trying to get product version of nonexistent file:" << filePath;
        return QString();
    }

    // Get filepath
    qDebug() << "Grabbing product version of file:" << filePath;

    // Try to open the file
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    // Read the whole binary into memory
    // It should be fine because it's unloaded after the check anyways
    QByteArray data = file.readAll();

    // Construct the UTF-16 Little-Endian search key for "ProductVersion\0"
    QString keyString = "ProductVersion";
    QByteArray searchKey;
    for (QChar c : keyString) {
        searchKey.append(static_cast<char>(c.unicode() & 0xFF));
        searchKey.append(static_cast<char>((c.unicode() >> 8) & 0xFF));
    }
    // Append the UTF-16 null terminator (0x0000) that follows the key in the struct
    searchKey.append('\0');
    searchKey.append('\0');

    // Locate the key in the binary data
    int index = data.indexOf(searchKey);
    if (index == -1) {
        return {}; // Key not found
    }

    // Move the index past the search key
    index += searchKey.size();

    // Skip 32-bit alignment padding
    // The PE format aligns the next struct member (the value) to a 32-bit boundary.
    // This usually manifests as two zero-bytes padding if the string wasn't naturally aligned.
    while (index + 1 < data.size() && data.at(index) == '\0' && data.at(index + 1) == '\0') {
        index += 2;
    }

    // Read the target value string
    QString productVersion;
    while (index + 1 < data.size()) {
        // Reconstruct the 16-bit character from Little-Endian bytes
        ushort c = (static_cast<uchar>(data.at(index + 1)) << 8) | static_cast<uchar>(data.at(index));

        if (c == 0) {
            break; // We hit the null terminator of the value string
        }

        productVersion.append(QChar(c));
        index += 2;
    }

    return productVersion.trimmed();
}

QString KfxVersion::getVersionStringFromAppDir()
{
    return KfxVersion::getVersionString(
        QCoreApplication::applicationDirPath() + "/keeperfx.exe"
    );
}

KfxVersion::VersionInfo KfxVersion::getVersionFromString(QString versionString)
{
    VersionInfo versionInfo;
    versionInfo.fullString = versionString;

    // Use regex to get the version from the string
    // Catches 1.2.3 and 1.2.3.4
    QRegularExpression regex (R"([0-9]+\.[0-9]+\.[0-9]+(\.[0-9]+)?)");
    QRegularExpressionMatch match = regex.match(versionString);

    // Check if regex has a match
    if (match.hasMatch() == false) {
        return versionInfo;
    }

    // Get version
    versionInfo.version = match.captured(0);

    // Get the type of the release
    if (match.captured(1) == ".0") {
        versionInfo.type = KfxVersion::ReleaseType::DEVELOPMENT;
    } else if (versionInfo.version == versionString) {
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

    // Log the type
    qDebug() << "Release type:" << versionString << "->" << versionInfo.type;

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
    // Make sure the feature is in the map
    if (!versionFunctionaltyMap.contains(functionalityString)) {
        qWarning() << "Invalid functionality check:" << functionalityString;
        return false;
    }

    // Get the version from the functionality map based on release type
    auto versions = versionFunctionaltyMap.value(functionalityString);
    QString targetVersion = (currentVersion.type == STABLE) ? versions.first : versions.second;

    // Make sure the functionality is available for this release type
    if(targetVersion.isEmpty()){
        return false;
    }

    // If release type is UNKNOWN or PROTOTYPE at this point, return true
    if (currentVersion.type != ReleaseType::STABLE && currentVersion.type != ReleaseType::ALPHA) {
        return true;
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
