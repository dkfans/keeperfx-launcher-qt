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
#include <QtEndian>

#ifdef Q_OS_WINDOWS
    #include <windows.h>
#endif

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
        QFile file(filePath);

        // open() implicitly checks if the file exists and is readable
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        // Read the first 1024 bytes. This is usually more than enough
        // to catch both the ELF magic and the PE header offset.
        const QByteArray header = file.read(1024);
        if (header.size() < 4) {
            return false; // File too small to be a valid binary
        }

        // Check for Linux/Unix ELF
        // Magic bytes: 0x7F, 'E', 'L', 'F'
        if (header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F') {
            return true;
        }

        // Check for Windows PE (Portable Executable)
        // Must start with 'M' 'Z' (DOS header)
        if (header[0] == 'M' && header[1] == 'Z') {

            // To ensure it's a modern Windows PE and not a 40-year-old MS-DOS executable,
            // we read the 32-bit pointer at offset 0x3C (60), which points to the real PE header.
            if (header.size() >= 64) {

                quint32 peOffset = qFromLittleEndian<quint32>(header.constData() + 0x3C);

                // Is the PE signature within the chunk we already read?
                if (peOffset > 0 && peOffset + 4 <= static_cast<quint32>(header.size())) {
                    if (header[peOffset] == 'P' && header[peOffset + 1] == 'E' &&
                        header[peOffset + 2] == '\0' && header[peOffset + 3] == '\0') {
                        return true;
                    }
                }

                // In rare cases, the DOS stub is unusually large and falls outside our 1024-byte read
                else if (peOffset > 0) {
                    if (file.seek(peOffset)) {
                        QByteArray peSignature = file.read(4);
                        if (peSignature.size() == 4 &&
                            peSignature[0] == 'P' && peSignature[1] == 'E' &&
                            peSignature[2] == '\0' && peSignature[3] == '\0') {
                            return true;
                        }
                    }
                }
            }

            // If we need to return true for ANY Windows/DOS executable (even ancient 16-bit
            // .exe files) uncomment the following line:
            // return true;
        }

        return false;
    }

    static bool is64BitBinary(const QString &binaryPath)
    {
        QFile file(binaryPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "is64BitBinary: Failed to open file:" << binaryPath;
            return false;
        }

        // Read the first 64 bytes to cover ELF magic and PE header offset
        QByteArray header = file.read(64);
        if (header.size() < 64) {
            return false;
        }

        // Check for Linux ELF
        if (header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F') {
            // The 5th byte (offset 0x04) in ELF header specifies the class
            // 0x01 = 32-bit, 0x02 = 64-bit
            return header[4] == 0x02;
        }

        // Check for Windows PE (.exe or .dll)
        if (header[0] == 'M' && header[1] == 'Z') {
            quint32 peOffset = qFromLittleEndian<quint32>(header.constData() + 0x3C);

            if (file.seek(peOffset)) {
                QByteArray peHeader = file.read(6);
                if (peHeader.size() == 6 &&
                    peHeader[0] == 'P' && peHeader[1] == 'E' &&
                    peHeader[2] == '\0' && peHeader[3] == '\0') {

                    // Machine type is stored right after the "PE\0\0" signature
                    quint16 machineType = qFromLittleEndian<quint16>(peHeader.constData() + 4);

                    // 0x8664 is x86_64 (AMD64)
                    // (You can also add `|| machineType == 0xAA64` here for ARM64 support)
                    return machineType == 0x8664;
                }
            }
        }

        return false; // Not a recognized 64-bit PE/ELF binary file
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
