#include "cdn.h"
#include "settings.h"

#include <QCoreApplication>
#include <QString>

QString CDN::endPoint;

QList<std::pair<QString, CDN::EndpointInfo>> CDN::getEndpointList()
{
    return {
        // clang-format off
        {"keeperfx.net",    {tr("KeeperFX.net (Default, Germany)",      "Download Server"),         "https://keeperfx.net"}},
        {"cloudflare",      {tr("Cloudflare CDN (Worldwide)",           "Download Server"),         "https://cdn-cf1.keeperfx.net"}},
        // clang-format on
    };
}

QString CDN::getEndpoint()
{
    // Check if endpoint is already loaded
    if (CDN::endPoint.isEmpty() == false) {
        return CDN::endPoint;
    }

    // Load the default endpoint as fallback
    CDN::endPoint = CDN_DEFAULT_ENDPOINT;

    // Get the user chosen CDN endpoint
    QString savedKey = Settings::getLauncherSetting("CDN_ENDPOINT").toString();

    // Look if user chosen CDN endpoint exists and load it
    for (const auto& [key, info] : CDN::getEndpointList()) {
        if (key == savedKey) {
            CDN::endPoint = info.url;
            break;
        }
    }

    // Make sure there is no trailing slash
    if(CDN::endPoint.endsWith("/")){
        CDN::endPoint.chop(1);
    }

    return CDN::endPoint;
}

void CDN::setEndpoint(QString url) {
    CDN::endPoint = url;
}