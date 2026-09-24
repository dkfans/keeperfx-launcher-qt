#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QVariant>
#include <QDirIterator>
#include <QUuid>
#include <QComboBox>
#include <QWheelEvent>
#include <QDesktopServices>

#ifdef Q_OS_WINDOWS
    #include <windows.h>
#endif

#include <LIEF/PE.hpp>
#include <LIEF/logging.hpp>

class Helper
{
public:

    static bool isKeeperFxInstalled()
    {
        // Check for 'keeperfx.exe' file in app directory
        QFile keeperFxBin (QCoreApplication::applicationDirPath() + "/keeperfx.exe");
        return (keeperFxBin.exists());
    }

    static void removeLeftoverNewLauncher()
    {
#ifdef Q_OS_WINDOWS
        QString newLauncherPathString(QCoreApplication::applicationDirPath()
                                    + "/keeperfx-launcher-qt.exe");
#else
        QString newLauncherPathString(QCoreApplication::applicationDirPath()
                                    + "/keeperfx-launcher-qt");
#endif

        QFile newLauncherFile(newLauncherPathString);
        if (newLauncherFile.exists()) {
            newLauncherFile.remove();
        }
    }

    static int countFilesRecursive(const QDir &dir) {

        int count = 0;

        // Loop trough all files
        QDirIterator it(dir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++count;
        }

        return count;
    }

    static bool isBinaryFile(const QString &filePath)
    {
        // Make sure file exists before continueing
        QFile file(filePath);
        if(file.exists() == false){
            return false;
        }

        // Save current logging state
        const auto previousLevel = LIEF::logging::get_level();

        // Disable logging temporarily (only if not already disabled)
        const bool wasEnabled = (previousLevel != LIEF::logging::LEVEL::OFF);
        if (wasEnabled) {
            LIEF::logging::disable();
        }

        try {
            std::unique_ptr<LIEF::Binary> bin{
                LIEF::Parser::parse(filePath.toStdString())
            };

            // Restore logging
            if (wasEnabled) {
                LIEF::logging::set_level(previousLevel);
            }

            return bin != nullptr;
        } catch (...) {

            // Restore logging
            if (wasEnabled) {
                LIEF::logging::set_level(previousLevel);
            }

            return false;
        }
    }

    static bool is64BitDLL(const std::string &dllPath)
    {
        try {
            auto pe = LIEF::PE::Parser::parse(dllPath);
            return LIEF::PE::Header::x86_64(pe->header().machine());
        } catch (const std::exception &e) {
            qWarning() << "LIEF error: " << e.what();
            return false; // Assume 32-bit or invalid file if parsing fails
        }
    }

    static bool is64BitDll(QString dllPath) {
        return Helper::is64BitDLL(dllPath.toStdString());
    }

    static bool checkForWritePermissionInDir(const QDir dir) {

        // Generate a random filename to test write permissions
        // Ex: ".write-permission-testfile-a90g7f2i.tmp"
        // The dot at the start means a hidden file on linux
        const QString fileName =
            ".write-permission-testfile-" +
            QUuid::createUuid().toString().remove('{').remove('}').remove('-').left(8) +
            ".tmp";

        QString filePath = dir.absoluteFilePath(fileName);
        qDebug() << "Testing file write permission:" << filePath;

        QFile file(filePath);

        // If for some magic reason this file already exists
        if(file.exists()){
            qWarning() << "Write permission file already exists:" << filePath;
            if(file.remove() == false){
                qWarning() << "Failed to remove existing write permission file:" << filePath;
                return false;
            }
        }

        // Check if we can write this file
        // This should also create the file
        if (file.open(QIODevice::WriteOnly)) {
            if(file.remove() == false) {
                qWarning() << "Failed to remove write permission test file:" << filePath;
            }
            return true;
        }

        qDebug() << "Directory not writable:" << filePath;
        return false;
    }

    static QFile getUnearthBinary()
    {
        const QString rootDir = QCoreApplication::applicationDirPath();
        const QStringList subdirs = {"unearth", "Unearth"};

#ifdef Q_OS_WINDOWS
        const QStringList filenames = {"unearth.exe"};
#else
        const QStringList filenames = {"unearth", "unearth.x86_64"};
#endif

        for (const QString &subdir : subdirs) {
            QDir dir(rootDir + QDir::separator() + subdir);
            if (!dir.exists())
                continue;

            const QStringList entries = dir.entryList(QDir::Files | QDir::NoSymLinks);
            for (const QString &entry : entries) {
                for (const QString &expected : filenames) {
                    if (entry.compare(expected, Qt::CaseInsensitive) == 0) {
                        return QFile(dir.absoluteFilePath(entry));
                    }
                }
            }
        }

        // Not found
        return QFile();
    }

    // Function to check if we are running under Wine
    static bool isRunningUnderWine()
    {
#ifdef Q_OS_WINDOWS
        HMODULE hModule = GetModuleHandleA("ntdll.dll");
        if (hModule) {
            // Check for Wine-specific function
            if (GetProcAddress(hModule, "wine_get_version")) {
                return true;
            }
        }
#endif
        return false;
    }

    // Function to get the Wine version as a QString
    static QString getWineVersion()
    {
#ifdef Q_OS_WINDOWS
        typedef const char *(__cdecl * wine_get_version_func)();
        HMODULE hModule = GetModuleHandleA("ntdll.dll");
        if (hModule) {
            wine_get_version_func wine_get_version = (wine_get_version_func) GetProcAddress(hModule, "wine_get_version");
            if (wine_get_version) {
                return QString::fromUtf8(wine_get_version());
            }
        }
#endif
        return QString();
    }

