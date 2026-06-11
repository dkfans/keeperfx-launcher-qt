#include "launcheroptions.h"

QCommandLineParser LauncherOptions::parser;
QStringList LauncherOptions::arguments;
QStringList LauncherOptions::activeOptions;

bool LauncherOptions::isSet(const QString option)
{
    return LauncherOptions::parser.isSet(option);
}

QString LauncherOptions::getValue(const QString option)
{
    return LauncherOptions::parser.value(option);
}

QStringList LauncherOptions::getArguments()
{
    return LauncherOptions::arguments;
}

void LauncherOptions::removeArgumentOption(QString option)
{
    LauncherOptions::arguments.erase(std::remove_if(LauncherOptions::arguments.begin(),
                                                    LauncherOptions::arguments.end(),
                                                    [option](const QString &arg) { return arg == QString("--" + option) || arg.startsWith(QString("--" + option + "=")); }),
                                     LauncherOptions::arguments.end());
}

void LauncherOptions::processApp(QApplication &app)
{
    // Set up the command line parser
    LauncherOptions::parser.setApplicationDescription("KeeperFX Launcher");
    LauncherOptions::parser.addHelpOption();
    LauncherOptions::parser.addVersionOption();

    // Define options
    // clang-format off
    QVector<QCommandLineOption> options = {

        // Toggle options
        {"disable-logfile",             "Do not log the debug output of the launcher to a file"},
        {"skip-verify",                 "Skip the certificate verification process"},
        {"install",                     "Start the KeeperFX install procedure"},
        {"skip-launcher-update",        "Do not update the launcher itself"},
        {"log-missing-translations",    "Log missing translations to debug"},
        {"log-missing-qt-translations", "Log missing Qt core translations to debug"},
        {"no-image-cache",              "Bypass image caching"},
        {"download-music",              "Start the music download procedure"},
        {"skip-file-removal",           "Do not ask for the removal of leftover files"},
        {"disable-gzip-upload",         "Disable GZip compression of uploads"},
        {"crash-report",                "Force a crash report dialog"},
        {"disable-tls-verification",    "Disable certificate validation for web requests"},

        // Parameters
        {"api-endpoint",                "Specify the API endpoint",                    "url"},
        {"translation-file",            "Force a PO translation file to be loaded",    "filepath"},
        {"language-file",               "Force a PO translation file to be loaded",    "filepath"}, // same as 'translation-file'
        {"language",                    "Force a language to be loaded",               "language code"},
    };
    // clang-format on

    // Add options to the parser
    for (const auto &option : options) {
        parser.addOption(option);
    }

    // Process
    parser.process(app);

    // Get active options
    for (const QCommandLineOption &option : options) {
        const QStringList names = option.names();
        QString name = !names.isEmpty() ? names.first() : QStringLiteral("<unnamed>");
        if (parser.isSet(option)) {
            if (!option.valueName().isEmpty()) {
                QString value = parser.value(option);
                activeOptions << "--" + name + "=" + value;
            } else {
                activeOptions << "--" + name;
            }
        }
    }

    // Remember app arguments
    LauncherOptions::arguments = app.arguments();
    LauncherOptions::arguments.removeFirst(); // remove executable path
}
