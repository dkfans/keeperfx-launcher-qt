#include "updatedialog.h"
#include "archiver.h"
#include "downloader.h"
#include "launcheroptions.h"
#include "savefile.h"
#include "settings.h"
#include "extractor.h"

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include <zlib.h>

#define GAME_FILE_BASE_URL "https://keeperfx.net/game-files"
#define AUTO_UPDATE_MESSAGEBOX_TIMER 2500

UpdateDialog::UpdateDialog(QWidget *parent, KfxVersion::VersionInfo versionInfo, bool autoUpdate)
    : QDialog(parent)
    , ui(new Ui::UpdateDialog)
    , networkManager(new QNetworkAccessManager(this))
{
    // Setup this UI
    ui->setupUi(this);
    this->originalTitleText = ui->titleLabel->text();

    // Disable resizing and remove maximize button
    setFixedSize(size());
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    // Connect signals to slots
    connect(this, &UpdateDialog::fileDownloadProgress, this, &UpdateDialog::onFileDownloadProgress);
    // GUI signals
    connect(this, &UpdateDialog::appendLog, this, &UpdateDialog::onAppendLog);
    connect(this, &UpdateDialog::clearProgressBar, this, &UpdateDialog::onClearProgressBar);
    connect(this, &UpdateDialog::setUpdateFailed, this, &UpdateDialog::onUpdateFailed);
    // Progress bar signals
    connect(this, &UpdateDialog::updateProgress, ui->progressBar, &QProgressBar::setValue);
    connect(this, &UpdateDialog::setProgressMaximum, ui->progressBar, &QProgressBar::setMaximum);
    connect(this, &UpdateDialog::setProgressBarFormat, ui->progressBar, &QProgressBar::setFormat);

    // Store version info in this dialog
    this->currentUpdateVersionInfo = versionInfo;

    // Get current version string
    QString currentVersionString = KfxVersion::currentVersion.version;
    if (KfxVersion::currentVersion.type == KfxVersion::ReleaseType::ALPHA) {
        currentVersionString += " " + tr("Alpha", "Appended to version string");
    }

    // Get new version string
    QString newVersionString = this->currentUpdateVersionInfo.version;
    if (this->currentUpdateVersionInfo.type == KfxVersion::ReleaseType::ALPHA) {
        newVersionString += " " + tr("Alpha", "Appended to version string");
    }

    // Update version text in UI
    QString infoLabelText = ui->infoLabel->text();
    ui->infoLabel->setText(
        infoLabelText.arg(
            tr("Current version:", "Label"),
            currentVersionString,
            tr("New version:", "Label"),
            newVersionString
        )
    );

    // Log the update path
    emit appendLog(QString("Update path: %1").arg(QCoreApplication::applicationDirPath()));

    // Check if alpha needs a new stable version
    if(currentUpdateVersionInfo.type == KfxVersion::ReleaseType::ALPHA) {
        updateToNewStableFirst = KfxVersion::checkIfAlphaUpdateNeedsNewStable(
            KfxVersion::currentVersion.version, this->currentUpdateVersionInfo.version);
        if(updateToNewStableFirst){
            emit appendLog(QString("Launcher will download a new stable version first"));
        }
    }

    // Handle auto update
    this->autoUpdate = autoUpdate;
    if (this->autoUpdate) {
        qDebug() << "Automatically starting update process";
        // Start process automatically
        ui->updateButton->click();
    }
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}

void UpdateDialog::onAppendLog(const QString &string)
{
    // Log to debug output
    qDebug() << "Update log:" << string;

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

void UpdateDialog::updateProgressBar(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        ui->progressBar->setMaximum(static_cast<int>(bytesTotal / 1024 / 1024));
        ui->progressBar->setValue(static_cast<int>(bytesReceived / 1024 / 1024));
        ui->progressBar->setFormat(tr("Downloading: %p% (%vMiB)", "Progress bar"));
    }
}

void UpdateDialog::onClearProgressBar()
{
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(1);
    ui->progressBar->setFormat("");
}

