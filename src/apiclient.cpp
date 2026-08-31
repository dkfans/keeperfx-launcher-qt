#include "apiclient.h"

#include "launcheroptions.h"
#include "cdn.h"

#include <QEventLoop>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequestFactory>

#define API_ENDPOINT "https://keeperfx.net/api"

QString ApiClient::getApiEndpoint()
{
    // Check if custom endpoint is set
    if (LauncherOptions::isSet("api-endpoint")) {
        return LauncherOptions::getValue("api-endpoint");
    }

    // Return default endpoint
    return QString(API_ENDPOINT);
}

QJsonDocument ApiClient::getJsonResponse(QUrl endpointPath, HttpMethod method, QJsonObject jsonPostObject)
{
    // Strip '/api' and slashes from the endpoint path
    QString endpointPathString = endpointPath.toString();
    if (endpointPathString.startsWith("/api")) {
        endpointPathString.remove(0, 4);
    }
    if (endpointPathString.startsWith("/")) {
        endpointPathString.remove(0, 1);
    }

    // Create full URL for logging
    QString endpointUrlString = ApiClient::getApiEndpoint() + "/" + endpointPathString;
    qDebug() << "ApiClient:" << (method == HttpMethod::GET ? "GET" : "POST") << endpointUrlString;

    // Setup network manager and API
    QNetworkAccessManager manager;
    QNetworkRequest apiRequest(QUrl(ApiClient::getApiEndpoint() + "/" + endpointPathString));
    apiRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Create the network reply object
    QNetworkReply *reply = nullptr;
    if (method == HttpMethod::GET) {
        reply = manager.get(apiRequest);
    } else if (method == HttpMethod::POST) {
        QJsonDocument jsonPostDoc(jsonPostObject);
        reply = manager.post(apiRequest, jsonPostDoc.toJson());
    }

    // Create an event loop to wait for the request to finish
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();  // Block until the request is finished

    // Check for errors
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "ApiClient [ERROR]" << endpointUrlString << "->" << reply->errorString();
        reply->deleteLater();
        return QJsonDocument();  // Return an empty QJsonDocument on error
    }

    // We retrieved something!
    qDebug() << "ApiClient:" << endpointUrlString << "-> Success";

    // Read the response and parse it as JSON
    QByteArray response = reply->readAll();
    reply->deleteLater();

    return QJsonDocument::fromJson(response);
}

QJsonObject ApiClient::getLatestStable(){

    // URL of the API endpoint
    // API endpoints can be found at: https://github.com/dkfans/keeperfx-website
    QUrl url("v1/release/stable/latest");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        return QJsonObject();
    }

    // Convert response and return
    QJsonObject jsonObj = jsonDoc.object();
    return jsonObj["release"].toObject();
}

QJsonObject ApiClient::getLatestAlpha(){

    // URL of the API endpoint
    // API endpoints can be found at: https://github.com/dkfans/keeperfx-website
    QUrl url("v1/release/alpha/latest");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        return QJsonObject();
    }

    // Convert response and return
    QJsonObject jsonObj = jsonDoc.object();
    return jsonObj["alpha_build"].toObject();
}

QUrl ApiClient::getDownloadUrlStable()
{
    // Get JSON object from API
    QJsonObject releaseObj = getLatestStable();
    if(releaseObj.isEmpty()){
        return QUrl();
    }

    // Get download URL
    QString downloadUrlString = releaseObj["download_url"].toString();
    qDebug() << "Stable Download URL:" << downloadUrlString;

    // Return
    return QUrl(downloadUrlString);
}

QUrl ApiClient::getDownloadUrlAlpha()
{
    // Get JSON object from API
    QJsonObject releaseObj = getLatestAlpha();
    if(releaseObj.isEmpty()){
        return QUrl();
    }

    // Get the default download URL
    QString downloadUrlString = releaseObj["download_url"].toString();

    // Check if there is a CDN download URL
    if(releaseObj.contains("download_url_cdn")){
        downloadUrlString = CDN::getEndpoint() + releaseObj["download_url_cdn"].toString();
    }

    // Log download URL
    qDebug() << "Alpha Download URL:" << downloadUrlString;

    // Return
    return QUrl(downloadUrlString);
}

QUrl ApiClient::getDownloadUrlMusic()
{
    // URL of the API endpoint
    // API endpoints can be found at: https://github.com/dkfans/keeperfx-website
    QUrl url("v1/workshop/item/393");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        qWarning() << "Download music URL: Invalid response";
        return QUrl();
    }

    // Convert response
    QJsonObject jsonObj = jsonDoc.object();

    // Get workshop item obj
    QJsonObject workshopItemObj = jsonObj["workshop_item"].toObject();
    if (workshopItemObj.isEmpty()) {
        qWarning() << "Download music URL: Workshop item object not found";
        return QUrl();
    }

    // Get files obj
    QJsonArray filesArray = workshopItemObj["files"].toArray();
    if (filesArray.isEmpty()) {
        qWarning() << "Download music URL: Files array not found";
        return QUrl();
    }

    // Get first file
    QJsonObject fileObj = filesArray[0].toObject();
    if (fileObj.isEmpty()) {
        qWarning() << "Download music URL: First file object not found";
        return QUrl();
    }

    // Get URL
    QString fileDownloadString = fileObj["url"].toString();
    if (fileDownloadString.isEmpty() || fileDownloadString.isNull()) {
        qWarning() << "Download music URL: File download string not found";
        return QUrl();
    }

    qDebug() << "Download music URL:" << fileDownloadString;

    // Return
    return QUrl(fileDownloadString);
}

std::optional<QMap<QString, QString>> ApiClient::getGameFileList(KfxVersion::ReleaseType type,
                                                                 QString version)
{
    // Get type as string
    QString typeString;
    if (type == KfxVersion::ReleaseType::STABLE) {
        typeString = "stable";
    } else if (type == KfxVersion::ReleaseType::ALPHA) {
        typeString = "alpha";
    } else {
        return std::nullopt;
    }

    // Get URL
    QUrl url("v1/release/" + typeString + "/" + version + "/files");

    // Get the JSON response
    QJsonDocument jsonDoc = ApiClient::getJsonResponse(url);
    if (jsonDoc.isObject() == false) {
        return std::nullopt;
    }

    // Convert to JSON object
    QJsonObject jsonObj = jsonDoc.object();

    // Make sure object is valid
    if (jsonObj["success"].toBool() != true || jsonObj["release_type"].toString() != typeString
        || jsonObj["version"].toString() != version || jsonObj.contains("files") == false) {
        return std::nullopt;
    }

    // Create path -> checksum map
    QMap<QString, QString> map;

    // Loop trough all files
    QJsonObject fileObj = jsonObj["files"].toObject();
    foreach (const QString &path, fileObj.keys()) {
        // Add to map
        map[path] = fileObj.value(path).toString();
    }

    return map;
}
