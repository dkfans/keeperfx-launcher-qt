#include "installkfxdialog.h"
#include "ui_installkfxdialog.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QScrollBar>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrent>

#include "apiclient.h"
#include "archiver.h"
#include "downloader.h"
#include "launcheroptions.h"
#include "settings.h"
#include "translator.h"
#include "extractor.h"
#include "helper.h"

InstallKfxDialog::InstallKfxDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::InstallKfxDialog)
{
    ui->setupUi(this);

    // Turn this dialog into a normal window
    setWindowFlags(Qt::Window
                   | Qt::WindowTitleHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowCloseButtonHint
                   | Qt::MSWindowsFixedSizeDialogHint
                   | Qt::WindowStaysOnTopHint // Always on top
        );

    // Fixed size, portable across Wayland/X11/Windows
    setFixedSize(size());

    // Move to center of primary screen
    QRect geometry = QGuiApplication::primaryScreen()->geometry();
    move(geometry.left() + (geometry.width() - width()) / 2, geometry.top() + (geometry.height() - height()) / 2 - 75); // move 75 pixels up

    // Bring to foreground
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();

    // Release dropdown
    ui->versionComboBox->addItem(tr("Stable (Default)", "Game Release Build"), "STABLE");
    ui->versionComboBox->addItem(tr("Alpha", "Game Release Build"), "ALPHA");
    ui->versionComboBox->setCurrentIndex(ui->versionComboBox->findData("STABLE"));

    // Setup signals and slots
    connect(this, &InstallKfxDialog::appendLog, this, &InstallKfxDialog::onAppendLog);
    connect(this, &InstallKfxDialog::clearProgressBar, this, &InstallKfxDialog::onClearProgressBar);
    connect(this, &InstallKfxDialog::setInstallFailed, this, &InstallKfxDialog::onInstallFailed);

    connect(this, &InstallKfxDialog::updateProgressBar, ui->progressBar, &QProgressBar::setValue);
    connect(this, &InstallKfxDialog::setProgressMaximum, ui->progressBar, &QProgressBar::setMaximum);
    connect(this, &InstallKfxDialog::setProgressBarFormat, ui->progressBar, &QProgressBar::setFormat);

    // Log the install path at the start
    emit appendLog(QString("Install path: %1").arg(QCoreApplication::applicationDirPath()));
}

InstallKfxDialog::~InstallKfxDialog()
{
    delete ui;
}

void InstallKfxDialog::on_installButton_clicked()
{
    // Change GUI
    ui->installButton->setDisabled(true);
    ui->progressBar->setTextVisible(true);

    // Tell user we start the installation
    emit appendLog("Installation started");

    // Reset settings
    Settings::resetKfxSettings();
    Settings::resetLauncherSettings();

    // If for some reason a '-new' launcher exists we'll remove it
    Helper::removeLeftoverNewLauncher();

    // Get release type to install
    // Also remember that we want this release version for later updates
    if (ui->versionComboBox->currentData() == "STABLE") {
        this->installReleaseType = KfxVersion::ReleaseType::STABLE;
        Settings::setLauncherSetting("CHECK_FOR_UPDATES_RELEASE", "STABLE");
    } else if (ui->versionComboBox->currentData() == "ALPHA") {
        this->installReleaseType = KfxVersion::ReleaseType::ALPHA;
        Settings::setLauncherSetting("CHECK_FOR_UPDATES_RELEASE", "ALPHA");
    }

    // Set launcher language to installation language
    if (LauncherOptions::isSet("language")) {
        QString languageCode = LauncherOptions::getValue("language").toLower();
        if (languageCode == "en") {
            Settings::setLauncherSetting("LAUNCHER_LANGUAGE", "en");
        } else {
            // Create dummy translator to check if this language works
            Translator *translator = new Translator;
            if (translator->loadLanguage(languageCode)) {
                Settings::setLauncherSetting("LAUNCHER_LANGUAGE", languageCode);
            }
        }
    }

    // Start the installation process
    // Even for the alpha release, we first need the stable
    startStableDownload();
}