void UpdateDialog::onUpdateFailed(const QString &reason)
{
    ui->updateButton->setDisabled(false);
    ui->titleLabel->setText(this->originalTitleText);
    onClearProgressBar();
    onAppendLog(reason);
    QMessageBox::warning(this, tr("Update failed", "MessageBox Title"), reason);
}

void UpdateDialog::on_updateButton_clicked()
{
    // Disable update button
    ui->updateButton->setDisabled(true);

    // Backup saves if enabled
    if (Settings::getLauncherSetting("BACKUP_SAVES") == true) {
        QList<SaveFile *> saveFiles = SaveFile::getAll();
        if (saveFiles.length() > 0) {
            backupSaves(saveFiles);
        }
    }

    // Check if we need a new stable version first
    if(updateToNewStableFirst) {

        // Get latest stable version
        std::optional<KfxVersion::VersionInfo> latestStableVersionInfo = KfxVersion::getLatestVersion(KfxVersion::ReleaseType::STABLE);
        if(!latestStableVersionInfo){
            emit appendLog("Failed to grab latest stable version");
            emit setUpdateFailed(tr("The updater failed to grab the latest stable version which is required for this alpha.", "Failure Message"));
            return;
        }

        // Make sure alpha is newer than latest stable
        if(KfxVersion::isNewerVersion(currentUpdateVersionInfo.version, latestStableVersionInfo.value().version)){

            // Switch versions
            nextUpdateVersionInfo = currentUpdateVersionInfo;
            currentUpdateVersionInfo = latestStableVersionInfo.value(); // value() gets the VersionInfo from the std::optional
        }
    }

    this->update();
}

void UpdateDialog::update()
{
    // Update GUI to show we are updating
    ui->progressBar->setTextVisible(true);
    ui->titleLabel->setText(tr("Updating...", "Title label"));

    // Tell user we start the installation
    emit appendLog(QString("Updating to version %1").arg(currentUpdateVersionInfo.fullString));

    // Try and get filemap
    emit appendLog("Trying to get filemap");
    auto fileMap = KfxVersion::getGameFileMap(currentUpdateVersionInfo.type, currentUpdateVersionInfo.version);

    // Check if filemap is found
    if (fileMap.has_value() && !fileMap->isEmpty()) {
        // Show filecount log
        QString fileCount = QString::number(fileMap->count());
        emit appendLog(QString("Filemap with %1 files retrieved").arg(fileCount));
        // Update using filemap
        updateUsingFilemap(fileMap.value());
    } else {
        emit appendLog("No filemap found");
        // Update using download URL
        updateUsingArchive(currentUpdateVersionInfo.downloadUrl);
    }
}

void UpdateDialog::backupSaves(QList<SaveFile *> saveFiles)
{
    onAppendLog(QString("Backing up %1 save file(s)...").arg(saveFiles.length()));

    // Backup all save files
    if (SaveFile::backupAll(saveFiles)) {
        emit appendLog("Savefiles have been backed up");
    }
}

