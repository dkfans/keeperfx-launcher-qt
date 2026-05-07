#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QVariant>
#include <QDirIterator>

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
};
