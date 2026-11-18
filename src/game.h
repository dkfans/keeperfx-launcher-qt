#pragma once

#include <QDateTime>
#include <QVariant>
#include <QString>
#include <QObject>
#include <QProcess>

class Game : public QObject
{
    Q_OBJECT

public:
    enum StartType {
        NORMAL,
        HVLOG,
        DIRECT_CONNECT,
        MAP,
        CAMPAIGN,
        LOAD_SAVE,
        LOAD_REPLAY,
        START_WITHOUT_MODS,
    };

    explicit Game(QWidget *parent = nullptr);

    static QString getStringFromStartType(StartType startType);

    bool start(StartType startType,
               QVariant data1 = QVariant(),
               QVariant data2 = QVariant(),
               QVariant data3 = QVariant());

    QString getErrorString();

signals:
    void gameEnded(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QWidget *parentWidget;
    QProcess *process;
    QString errorString;
    QDateTime startTime; // Used to tell this run's crash dump apart from older ones
};
