#include "cdn.h"
#include "settings.h"
#include "apiclient.h"
#include "launcheroptions.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>

QString CDN::endPoint;
QString CDN::activeKeyOrUrl;
QString CDN::suggestedEndpointKey;

void CDN::refreshEndpointCache()
{
    QUrl url("v1/cdn/endpoints");
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);

    if (!jsonDoc.isObject()) {
        return; // Silent fail
    }

    QJsonObject jsonObj = jsonDoc.object();
    QJsonObject endpointsObj = jsonObj["endpoints"].toObject();

    if (endpointsObj.isEmpty()) {
        return; // Silent fail
    }

    // Save to the specifically named INI file
    QString configPath = QCoreApplication::applicationDirPath() + "/keeperfx-launcher-qt.cdn.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    settings.clear(); // Clear old endpoints before writing new ones

    for (auto it = endpointsObj.constBegin(); it != endpointsObj.constEnd(); ++it) {
        QString key = it.key();
        QJsonObject ep = it.value().toObject();
        QJsonObject loc = ep["location"].toObject();

        settings.beginGroup(key);
        settings.setValue("name", ep["name"].toString());
        settings.setValue("url", ep["url"].toString());
        settings.setValue("location_code", loc["code"].toString());
        settings.setValue("location_name", loc["name"].toString());
        settings.endGroup();
    }

    settings.sync();

    // Cache the suggested endpoint key in memory
    QString suggested = jsonObj["suggested_endpoint"].toString();
    if (!suggested.isEmpty() && endpointsObj.contains(suggested)) {
        CDN::suggestedEndpointKey = suggested;
    }
}

QString CDN::getSuggestedCdn()
{
    // Fetch cache from API if it hasn't been loaded into memory yet
    if (CDN::suggestedEndpointKey.isEmpty()) {
        CDN::refreshEndpointCache();
    }

    // Default fallback if API fails
    if (CDN::suggestedEndpointKey.isEmpty()) {
        CDN::suggestedEndpointKey = "keeperfx.net";
    }

    return CDN::suggestedEndpointKey;
}

QMap<QString, CdnEndpointInfo> CDN::getEndpoints()
{
    QMap<QString, CdnEndpointInfo> result;

    QString configPath = QCoreApplication::applicationDirPath() + "/keeperfx-launcher-qt.cdn.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    const QStringList groups = settings.childGroups();
    for (const QString& group : groups) {
        settings.beginGroup(group);
        CdnEndpointInfo info;
        info.name = settings.value("name").toString();
        info.url = settings.value("url").toString();
        info.locationCode = settings.value("location_code").toString();
        info.locationName = settings.value("location_name").toString();
        settings.endGroup();

        result.insert(group, info);
    }

    // Silently fail with a single fallback if the INI file is missing/empty
    if (result.isEmpty()) {
        CdnEndpointInfo fallback;
        fallback.name = "KeeperFX.net";
        fallback.url = "https://keeperfx.net";
        fallback.locationCode = "DE";
        fallback.locationName = "Germany";
        result.insert("keeperfx.net", fallback);
    }

    return result;
}

QMap<QString, QString> CDN::getEndpointList()
{
    QMap<QString, QString> uiList;
    auto endpoints = CDN::getEndpoints();

    // Add named endpoints from the INI file
    for (auto it = endpoints.constBegin(); it != endpoints.constEnd(); ++it) {
        uiList.insert(it.key(), it.value().displayString());
    }

    // Include custom URL if currently active
    QString activeKey = CDN::getActiveEndpointKey();
    if (!activeKey.isEmpty() && !endpoints.contains(activeKey)) {
        if (activeKey.startsWith("http://") || activeKey.startsWith("https://")) {
            uiList.insert(activeKey, activeKey);
        }
    }

    return uiList;
}

QString CDN::resolveUrlFromKeyOrUrl(const QString &keyOrUrl)
{
    QMap<QString, CdnEndpointInfo> endpoints = CDN::getEndpoints();

    if (endpoints.contains(keyOrUrl)) {
        return endpoints.value(keyOrUrl).url;
    }

    if (keyOrUrl.startsWith("http://") || keyOrUrl.startsWith("https://")) {
        return keyOrUrl; // Raw custom URL
    }

    // Fallback if key was removed from server
    return endpoints.contains("keeperfx.net") ? endpoints.value("keeperfx.net").url : "https://keeperfx.net";
}

void CDN::loadEndpoint()
{
    // Check if custom CDN endpoint is set
    if (LauncherOptions::isSet("cdn-endpoint")) {
        CDN::endPoint = LauncherOptions::getValue("cdn-endpoint");
        return;
    }

    QString savedKey = Settings::getLauncherSetting("CDN_ENDPOINT").toString();
    if (savedKey.isEmpty()) {
        savedKey = "keeperfx.net";
    }

    CDN::setActiveEndpoint(savedKey);
}

QString CDN::getEndpoint()
{
    if (CDN::endPoint.isEmpty()) {
        CDN::loadEndpoint();
    }
    return CDN::endPoint;
}

QString CDN::getActiveEndpointKey()
{
    if (CDN::activeKeyOrUrl.isEmpty()) {
        CDN::loadEndpoint();
    }
    return CDN::activeKeyOrUrl;
}

void CDN::setActiveEndpoint(const QString &keyOrUrl)
{
    CDN::activeKeyOrUrl = keyOrUrl;
    CDN::endPoint = CDN::resolveUrlFromKeyOrUrl(keyOrUrl);

    // Make sure there is no trailing slash
    if (CDN::endPoint.endsWith("/")) {
        CDN::endPoint.chop(1);
    }
}

void CDN::saveActiveEndpoint()
{
    if (!CDN::activeKeyOrUrl.isEmpty()) {
        Settings::setLauncherSetting("CDN_ENDPOINT", CDN::activeKeyOrUrl);
    }
}

void CDN::saveEndpoint(const QString &keyOrUrl)
{
    CDN::setActiveEndpoint(keyOrUrl);
    CDN::saveActiveEndpoint();
}

QString CDN::getCurrentEndpointDisplayName()
{
    QString activeKey = CDN::getActiveEndpointKey();
    QMap<QString, CdnEndpointInfo> endpoints = CDN::getEndpoints();

    if (endpoints.contains(activeKey)) {
        return endpoints.value(activeKey).displayString();
    }

    // Custom URL or override
    return CDN::getEndpoint();
}