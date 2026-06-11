#pragma once

#include <QCoreApplication>
#include <QMap>
#include <QString>

class CDN {

    Q_DECLARE_TR_FUNCTIONS(CDN);

public:

    struct EndpointInfo {
        QString name;
        QString url;
    };

    static QList<std::pair<QString, EndpointInfo>> getEndpointList();
    static QString getEndpoint();

private:

    static QString endPoint;

};