void UpdateDialog::updateUsingFilemap(QMap<QString, QString> fileMap)
{
    emit appendLog("Comparing filemap against local files...");

    // Set progress bar
    emit setProgressMaximum(fileMap.count() - 1);
    emit setProgressBarFormat(tr("Comparing: %p%", "Progress bar (%p=percentage)"));

    // Create fresh file list
    updateList = QStringList();

    // Loop through the filemap
    int progressCount = 0;
    for (auto it = fileMap.begin(); it != fileMap.end(); ++it) {
        QString filePath = it.key();
        QString expectedChecksum = it.value();

        // If we are updating to another KeeperFX version after this one,
        // we can skip some specific files that should always be present.
        // We do this to make the download go faster
        if(nextUpdateVersionInfo.type != KfxVersion::ReleaseType::UNKNOWN){
            QStringList filesToSkip = {
                {"/keeperfx"},
                {"/keeperfx.exe"},
                {"/keeperfx.map"},
                {"/keeperfx_hvlog.exe"},
                {"/keeperfx_hvlog.map"},
                {"/keeperfx-launcher-qt"},
                {"/keeperfx-launcher-qt.exe"},
            };
            if(filesToSkip.contains(filePath)){
                emit appendLog("Ignoring: " + filePath);
                continue;
            }
        }

        // Removed any leading zeros in the checksum
        expectedChecksum.replace(QRegularExpression("^0+(?!$)"), "");

        // Construct the full path to the local file
        QString localFilePath = QCoreApplication::applicationDirPath() + filePath;
        QFile file(localFilePath);

        // Check if local file exists
        if (!file.exists()) {
            updateList.append(filePath);
            continue;
        }

        // Make sure local file is readable
        // It needs to be for the checksum to work
        if (!file.open(QIODevice::ReadOnly)) {

            // We'll try to fallback to removing the file because the file might be corrupted or something
            // This shouldn't really happen but it might fix a weird issue
            if(file.remove() == true){
                qDebug() << "Deleted file during update:" << localFilePath;
                updateList.append(filePath);
                continue;
            }

            emit appendLog(QString("Failed to open file: %1").arg(localFilePath));
            emit setUpdateFailed(tr("Failed to open file: %1", "Failure Message").arg(localFilePath));
            return;
        }

        // Calculate CRC32 checksum
        QByteArray fileData = file.readAll();
        uLong crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef *>(fileData.data()), fileData.size());
        QString localChecksum = QString::number(crc, 16);

        // Compare checksums
        if (localChecksum != expectedChecksum) {
            qDebug() << "Checksum difference:" << filePath << ":" << localChecksum << "->"
                     << expectedChecksum;
            updateList.append(filePath);
        }

        // Update progress
        emit updateProgress(++progressCount);
    }

    // Clear progress bar
    emit clearProgressBar();

    // Get total files
    this->totalFiles = updateList.count();

    // If no files are found
    if (this->totalFiles == 0) {
        emit appendLog("No changes in the files have been detected");
        emit appendLog("Switching to archive download");
        // Switch to archive download
        updateUsingArchive(currentUpdateVersionInfo.downloadUrl);
        return;
    }

    // Check if we need to remove the launcher from the update list
    if (LauncherOptions::isSet("skip-launcher-update") && updateList.contains("/keeperfx-launcher-qt.exe")) {
        qInfo() << "Skipping launcher self-update ( --skip-launcher-update)";
        if (updateList.removeOne("/keeperfx-launcher-qt.exe")) {
            totalFiles--;
        }
    }

    // Log filecount
    emit appendLog(QString("%1 files need to be updated").arg(totalFiles));

    // Get type as string
    QString typeString;
    if (currentUpdateVersionInfo.type == KfxVersion::ReleaseType::STABLE) {
        typeString = "stable";
    } else if (currentUpdateVersionInfo.type == KfxVersion::ReleaseType::ALPHA) {
        typeString = "alpha";
    } else {
        return;
    }

    // Get download base URL
    QString baseUrl = QString(GAME_FILE_BASE_URL) + "/" + typeString + "/"
                      + currentUpdateVersionInfo.version;

    // Start downloading files
    downloadFiles(baseUrl);
}

void UpdateDialog::updateUsingArchive(QString downloadUrlString)
{
    emit appendLog(QString("Downloading archive: %1").arg(downloadUrlString));

    QUrl downloadUrl(downloadUrlString);

    // Get output file
    QString outputFilePath = QCoreApplication::applicationDirPath() + "/" + downloadUrl.fileName() + ".tmp";
    QFile *outputFile = new QFile(outputFilePath);

    Downloader *downloader = new Downloader(this);
    connect(downloader, &Downloader::downloadProgress, this, &UpdateDialog::updateProgressBar);
    connect(downloader, &Downloader::downloadCompleted, this, &UpdateDialog::onArchiveDownloadFinished);

    downloader->download(downloadUrl, outputFile);
}

