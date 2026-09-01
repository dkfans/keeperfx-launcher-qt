#pragma once

#include <QDialog>
#include <QCloseEvent>
#include <QDir>

#include "kfxversion.h"

namespace Ui {
class InstallKfxDialog;
}

class InstallKfxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InstallKfxDialog(QWidget *parent = nullptr);
    ~InstallKfxDialog();

private slots:
    void on_installButton_clicked();
    void on_versionComboBox_currentIndexChanged(int index);
    void on_cancelButton_clicked();

    void onAppendLog(const QString &string);
    void onInstallFailed(const QString &reason);
    void onClearProgressBar();
    void updateProgressBarDownload(qint64 bytesReceived, qint64 bytesTotal);

    void onStableDownloadFinished(bool success);
    void onStableArchiveTestComplete(uint64_t archiveSize);
    void onStableExtractComplete();

    void onAlphaDownloadFinished(bool success);
    void onAlphaArchiveTestComplete(uint64_t archiveSize);
    void onAlphaExtractComplete();

signals:
    void appendLog(const QString &string);
    void setInstallFailed(const QString &reason);
    void clearProgressBar();
    void updateProgressBar(int value);
    void setProgressMaximum(int value);
    void setProgressBarFormat(QString format);

private:
    void startStableDownload();

    void startAlphaDownload();

    void testAndExtractArchive(bool isStable);
    void closeEvent(QCloseEvent *event) override;

    Ui::InstallKfxDialog *ui;
    KfxVersion::ReleaseType installReleaseType;

    QUrl downloadUrlStable;
    QUrl downloadUrlAlpha;

    QFile *tempArchiveStable;
    QFile *tempArchiveAlpha;

    QDir tempDirStable;
    QDir tempDirAlpha;

    bool moveTempFilesToAppDir(QDir sourceDir);

    void completeInstall();

    void loadCdnListAndSelectSuggested();
};