void InstallKfxDialog::startStableDownload()
{
    emit appendLog("Getting download URL for stable release...");
    downloadUrlStable = ApiClient::getDownloadUrlStable();

    if (downloadUrlStable.isEmpty()) {
        emit appendLog("Failed to get download URL for stable release");
        emit setInstallFailed(tr("Failed to get download URL for stable release", "Failure Message"));
        return;
    }

    emit appendLog(QString("Stable release URL: %1").arg(downloadUrlStable.toString()));

    QString outputFilePath = QCoreApplication::applicationDirPath() + "/" + downloadUrlStable.fileName() + ".tmp";
    tempArchiveStable = new QFile(outputFilePath);

    Downloader *downloader = new Downloader(this);
    connect(downloader, &Downloader::downloadProgress, this, &InstallKfxDialog::updateProgressBarDownload);
    connect(downloader, &Downloader::downloadCompleted, this, &InstallKfxDialog::onStableDownloadFinished);

    downloader->download(downloadUrlStable, tempArchiveStable);
}

void InstallKfxDialog::onStableDownloadFinished(bool success)
{
    if (!success) {
        emit appendLog("Failed to download stable release");
        emit setInstallFailed(tr("Failed to download stable release", "Failure Message"));
        return;
    }

    emit appendLog("KeeperFX stable release successfully downloaded");
    emit clearProgressBar();

    // Test archive
    emit appendLog("Testing stable release archive...");
    QThreadPool::globalInstance()->start([this]() {
        uint64_t archiveSize = Archiver::testArchiveAndGetSize(this->tempArchiveStable);
        QMetaObject::invokeMethod(this, "onStableArchiveTestComplete", Qt::QueuedConnection, Q_ARG(uint64_t, archiveSize));
    });
}

