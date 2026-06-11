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
    if (CDN::endPoint.isEmpty()) {

        CDN::endPoint = "https://keeperfx.net";
        QString savedKey = Settings::getLauncherSetting("CDN_ENDPOINT").toString();

        for (const auto& [key, info] : CDN::getEndpointList()) {
            if (key == savedKey) {
                CDN::endPoint = info.url;
                break;
            }
        }

        if(CDN::endPoint.endsWith("/")){
            CDN::endPoint.chop(1);
        }
    }

    return CDN::endPoint;
}