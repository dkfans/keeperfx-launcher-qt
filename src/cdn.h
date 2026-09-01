#pragma once

#include <QCoreApplication>
#include <QSettings>
#include <QMap>
#include <QString>

struct CdnEndpointInfo {
    QString name;
    QString url;
    QString locationCode;
    QString locationName;

    // Helper for displaying in UI dropdowns and installer logs
    QString displayString() const {
        if (locationCode == "XX") {
            return url; // Show raw URL for local dev / unknown
        }
        return QString("%1 (%2)").arg(name, locationName);
    }
};

class CDN {
    Q_DECLARE_TR_FUNCTIONS(CDN);

        public:
    // Fetches from API, saves to INI, and caches the suggested key in memory
    static void refreshEndpointCache();

    // Returns the cached suggested CDN key, fetching it first if empty
    static QString getSuggestedCdn();

    // Reads the INI file and returns the parsed structs
    static QMap<QString, CdnEndpointInfo> getEndpoints();

    // Returns a map formatted for UI dropdowns (Key -> Display String)
    static QMap<QString, QString> getEndpointList();

    // Returns the active URL for downloads
    static QString getEndpoint();

    // Returns the currently active key or custom URL string in memory
    static QString getActiveEndpointKey();

    // Sets the active endpoint in memory ONLY (does not write to settings file)
    static void setActiveEndpoint(const QString &keyOrUrl);

    // Saves the currently active in-memory endpoint to the settings file
    static void saveActiveEndpoint();

    // Convenience function: sets active in memory AND saves to settings file
    static void saveEndpoint(const QString &keyOrUrl);

    // Returns the formatted display name of the active CDN
    static QString getCurrentEndpointDisplayName();

        private:
    static QString endPoint;           // Cached active URL
    static QString activeKeyOrUrl;     // Current key or custom URL in memory
    static QString suggestedEndpointKey;

    // Internal resolution helpers
    static void loadEndpoint();
    static QString resolveUrlFromKeyOrUrl(const QString &keyOrUrl);
};