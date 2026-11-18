#include "game.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include "crashdialog.h"
#include "kfxversion.h"
#include "settings.h"

Game::Game(QWidget *parent)
    : QObject(parent)
    , process(new QProcess(this))
    , parentWidget(parent)
{
    // Setup the process
    process->setWorkingDirectory(QApplication::applicationDirPath());

    // Connect the process finished signal to the onProcessFinished slot
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &Game::onProcessFinished);
    /*connect(process, &QProcess::readyReadStandardOutput, this, &Game::handleProcessOutput);
    connect(process, &QProcess::readyReadStandardError, this, &Game::handleProcessOutput);*/

}

QString Game::getStringFromStartType(StartType startType)
{
    switch (startType) {
    case NORMAL:
        return "Normal";
    case HVLOG:
        return "Heavy Log";
    case DIRECT_CONNECT:
        return "Direct Connect";
    case MAP:
        return "Map";
    case CAMPAIGN:
        return "Campaign";
    case LOAD_SAVE:
        return "Load Save";
    case LOAD_REPLAY:
        return "Load replay";
    }
    return "Unknown start type";
}

bool Game::start(StartType startType, QVariant data1, QVariant data2, QVariant data3)
{
    // Refresh error
    this->errorString = QString();

    // Log some stuff
    qInfo() << "Setting up game for start";
    qInfo() << "Start type:" << Game::getStringFromStartType(startType);
    qDebug() << "Data[1]" << data1.toString();
    qDebug() << "Data[2]" << data2.toString();
    qDebug() << "Data[3]" << data3.toString();

    // Get the game parameters
    QStringList params = Settings::getGameSettingsParameters();

    // If version is too old for custom -config path we'll use the default 'keeperfx.cfg' instead
    if (KfxVersion::hasFunctionality("absolute_config_path") == true) {
        // Add '-config' parameter with our custom keeperfx.cfg
        QFileInfo configFileInfo(Settings::getKfxConfigFile());
        params << "-config" << QDir::toNativeSeparators(configFileInfo.absoluteFilePath());
    } else {
        qWarning() << "Game version too old for custom config path";
    }

    // Campaign
    if (startType == StartType::CAMPAIGN) {
        params << "-campaign" << data1.toString();
    }

    // Direct Connect (MP)
    if (startType == StartType::DIRECT_CONNECT) {
        params << "-connect" << QString(data1.toString() + ":" + data2.toString());
    }

    // Load replay file
    if (startType == StartType::LOAD_REPLAY) {
        params << "-packetload" << QString(data1.toString());
    }

    // Start without mods
    if (startType == StartType::START_WITHOUT_MODS) {
        params << "-nomods";
    }

    // Log parameters
    if (params.count() > 0) {
        qInfo() << "Game parameters:" << params.join(" ");
    } else {
        qInfo() << "No game parameters set";
    }

    // Get the game binary
    // For now it's only the .exe release
    QString keeperfxBin;
    if (Settings::getLauncherSetting("GAME_HEAVY_LOG_ENABLED").toBool() == true) {
        keeperfxBin = QApplication::applicationDirPath() + "/keeperfx_hvlog.exe";
    } else {
        keeperfxBin = QApplication::applicationDirPath() + "/keeperfx.exe";
    }

    // Start the process
    this->startTime = QDateTime::currentDateTime();
    #ifdef Q_OS_WINDOWS
        qInfo() << "Starting game (Windows)";
        process->start(keeperfxBin, params);
    #else
        if (qEnvironmentVariableIsSet("FLATPAK_ID")) {
            qInfo() << "Starting game (Linux: Flatpak -> Wine)";
            // Run Wine outside Flatpak
            params.prepend(keeperfxBin);
            params.prepend("wine");
            params.prepend("--host");
            process->start("flatpak-spawn", params);
        } else {
            qInfo() << "Starting game (Linux: Wine)";
            // Normal Wine execution
            params.prepend(keeperfxBin);
            process->start("wine", params);
        }
    #endif

    // Log the full command line
    qDebug() << "Full command:" << process->program() + " " + process->arguments().join(" ");

    // Wait for process to start and check errors
    if (!process->waitForStarted()) {
        this->errorString = process->errorString();
        qDebug() << "Error: Process failed to start:" << process->errorString();
        return false;
    }

    return true;
}

void Game::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Check for crash
    if (exitStatus == QProcess::ExitStatus::CrashExit || exitCode != 0) {

        // Check if crash reporting is enabled
        if(Settings::getLauncherSetting("CRASH_REPORTING_ENABLED").toBool() == true){

            // Check if heavy log is enabled
            if(Settings::getLauncherSetting("GAME_HEAVY_LOG_ENABLED").toBool() == true){

                // Tell user we crashed but will not submit heavylog logs
                QMessageBox::warning(this->parentWidget,
                    tr("KeeperFX has crashed", "MessageBox Title"),
                    tr("It seems that KeeperFX has crashed and is unable to recover.\n\n"
                       "Crash reporting is disabled because you are using the heavylog option.\n\n"
                        "If you require assistance, please create an issue on Github or contact us on the Keeper Klan Discord.", "MessageBox Text"));

            } else {

                // Create Crash Report dialog
                CrashDialog crashDialog(this->parentWidget);

                // Check if there is process output and add it to the dialog object
                QString stdErrorString = this->process->readAllStandardError();
                if (stdErrorString.isEmpty() == false) {
                    qDebug() << "Process StdError:" << stdErrorString;
                    crashDialog.setStdErrorString(stdErrorString);
                }

                // Add the exit code and this run's crash dump (if the game wrote one)
                crashDialog.setGameRun(exitCode, this->startTime);

                // Open the crash dialog
                crashDialog.exec();
            }
        }
    }

    emit gameEnded(exitCode, exitStatus);
}

QString Game::getErrorString()
{
    return this->errorString;
}