void UpdateDialog::onArchiveDownloadFinished(bool success)
{
    if (!success) {
        emit appendLog("Archive download failed");
        emit setUpdateFailed(tr("Archive download failed.", "Failure Message"));
        return;
    }

    emit appendLog("Archive successfully downloaded");
    emit clearProgressBar();

    QFile *outputFile = new QFile(QCoreApplication::applicationDirPath() + "/" + QUrl(currentUpdateVersionInfo.downloadUrl).fileName() + ".tmp");

    // Test archive
    emit appendLog("Testing archive...");
    QThreadPool::globalInstance()->start([this, outputFile]() {
        uint64_t archiveSize = Archiver::testArchiveAndGetSize(outputFile);
        QMetaObject::invokeMethod(this, "onArchiveTestComplete", Qt::QueuedConnection, Q_ARG(uint64_t, archiveSize));
    });
}

void UpdateDialog::onArchiveTestComplete(uint64_t archiveSize){

    QFile *outputFile = new QFile(QCoreApplication::applicationDirPath() + "/" + QUrl(currentUpdateVersionInfo.downloadUrl).fileName() + ".tmp");

    // Show total size
    double archiveSizeInMiB = static_cast<double>(archiveSize) / (1024 * 1024);
    QString archiveSizeString = QString::number(archiveSizeInMiB, 'f', 2); // Format to 2 decimal places
    emit appendLog(QString("Total size: %1MiB").arg(archiveSizeString));

    // Start extraction process
    emit setProgressMaximum(static_cast<int>(archiveSize));
    emit setProgressBarFormat(tr("Extracting: %p%", "Progress bar"));
    emit appendLog("Extracting...");

    Extractor *extractor = new Extractor(this);
    connect(extractor, &Extractor::progress, this, &UpdateDialog::updateProgress);
    connect(extractor, &Extractor::extractComplete, this, &UpdateDialog::onUpdateComplete);
    connect(extractor, &Extractor::extractFailed, this, &UpdateDialog::setUpdateFailed);

    extractor->extract(outputFile, QCoreApplication::applicationDirPath());
}

void UpdateDialog::onUpdateComplete()
{
    emit appendLog("Extraction completed");
    emit clearProgressBar();

    // Remove temp archive
    emit appendLog("Removing temporary archive...");
    QFile *archiveFile = new QFile(QCoreApplication::applicationDirPath() + "/" + QUrl(currentUpdateVersionInfo.downloadUrl).fileName() + ".tmp");
    if (archiveFile->exists()) {
        archiveFile->remove();
    }

    // Check if this is the first update and we have another one that needs to be done
    if(nextUpdateVersionInfo.type != KfxVersion::ReleaseType::UNKNOWN){
        currentUpdateVersionInfo = nextUpdateVersionInfo;
        nextUpdateVersionInfo = KfxVersion::VersionInfo{}; // reset var

        // Do another update
        this->update();
        return;
    }

    // Handle any settings update
    emit appendLog("Copying over any new settings...");
    Settings::load();

    // We are done!
    ui->titleLabel->setText(tr("Update complete!", "Title label"));
    emit appendLog("Done!");

    // Create message box
    QMessageBox *msgBox = new QMessageBox(QMessageBox::Information,
                                          "KeeperFX",
                                          tr("KeeperFX has been successfully updated to version %1!", "MessageBox Text").arg(currentUpdateVersionInfo.version),
                                          QMessageBox::Ok);

    // Close messagebox after a delay if auto updating
    if (this->autoUpdate) {
        QTimer::singleShot(AUTO_UPDATE_MESSAGEBOX_TIMER, msgBox, &QMessageBox::accept);
    }

    // Show messagebox
    msgBox->exec();

    // Accept and close the update dialog
    accept();
}

void UpdateDialog::on_cancelButton_clicked()
{
    close();
}

