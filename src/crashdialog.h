#pragma once

#include "savefile.h"

#include <QDateTime>
#include <QDialog>
#include <optional>

namespace Ui {
class CrashDialog;
}

class CrashDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CrashDialog(QWidget *parent = nullptr);
    ~CrashDialog();

    void setStdErrorString(QString stdErrorString);

    // Records how the game ended and looks for the crash dump and replay from this run
    void setGameRun(int exitCode, const QDateTime &gameStartTime);

private slots:
    void on_cancelButton_clicked();
    void on_sendButton_clicked();

private:
    Ui::CrashDialog *ui;
    QList<SaveFile *> saveFileList;
    QString stdErrorString;
    std::optional<int> exitCode;
    QString crashDumpPath; // Empty when the game did not write a dump during this run
    QString replayPath;    // Replay being recorded or played when the game crashed, if any
    std::optional<int> replayTurn;

    static QString findCrashDump(const QDateTime &notBefore);
    void findReplay();
};
