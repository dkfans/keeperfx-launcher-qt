#include "archiver.h"

#include "helper.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <bit7z/bitextractor.hpp>
#include <bit7z/bitabstractarchivehandler.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <bit7z/bitfileextractor.hpp>

std::optional<bit7z::Bit7zLibrary> Archiver::lib;

// Initialize the library if it's not already loaded
void Archiver::loadBit7zLib()
{
    // Only load if it's not already initialized
    if (Archiver::lib) {
        return;
    }

    QString libPath;

    // Get the 7z library file (.dll/.so)
#ifdef Q_OS_WINDOWS
    if (QFile(QCoreApplication::applicationDirPath() + "/7za.dll").exists()) {
        libPath = QCoreApplication::applicationDirPath() + "/7za.dll";
    } else if (QFile(QCoreApplication::applicationDirPath() + "/7z.dll").exists()) {
        libPath = QCoreApplication::applicationDirPath() + "/7z.dll";
    } else {
        qWarning() << "Failed to find 7zip lib to load";
        return;
    }
#else
    libPath = QCoreApplication::applicationDirPath() + "/7z.so";
#endif

    // Make sure this library's architecture matches our own binary architecture
#if QT_POINTER_SIZE == 8
    if (!Helper::is64BitBinary(libPath)) {
        qWarning() << "Not a 64 bit library:" << libPath;
    }
#else
    if (Helper::is64BitBinary(libPath)) {
        qWarning() << "Trying to load 64 bit library in non 64 bit executably:" << libPath;
    }
#endif

    qDebug() << "7z lib path:" << libPath;

    // Initialize the static library
    lib.emplace(BIT7Z_STRING(libPath.toStdString()));

    // Make sure lib is loaded now
    if (!Archiver::lib) {
        throw std::runtime_error("Failed to load bit7z library");
    }
}

bit7z::BitArchiveReader Archiver::getReader(std::string filePath)
{
    // Make sure library is loaded
    Archiver::loadBit7zLib();

    // Create the reader
    // For now only 7z because we use .tmp file extension
    return bit7z::BitArchiveReader{*lib, filePath, bit7z::BitFormat::SevenZip};
}

bit7z::BitFileExtractor Archiver::getExtractor()
{

    // Make sure library is loaded
    Archiver::loadBit7zLib();

    // Create the reader
    // For now only 7z because we use .tmp file extension
    return bit7z::BitFileExtractor{*lib, bit7z::BitFormat::SevenZip};
}

bit7z::BitFileCompressor Archiver::getCompressor()
{
    // Make sure library is loaded
    Archiver::loadBit7zLib();

    // Create the compressor
    // For now only 7z
    return bit7z::BitFileCompressor{*lib, bit7z::BitFormat::SevenZip};
}

bool Archiver::compressSingleFile(QFile *inputFile, std::string outputPath)
{
    bit7z::BitFileCompressor compressor = Archiver::getCompressor();

    qDebug() << inputFile->fileName().toStdString();
    qDebug() << outputPath;

    try {
        compressor.compress({inputFile->fileName().toStdString()}, outputPath);

        return true;

    } catch ( const bit7z::BitException& ex ) {

        qWarning() << "Failed to compress single file:" << ex.what();
    }

    return false;
}

uint64_t Archiver::testArchiveAndGetSize(QFile *archiveFile)
{
    // Get file info for the archive file
    QFileInfo archiveFileInfo(archiveFile->filesystemFileName());

    // Get archive reader
    bit7z::BitArchiveReader archive = Archiver::getReader(
        archiveFileInfo.absoluteFilePath().toStdString()
    );

    try{

        // Test the archive
        // Throws a BitException when it is invalid
        archive.test();

        // Return the total size of the uncompressed files
        return archive.size();

    } catch (const bit7z::BitException& ex) {

        qWarning() << "Archive test failure:" << ex.what();
        return -1;
    }
}