void InstallKfxDialog::onStableArchiveTestComplete(uint64_t archiveSize) {

    // Make sure test is successful and archive size is valid
    if (archiveSize < 0) {
        emit appendLog("Stable release archive test failed");
        emit setInstallFailed(tr("Stable release archive test failed. It may be corrupted.", "Failure message"));
        return;
    }

    // Get size
    double archiveSizeInMiB = static_cast<double>(archiveSize) / (1024 * 1024);
    QString archiveSizeString = QString::number(archiveSizeInMiB,
                                                'f',
                                                2); // Format to 2 decimal places
    emit appendLog(QString("Total size: %1MiB").arg(archiveSizeString));

    // Start extraction process
    emit setProgressMaximum(static_cast<int>(archiveSize));
    emit setProgressBarFormat(tr("Extracting: %p%", "Progress bar"));
    emit appendLog("Extracting...");

    // Set temp directory
    QString dirHash = QCryptographicHash::hash(downloadUrlStable.fileName().toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    tempDirStable = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kfx-install-" + dirHash);
    emit appendLog(QString("Temp directory path: %1").arg(tempDirStable.absolutePath()));

    // Make temp directory
    if (!tempDirStable.exists()) {
        tempDirStable.mkpath(".");
    }

    // Make sure temp directory exists now
    if (!tempDirStable.exists()) {
        emit appendLog("Failed to create temp directory");
        emit setInstallFailed(tr("Failed to create temp directory", "Failure message"));
        return;
    }

    // Create extractor and connect signals
    Extractor *extractor = new Extractor(this);
    connect(extractor, &Extractor::progress, this, &InstallKfxDialog::updateProgressBar);
    connect(extractor, &Extractor::extractComplete, this, &InstallKfxDialog::onStableExtractComplete);
    connect(extractor, &Extractor::extractFailed, this, &InstallKfxDialog::setInstallFailed);

    // Start extraction
    extractor->extract(tempArchiveStable, tempDirStable.absolutePath());
}

void InstallKfxDialog::onStableExtractComplete()
{
    emit appendLog("Extraction completed");
    emit clearProgressBar();

    // Remove temp archive
    emit appendLog("Removing temporary archive");
    if (tempArchiveStable->exists()) {
        tempArchiveStable->remove();
    }

    // Move temp files to app dir
    if(this->moveTempFilesToAppDir(tempDirStable) == false){
        return;
    }

    emit appendLog("All files are copied");
    emit clearProgressBar();

    // Remove temp dir
    if(tempDirStable.removeRecursively() == false){
        emit appendLog("Failed to remove temp dir");
    }

    // Move to alpha download if user wants it
    if (this->installReleaseType == KfxVersion::ReleaseType::ALPHA) {
        startAlphaDownload();
        return;
    }

    // Complete the installation
    // This will update some settings, show a messagebox and close the window
    completeInstall();
}

void InstallKfxDialog::startAlphaDownload()
{
    emit appendLog("Getting download URL for alpha patch...");
    downloadUrlAlpha = ApiClient::getDownloadUrlAlpha();

    if (downloadUrlAlpha.isEmpty()) {
        emit appendLog("Failed to get download URL for alpha patch");
        emit setInstallFailed(tr("Failed to get download URL for alpha patch", "Failure message"));
        return;
    }

    emit appendLog(QString("Alpha patch URL: %1").arg(downloadUrlAlpha.toString()));

    QString outputFilePath = QCoreApplication::applicationDirPath() + "/" + downloadUrlAlpha.fileName() + ".tmp";
    tempArchiveAlpha = new QFile(outputFilePath);

    Downloader *downloader = new Downloader(this);
    connect(downloader, &Downloader::downloadProgress, this, &InstallKfxDialog::updateProgressBarDownload);
    connect(downloader, &Downloader::downloadCompleted, this, &InstallKfxDialog::onAlphaDownloadFinished);

    downloader->download(downloadUrlAlpha, tempArchiveAlpha);
}

void InstallKfxDialog::onAlphaDownloadFinished(bool success)
{
    if (!success) {
        emit appendLog("Alpha patch download failed");
        emit setInstallFailed(tr("Alpha patch download failed.", "Failure message"));
        return;
    }

    emit appendLog("KeeperFX alpha patch successfully downloaded");

    // Test archive
    emit appendLog("Testing alpha patch archive...");
    QThreadPool::globalInstance()->start([this]() {
        uint64_t archiveSize = Archiver::testArchiveAndGetSize(this->tempArchiveAlpha);
        QMetaObject::invokeMethod(this, "onAlphaArchiveTestComplete", Qt::QueuedConnection, Q_ARG(uint64_t, archiveSize));
    });
}

void InstallKfxDialog::onAlphaArchiveTestComplete(uint64_t archiveSize)
{
    // Make sure test is successful and archive size is valid
    if (archiveSize < 0) {
        emit appendLog("Alpha patch archive test failed");
        emit setInstallFailed(tr("Alpha patch archive test failed. It may be corrupted.", "Failure Message"));
        return;
    }

    // Get size
    double archiveSizeInMiB = static_cast<double>(archiveSize) / (1024 * 1024);
    QString archiveSizeString = QString::number(archiveSizeInMiB, 'f', 2); // Format to 2 decimal places
    emit appendLog(QString("Total size: %1MiB").arg(archiveSizeString));

    // Start extraction process
    emit setProgressMaximum(static_cast<int>(archiveSize));
    emit setProgressBarFormat(tr("Extracting: %p%", "Progress bar"));
    emit appendLog("Extracting...");

    // Create temp dir
    QString dirHash = QCryptographicHash::hash(downloadUrlAlpha.fileName().toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    tempDirAlpha = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kfx-install-alpha-" + dirHash);
    emit appendLog(QString("Temp directory path: %1").arg(tempDirAlpha.absolutePath()));

    // Make temp directory
    if (!tempDirAlpha.exists()) {
        tempDirAlpha.mkpath(".");
    }

    // Make sure temp directory exists now
    if (!tempDirAlpha.exists()) {
        emit appendLog("Failed to create temp directory");
        emit setInstallFailed(tr("Failed to create temp directory", "Failure message"));
        return;
    }

    Extractor *extractor = new Extractor(this);
    connect(extractor, &Extractor::progress, this, &InstallKfxDialog::updateProgressBar);
    connect(extractor, &Extractor::extractComplete, this, &InstallKfxDialog::onAlphaExtractComplete);
    connect(extractor, &Extractor::extractFailed, this, &InstallKfxDialog::setInstallFailed);

    extractor->extract(tempArchiveAlpha, tempDirAlpha.absolutePath());
}

void InstallKfxDialog::onAlphaExtractComplete()
{
    emit appendLog("Extraction completed");
    emit clearProgressBar();

    // Remove temp archive
    emit appendLog("Removing temporary archive");
    if (tempArchiveAlpha->exists()) {
        tempArchiveAlpha->remove();
    }

    // Move temp files to app dir
    if(this->moveTempFilesToAppDir(tempDirAlpha) == false){
        return;
    }

    emit appendLog("All files are copied");
    emit clearProgressBar();

    // Remove temp dir
    if(tempDirAlpha.removeRecursively() == false){
        emit appendLog("Failed to remove temp dir");
    }

    // Complete the installation
    // This will update some settings, show a messagebox and close the window
    completeInstall();
}

void InstallKfxDialog::updateProgressBarDownload(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        ui->progressBar->setMaximum(static_cast<int>(bytesTotal / 1024 / 1024));
        ui->progressBar->setValue(static_cast<int>(bytesReceived / 1024 / 1024));
        ui->progressBar->setFormat(tr("Downloading: %p% (%vMiB)", "Progress bar"));
    }
}

void InstallKfxDialog::onAppendLog(const QString &string)
{
    // Log to debug output
    qDebug() << "Install log:" << string;

    // Set the cursor to the end
    ui->logTextArea->moveCursor(QTextCursor::End);

    // Add string to log with timestamp
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timestampString = currentDateTime.toString("HH:mm:ss");
    ui->logTextArea->insertPlainText("[" + timestampString + "] " + string + "\n");

    // Scroll to the left on new text
    QScrollBar *hScrollBar = ui->logTextArea->horizontalScrollBar();
    if (hScrollBar) {
        hScrollBar->setValue(hScrollBar->minimum());
    }

    // Scroll to the bottom on new text
    QScrollBar *vScrollBar = ui->logTextArea->verticalScrollBar();
    if (vScrollBar) {
        vScrollBar->setValue(vScrollBar->maximum());
    }

    // Force redraw
    QApplication::processEvents();
}

void InstallKfxDialog::onClearProgressBar()
{
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(1);
    ui->progressBar->setFormat("");
}

void InstallKfxDialog::onInstallFailed(const QString &reason)
{
    ui->installButton->setDisabled(false);
    onClearProgressBar();
    QMessageBox::warning(this, tr("Installation failed", "MessageBox Title"), reason);
}

void InstallKfxDialog::on_versionComboBox_currentIndexChanged(int index)
{
    if (ui->versionComboBox->currentData() == "ALPHA") {
        int result = QMessageBox::question(this,
                                           tr("KeeperFX Alpha builds", "MessageBox Title"),
                                           tr("Alpha patches may contain new features and changes, but often contain "
                                              "bugs and unfinished functionality. These patches are mostly meant "
                                              "for testers, and it is suggested to be part of the Keeper Klan discord "
                                              "if you use them.\n\nAre you sure you want to use Alpha patches?",
                                              "MessageBox Text"));

        // Check whether or not user is sure
        if (result != QMessageBox::Yes) {
            // Set back to Stable if they are not sure
            ui->versionComboBox->setCurrentIndex(ui->versionComboBox->findData("STABLE"));
        }
    }
}

void InstallKfxDialog::on_cancelButton_clicked()
{
    close();
}

void InstallKfxDialog::closeEvent(QCloseEvent *event)
{
    // Check if we have a windows build
#ifdef Q_OS_WINDOWS
    bool isWindows = true;
#else
    bool isWindows = false;
#endif

    // Get uninstaller binary path
    QFile uninstallerPath(QCoreApplication::applicationDirPath() + "/unins000.exe");

    // Check if --install is passed on Windows and an uninstaller is present
    if (isWindows && LauncherOptions::isSet("install") && uninstallerPath.exists()) {
        // Ask if user wants to start uninstaller
        int result = QMessageBox::question(this,
                                           tr("Uninstall KeeperFX", "MessageBox Title"),
                                           tr("Do you want to uninstall KeeperFX?\n\nThis will remove any leftover files.", "MessageBox Text"));

        // Handle answer
        if (result == QMessageBox::Yes) {
            QProcess::startDetached(uninstallerPath.fileName(), LauncherOptions::getArguments());
        }

        // Exit the installer
        QTimer::singleShot(0, []() { QCoreApplication::exit(1); });
        return;

    } else {

        // Check if KeeperFX is already installed
        if (Helper::isKeeperFxInstalled() == true){
            // Close the installer
            event->accept();
            return;
        }

        // User will not be able to play KeeperFX at this point,
        // so ask if they are sure about stopping the installation
        int result = QMessageBox::question(this, tr("Confirmation", "MessageBox Title"), tr("Are you sure?\n\nYou will be unable to play KeeperFX.", "MessageBox Text"));
        if (result == QMessageBox::Yes) {
            // Close the installer
            event->accept();
        } else {
            // Keep the installer open
            event->ignore();
        }
    }
}

bool InstallKfxDialog::moveTempFilesToAppDir(QDir sourceDir)
{
    // Define rename rules
    QMap<QString, QString> renameRules = {
        {"keeperfx.cfg", "_keeperfx.cfg"},
        {"keeperfx-launcher-qt.exe", "keeperfx-launcher-qt-new.exe"},
        {"7z.dll", "7z-new.dll"},
        {"7za.dll", "7za-new.dll"},
    };

    // Count total files to copy (recursive)
    int totalFiles = Helper::countFilesRecursive(sourceDir);

    // Tell user we're copying
    emit appendLog(QString("Copying %1 files from temp dir").arg(totalFiles));

    // Set progress bar
    emit setProgressBarFormat(tr("Copying: %p%", "Progress bar"));
    emit setProgressMaximum(totalFiles);

    // Vars
    QDir appDir(QCoreApplication::applicationDirPath());
    int copiedFiles = 0;

    qDebug() << "Source copy dir:" << sourceDir.absolutePath();

    // Iterate recursively
    QDirIterator it(sourceDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {

        QString srcFilePath = it.next();
        QFileInfo info(srcFilePath);

        // Build relative path for destination
        QString relPath = sourceDir.relativeFilePath(srcFilePath);   // e.g. "data/file.txt"

        // Apply rename rule
        QString destRelPath = renameRules.value(relPath, relPath);
        if (destRelPath != relPath) {
            qDebug() << QString("Renaming file during copy: %1 -> %2").arg(relPath, destRelPath);
        }

        QString destFilePath = appDir.absoluteFilePath(destRelPath);

        // Ensure destination directory exists
        QFileInfo destInfo(destFilePath);
        if (QDir().mkpath(destInfo.absolutePath()) == false) {
            emit appendLog(QString("Failed to create destination directory: %1").arg(destInfo.absolutePath()));
            emit setInstallFailed(tr("Failed to create destination directory: %1", "Failure Message").arg(destInfo.absolutePath()));
            return false;
        }

        // Remove existing destination
        QFile destFile(destFilePath);
        if (destFile.exists()){
            qDebug() << "Removing existing file in appdir:" << destFilePath;
            destFile.remove();
        }

        // Move file
        if (!QFile::rename(srcFilePath, destFilePath)) {
            emit appendLog("Failed to move file: " + srcFilePath);
            emit setInstallFailed(tr("Failed to move file: %1").arg(srcFilePath));
            return false;
        }

        //emit appendLog("Moved: " + relPath);
        emit updateProgressBar(++copiedFiles);
    }

    return true;
}

void InstallKfxDialog::completeInstall()
{
    // Load the current KeeperFX version
    // We need it for automatically setting settings depending on the KFX version
    KfxVersion::loadCurrentVersion();

    // Handle any settings update
    emit appendLog("Loading settings");
    Settings::load();

    // Set game language
    emit appendLog("Setting game language to system language");
    Settings::autoSetGameLanguageToLocaleLanguage();

    // Set max fps
    if (KfxVersion::hasFunctionality("max_frames_per_second") == true) {
        emit appendLog("Setting max FPS to screen refresh rate");
        if(Settings::autoSetMaxFpsToScreenRefreshRate() == true){
            emit appendLog(QString("Max FPS set to: %1").arg(Settings::getKfxSetting("FRAMES_PER_SECOND").toString()));
        } else {
            if (KfxVersion::hasFunctionality("auto_determine_monitor_refresh_rate") == true) {
                Settings::setKfxSetting("FRAMES_PER_SECOND", "AUTO 0");
                emit appendLog(QString("Max FPS set to: AUTO 0"));
            }
        }
    }

    // Done!
    emit appendLog("Done!");
    QMessageBox::information(this, "KeeperFX", tr("KeeperFX has been successfully installed!", "MessageBox Text"));
    accept();
}
