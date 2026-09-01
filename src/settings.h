#pragma once

#include <QFile>
#include <QString>
#include <QSettings>

class Settings
{
public:
    static QVariant getKfxSetting(QAnyStringView key);
    static void setKfxSetting(QAnyStringView key, const QVariant &value);
    static void removeKfxSetting(QAnyStringView key);

    static QVariant getLauncherSetting(QAnyStringView key);
    static void setLauncherSetting(QAnyStringView key, const QVariant &value);

    static QFile getKfxConfigFile();

    static void load();

    static QStringList getGameSettingsParameters();

    static bool autoSetGameLanguageToLocaleLanguage();
    static bool autoSetMaxFpsToScreenRefreshRate(QWidget *widget = nullptr);

    static void resetKfxSettings();
    static void resetLauncherSettings();

private:
    static QMap<QString, QVariant> defaultLauncherSettingsMap;
    static QMap<QString, QString> gameSettingsParameterMap;
    static QMap<QString, QString> localeToGameLanguageMap;

    static QStringList forceStringKfxSettings;
    static QStringList trueStrings;
    static QStringList falseStrings;

    static QSettings *kfxSettings;
    static QSettings *launcherSettings;

    static void copyMissingSettings(QSettings *fromSettingsFile, QSettings *toSettingsFile);
    static void copyMissingDefaultSettings();
    static void copyMissingAlphaSettings();
    static void copyMissingLauncherSettings();
};