    // Function to get the Wine host machine name as a QString
    static QString getWineHostMachineName()
    {
#ifdef Q_OS_WINDOWS
        typedef const char *(__cdecl * wine_get_host_machine_name_func)();
        HMODULE hModule = GetModuleHandleA("ntdll.dll");
        if (hModule) {
            wine_get_host_machine_name_func wine_get_host_machine_name = (wine_get_host_machine_name_func) GetProcAddress(hModule, "wine_get_host_machine_name");
            if (wine_get_host_machine_name) {
                return QString::fromUtf8(wine_get_host_machine_name());
            }
        }
#endif
        return QString();
    }

    static bool makeBinaryExecutable(const QString &path)
    {
#ifdef Q_OS_UNIX

        QFile file(path);
        if (!file.exists()){
            return false;
        }

        // Get file permissions object
        QFile::Permissions perms = file.permissions();

        // Add execute bits for owner, group, and others
        perms |= QFileDevice::ExeOwner;
        perms |= QFileDevice::ExeGroup;
        perms |= QFileDevice::ExeOther;

        // Set file permissions
        return file.setPermissions(perms);

#else
        // On Windows executability is not a permission flag
        Q_UNUSED(path);
        return true;
#endif
    }

    /**
     * Disables background scrolling for a single QComboBox.
     */
    inline static void disableComboBoxScroll(QComboBox* comboBox) {
        if (!comboBox) return;

        comboBox->setFocusPolicy(Qt::StrongFocus);

        class ScrollBlocker : public QObject {
            protected:
            bool eventFilter(QObject *obj, QEvent *event) override {
                if (event->type() == QEvent::Wheel) {
                    QComboBox* combo = qobject_cast<QComboBox*>(obj);

                    // If combobox isn't focused, we block it from scrolling
                    if (combo && !combo->hasFocus()) {
                        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);

                        // Manually bubble the wheel event up the widget tree.
                        // This allows the QScrollArea to see the scroll wheel action.
                        QWidget* parent = combo->parentWidget();
                        while (parent) {
                            wheelEvent->setAccepted(false); // Reset event state

                            // Send event to the parent
                            QCoreApplication::sendEvent(parent, wheelEvent);

                            // If a parent (like a ScrollArea) accepts and handles it, stop bubbling
                            if (wheelEvent->isAccepted()) {
                                break;
                            }

                            // Move up to the next parent
                            parent = parent->parentWidget();
                        }

                        // Return true to stop the QComboBox itself from changing values
                        return true;
                    }
                }
                return QObject::eventFilter(obj, event);
            }
        };

        static ScrollBlocker filter;
        comboBox->installEventFilter(&filter);
    }

    /**
     * Recursively loops through a parent widget and disables scroll
     * for all QComboBoxes and derived classes.
     */
    inline static void disableAllComboBoxScrolls(QWidget* parentWidget) {
        if (!parentWidget) return;

        // findChildren recursively grabs every QComboBox in the UI
        for (QComboBox* combo : parentWidget->findChildren<QComboBox*>()) {
            disableComboBoxScroll(combo);
        }
    }

    static bool openLocalFileWithDefaultSystemHandler(QString filePath)
    {
        // Make sure the config file exists
        if (!QFileInfo::exists(filePath)) {
            qWarning() << "File does not exist:" << filePath;
            return false;
        }

#ifdef Q_OS_UNIX
        // Check if we are running inside a Flatpak (like Qt Creator's environment)
        if (qEnvironmentVariableIsSet("FLATPAK_ID")) {
            qInfo() << "Flatpak detected: Opening file via 'xdg-open' on host:" << filePath;
            // Escape the sandbox and ask the host OS to open the file
            if(QProcess::startDetached("flatpak-spawn", {"--host", "xdg-open", filePath})){
                return true;
            }
        }

        qInfo() << "Opening file via 'xdg-open' with clean environment:" << filePath;

        // Create process that will delete its object later
        QProcess *process = new QProcess();
        QObject::connect(process, &QProcess::finished, process, &QObject::deleteLater);

        // Create clean environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("LD_LIBRARY_PATH");
        process->setProcessEnvironment(env);

        // Start xdg-open with clean environment
        process->setProgram("xdg-open");
        process->setArguments({filePath});
        if(process->startDetached()){
            return true;
        }
#endif

        // Open file using QT's default OS functionality
        qInfo() << "Opening file via QDesktopServices::openUrl:" << filePath;
        return QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }

    static bool isFileLocked(const QString &filePath) {

        // Make sure the file exists
        QFile file(filePath);
        if (!file.exists()) {
            return false;
        }

#ifdef Q_OS_WIN

        // Convert path to Windows native format and wide string
        std::wstring wPath = QDir::toNativeSeparators(filePath).toStdWString();

        // Try to open the file with write access
        HANDLE hFile = CreateFileW(
            wPath.c_str(),
            GENERIC_WRITE,
            0,                      // No sharing (requires exclusive write access)
            nullptr,
            OPEN_EXISTING,          // Do not create, only open existing
            FILE_ATTRIBUTE_NORMAL,
            nullptr
            );

        // Check if we failed to get a handle
        if (hFile == INVALID_HANDLE_VALUE) {

            DWORD errorCode = GetLastError();
            // 32: ERROR_SHARING_VIOLATION (file is open in another program)
            // 1224: ERROR_USER_MAPPED_FILE (executable is currently running)
            if (errorCode == ERROR_SHARING_VIOLATION || errorCode == ERROR_USER_MAPPED_FILE) {
                return true;
            }

            // Failed due to permissions and not a lock
            return false;
        }

        // File was succesfully opened for writing so let's close the handle again
        CloseHandle(hFile);
        return false;
#else
        // Fallback for non-Windows platforms using pure Qt
        // Append flag should guarantee the file won't be truncated
        return !file.open(QIODevice::Append);
#endif
    }
};
