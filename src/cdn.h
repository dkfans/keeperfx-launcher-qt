#pragma once

#include <QCoreApplication>
#include <QMap>
#include <QString>

#define CDN_DEFAULT_ENDPOINT "https://keeperfx.net"

class CDN {

    Q_DECLARE_TR_FUNCTIONS(CDN);

public:

    struct EndpointInfo {
        QString name;
        QString url;
    };

    static QList<std::pair<QString, EndpointInfo>> getEndpointList();
    static QString getEndpoint();
    static void setEndpoint(QString url);

private:

    static QString endPoint;

};