void UpdateDialog::closeEvent(QCloseEvent *event)
{
    // Ask if user is sure
    int result = QMessageBox::question(this, tr("Confirmation", "MessageBox Title"), tr("Are you sure you want to cancel the update?", "MessageBox Text"));
    if (result == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}

void UpdateDialog::downloadFiles(const QString &baseUrl)
{
    // Create temp dir
    tempDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                   + "/kfx-update-" + currentUpdateVersionInfo.version);
    emit appendLog(QString("Temp directory path: %1").arg(tempDir.absolutePath()));

    // Make temp directory
    if (!tempDir.exists()) {
        tempDir.mkpath(".");
    }

    // Make sure temp directory exists now
    if (!tempDir.exists()) {
        emit appendLog("Failed to create temp directory");
        emit setUpdateFailed(tr("Failed to create temp directory", "Failure message"));
        return;
    }

    // Set progress bar to track total files
    emit setProgressMaximum(totalFiles);
    emit setProgressBarFormat(tr("Downloading: %p%", "Progress bar"));

    // Count downloaded files
    downloadedFiles = 0;

    for (const QString &filePath : std::as_const(updateList)) {
        QUrl url(baseUrl + filePath);
        QNetworkRequest request(url);

        // Disable HTTP/2 usage
        // We do this because otherwise we flood the server with requests
        // TODO: queue and limit parallel requests
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QNetworkReply *reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, filePath]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray fileData = reply->readAll();

                QString tempFilePath = tempDir.absolutePath() + filePath;
                QDir().mkpath(QFileInfo(tempFilePath).path());

                // Open the temporary file
                QFile tempFile(tempFilePath);
                if (tempFile.open(QIODevice::WriteOnly)) {
                    // Write the file
                    qint64 bytesWritten = tempFile.write(fileData);
                    tempFile.flush(); // Ensure data is written
                    tempFile.close();

                    // Verify file is written
                    if (bytesWritten == fileData.size()) {
                        emit appendLog(QString("Downloaded: %1").arg(filePath));
                    } else {
                        emit appendLog(QString("Failed to write: %1").arg(tempFilePath));
                    }
                } else {
                    emit appendLog(QString("Failed to open file for writing: %1").arg(tempFilePath));
                }
            } else {
                emit appendLog(QString("Failed to download: %1 -> %2").arg(filePath, reply->errorString()));
            }
            reply->deleteLater();

            emit fileDownloadProgress();
        });
    }
}

void UpdateDialog::onFileDownloadProgress()
{
    // Increase counter and update progress bar
    downloadedFiles++;
    emit updateProgress(downloadedFiles);

    // If all files are downloaded
    if (downloadedFiles == totalFiles) {
        // Show user
        emit clearProgressBar();
        emit appendLog("All files downloaded successfully");

        // Start copying files to KeeperFX dir
        emit appendLog("Moving files to KeeperFX directory...");

        // Make sure temp dir exists
        if (!tempDir.exists()) {
            emit appendLog("The temporary directory does not exist");
            emit setUpdateFailed(tr("The temporary directory does not exist", "Failure Message"));
            return;
        }

        // Define rename rules
        QMap<QString, QString> renameRules = {
            {"/keeperfx.cfg", "/_keeperfx.cfg"},
            {"/keeperfx-launcher-qt.exe", "/keeperfx-launcher-qt-new.exe"},
            {"/keeperfx-launcher-qt", "/keeperfx-launcher-qt-new"},
        };

        // Set progress bar
        emit setProgressBarFormat(tr("Copying: %p%", "Progress bar"));
        emit setProgressMaximum(totalFiles);

        // Vars
        QDir appDir(QCoreApplication::applicationDirPath());
        int copiedFiles = 0;

        // Move and rename files
        for (const QString &filePath : std::as_const(updateList)) {
            QString srcFilePath = tempDir.absolutePath() + filePath;

            // Make sure source file exists
            QFile srcFile(srcFilePath);
            if (!srcFile.exists()) {
                emit appendLog(QString("File does not exist: %1").arg(srcFilePath));
                emit setUpdateFailed(tr("File does not exist: %1", "Failure Message").arg(srcFilePath));
                return;
            }

            // Use the new name if exists, otherwise use the same name
            QString destFileName = renameRules.value(filePath, filePath);
            if (destFileName != filePath) {
                emit appendLog(QString("Renaming file during copy: %1 -> %2").arg(filePath, destFileName));
            }

            // Get destination path
            QString destFilePath = appDir.absolutePath() + destFileName;

            // Make sure destination directory exists
            QFileInfo destInfo(destFilePath);
            if (QDir().mkpath(destInfo.absolutePath()) == false) {
                emit appendLog(QString("Failed to create destination directory: %1").arg(destInfo.absolutePath()));
                emit setUpdateFailed(tr("Failed to create destination directory: %1", "Failure Message").arg(destInfo.absolutePath()));
                return;
            }

            // Remove destination file if it exists
            QFile destFile(destFilePath);
            if (destFile.exists()) {
                if(destFile.remove() == false){
                    emit setUpdateFailed(tr("Failed to remove file (%1): %2", "Failure Message").arg(destFile.errorString(), destFile.fileName()));
                    return;
                }
            }

            // Move file
            if (srcFile.rename(destFilePath) == true) {
                emit appendLog(QString("File moved: %1").arg(filePath));
            } else {

                // Check if we tried moving a binary file while others have already been succesfully moved.
                // We do this because if a binary file fails to move while non binaries don't,
                // it most likely means an antivirus or similar protection blocked it.
                // We can't check if the file is a binary by opening it because even that may be blocked,
                // so we just check for known file extensions here.
                if(copiedFiles > 0 && (
                    #ifdef Q_OS_WINDOWS
                        filePath.toLower().endsWith(".exe") ||
                        filePath.toLower().endsWith(".dll")
                    #else
                        filePath.contains(".") == false ||
                        filePath.toLower().endsWith(".so")
                    #endif
                )) {
                    // We'll show the same message as below but also add an antivirus warning
                    emit setUpdateFailed(
                        tr("Failed to move file (%1): %2", "Failure Message").arg(srcFile.errorString(), filePath)
                        + QString("\n\n")
                        + tr("It is possible an antivirus is causing this problem. Try disabling it and try again.", "Failure Message").arg(srcFile.errorString(), filePath)
                    );
                } else {
                    emit setUpdateFailed(tr("Failed to move file (%1): %2", "Failure Message").arg(srcFile.errorString(), filePath));
                }

                return;
            }

            emit updateProgress(++copiedFiles);
        }

        // If all files have been updated
        if (copiedFiles == totalFiles) {

            // Check if this is the first update and we have another one that needs to be done
            if(nextUpdateVersionInfo.type != KfxVersion::ReleaseType::UNKNOWN){
                currentUpdateVersionInfo = nextUpdateVersionInfo;
                nextUpdateVersionInfo = KfxVersion::VersionInfo{}; // reset var

                // Do another update
                this->update();
                return;
            }

            ui->titleLabel->setText(tr("Update complete!", "Title label"));
            emit clearProgressBar();

            // Copy new settings
            emit appendLog("Copying any new settings...");
            Settings::load();

            // Success!
            emit appendLog("Done!");

            // Create message box
            QMessageBox *msgBox = new QMessageBox(QMessageBox::Information,
                                                  "KeeperFX",
                                                  tr("KeeperFX has been successfully updated to version %1!", "MessageBox Text").arg(currentUpdateVersionInfo.version),
                                                  QMessageBox::Ok,
                                                  this);

            // Close messagebox after a delay if auto updating
            if (this->autoUpdate) {
                QTimer::singleShot(AUTO_UPDATE_MESSAGEBOX_TIMER, msgBox, &QMessageBox::accept);
            }

            // Show messagebox
            msgBox->exec();

            // Accept and close update dialog
            accept();
            return;

        } else {
            emit appendLog("Failed to move all downloaded files");
            emit setUpdateFailed(tr("Failed to move all downloaded files", "Failure message"));
        }
    }
}
