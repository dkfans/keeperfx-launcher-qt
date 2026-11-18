#include "savefile.h"

#include "archiver.h"
#include "kfxversion.h"

#include <QApplication>
#include <QDir>
#include <QMap>

#include <zlib.h>

// Constructor
SaveFile::SaveFile(const QString &filePath)
{
    // Set the path of the savefile
    file.setFileName(filePath);

    // Make sure the save can be loaded
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "Savefile can not be opened:" << filePath;
        return;
    }

    // Get the filename of the file
    fileName = QFileInfo(file).fileName();

    // KFXS saves (KeeperFX 1.4+) describe their own layout
    if (file.peek(4) == "KFXS") {
        if (file.size() <= 64 * 1024LL * 1024LL) {
            readKfxsTitle(file.readAll());
        }
        file.close();
        qDebug() << "Savefile object created:" << toString();
        return;
    }

    try {

        // Check the header of the savefile
        if (!checkFileHeader(file)) {
            file.close();
            return;
        }

        // Positions and length of data we want to read
        // You can use any hex editor to figure out the starting pos
        // The values below should be for save files before kfx v1.2.0.4479
        // Save files after this version have their struct changed
        qint64 saveNamePos = 0x12;
        qint64 saveNameLength = 15;
        qint64 campaignNamePos = 0x25;
        qint64 campaignNameLength = 160;

        // Positions have changed when LUA was added (1.2.0.4479)
        if(KfxVersion::hasFunctionality("save_file_struct_lua")){
            saveNamePos = 0xE;
            campaignNamePos = 0x21;
        }

        // Positions have changed when save name length has increased (1.3.1.4881)
        if(KfxVersion::hasFunctionality("save_file_struct_30_char_name")){
            saveNameLength = 30;
            campaignNamePos = 0x30;
        }

        // The current position in the save file seek
        qint64 currentPos;

        // Read save name
        // We force the loop to end at the end pos, but we should always return earlier
        currentPos = saveNamePos;
        while (currentPos < (saveNamePos + saveNameLength)) {
            file.seek(currentPos);
            QByteArray buff = file.read(1);
            if (buff.isEmpty() || buff.at(0) == '\0') {
                break;
            }
            saveName += buff;
            currentPos++;
        }

        // Read campaign/ruleset name
        currentPos = campaignNamePos;
        while (currentPos < (campaignNamePos + campaignNameLength)) {
            file.seek(currentPos);
            QByteArray buff = file.read(1);
            if (buff.isEmpty() || buff.at(0) == '\0') {
                break;
            }
            campaignName += buff;
            currentPos++;
        }

        qDebug() << "Savefile object created:" << toString();

    } catch (QException& ex) {
        qWarning() << "Savefile exception: " << ex.what();
    }

    // Make sure the file handle is closed again
    file.close();
}

bool SaveFile::isValid() {
    return !saveName.isEmpty() && !campaignName.isEmpty();
}

QString SaveFile::toString() {
    // return fileName + ": " + saveName + " (" + campaignName + ")";
    return saveName + " (" + campaignName + ")";
}

QList<SaveFile *> SaveFile::getAll()
{
    QList<SaveFile *> list;

    // Check if the save file dir exists
    QDir saveFileDir(QApplication::applicationDirPath() + "/save");
    if (saveFileDir.exists() == false) {
        return list; // Empty list
    }

    // Create the filename filter
    QStringList saveFileFilter;
    saveFileFilter << "fx1g*.sav";

    // Get savefile paths
    QStringList saveFiles = saveFileDir.entryList(saveFileFilter, QDir::Files);
    if (saveFiles.isEmpty()) {
        return list; // Empty list
    }

    // Loop trough all files
    for (const QString &saveFileFilename : std::as_const(saveFiles)) {
        // Try to load this file as a SaveFile
        SaveFile *saveFile = new SaveFile(saveFileDir.absoluteFilePath(saveFileFilename));

        if (saveFile->isValid()) {
            list << saveFile;
        }
    }

    return list;
}

// See src/kfx/save/FORMAT.txt in dkfans/keeperfx. A KFXS file is a 20-byte header, then chunks
// of {id, flags, stored_len, raw_len, crc, body}. META holds a CatalogueEntry record; the
// SCHM chunk lists every struct's fields so we can find textname and campaign_name in it.
bool SaveFile::readKfxsTitle(const QByteArray &data)
{
    auto u16 = [&data](qsizetype at) { return quint16(quint8(data[at]) | quint8(data[at + 1]) << 8); };
    auto u32 = [&u16](qsizetype at) { return quint32(u16(at)) | quint32(u16(at + 2)) << 16; };

    if (data.size() < 20 || data.mid(8, 2) != QByteArray("\x01\x00", 2)) {
        return false; // only format major 1 is known
    }

    QByteArray meta, schema;
    for (qsizetype at = 20; at + 20 <= data.size();) {
        const QByteArray id = data.mid(at, 4);
        const quint32 flags = u32(at + 4), storedLen = u32(at + 8), rawLen = u32(at + 12);
        if (quint64(at) + 20 + storedLen > quint64(data.size())) {
            return false;
        }
        if (id != "META" && id != "SCHM") { // only the title and the schema are needed
            at += 20 + storedLen;
            continue;
        }
        if (rawLen > 4 * 1024 * 1024) {
            return false;
        }
        QByteArray body = data.mid(at + 20, storedLen);
        if (flags & 0x01) { // zlib
            QByteArray raw(rawLen, '\0');
            uLongf length = rawLen;
            if (uncompress(reinterpret_cast<Bytef *>(raw.data()), &length, reinterpret_cast<const Bytef *>(body.constData()), storedLen) != Z_OK) {
                return false;
            }
            body = raw;
        }
        if (id == "META") meta = body;
        if (id == "SCHM") schema = body;
        if (id == "TURN") break;
        at += 20 + storedLen;
    }
    if (meta.isEmpty() || schema.size() < 4) {
        return false;
    }

    // Walk the schema to CatalogueEntry and note where its string fields sit in the record
    static const int typeSizes[] = {0, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 1, 1};
    auto s16 = [&schema](qsizetype at) { return quint16(quint8(schema[at]) | quint8(schema[at + 1]) << 8); };
    auto s32 = [&schema, &s16](qsizetype at) { return quint32(s16(at)) | quint32(s16(at + 2)) << 16; };
    qsizetype p = 0;
    auto str = [&]() { const quint16 n = s16(p); const QString s = QString::fromLatin1(schema.mid(p + 2, n)); p += 2 + n; return s; };

    QMap<QString, QPair<qsizetype, qsizetype>> strings; // field -> (offset in record, capacity)
    const quint32 structCount = s32(p);
    p += 4;
    for (quint32 s = 0; s < structCount && p < schema.size(); s++) {
        const QString structName = str();
        const quint16 fieldCount = s16(p);
        p += 2;
        qsizetype offset = 0;
        bool layoutKnown = true;
        for (quint16 f = 0; f < fieldCount && p < schema.size(); f++) {
            const QString fieldName = str();
            const quint8 type = quint8(schema[p]);
            const quint8 dimCount = quint8(schema[p + 2]);
            p += 3;
            qsizetype count = 1;
            for (quint8 d = 0; d < dimCount; d++, p += 4) count *= s32(p);
            str(); // sub_type
            if (type == 14) { // union: skip its member list
                const quint16 members = s16(p);
                p += 2;
                for (quint16 m = 0; m < members; m++) { str(); str(); p += 4; }
            }
            if (structName != "CatalogueEntry") continue;
            if (type == 12) strings[fieldName] = qMakePair(offset, count);
            if (type == 0 || type > 12) layoutKnown = false; // a struct or union: offsets after it are unknown
            if (layoutKnown) offset += typeSizes[type] * count;
        }
    }

    auto readString = [&meta, &strings](const QString &field) {
        if (!strings.contains(field)) return QString();
        const auto [offset, capacity] = strings[field];
        const QByteArray bytes = meta.mid(offset, capacity);
        return QString::fromLatin1(bytes.left(bytes.indexOf('\0') >= 0 ? bytes.indexOf('\0') : bytes.size()));
    };
    saveName = readString("textname");
    campaignName = readString("campaign_name");
    return !saveName.isEmpty();
}

bool SaveFile::checkFileHeader(QFile& file) {
    file.seek(0x4);
    QByteArray header = file.read(4);
    return (header == "INFO");
}

bool SaveFile::backupAll()
{
    return SaveFile::backupAll(SaveFile::getAll());
}

bool SaveFile::backupAll(QList<SaveFile *> saveFiles)
{
    // Check if there are savefiles to backup
    if (saveFiles.length() == 0) {
        qDebug() << "No save files found to backup";
        return true;
    }

    // Get the backup dir and make it if it does not exist yet
    QString backupDirPath = QCoreApplication::applicationDirPath() + "/save/backup";
    QDir dir;
    if (!dir.exists(backupDirPath)) {
        dir.mkpath(backupDirPath);
    }

    // Create filename and path
    QString archiveFileName = "keeperfx-save-backup-" + QDate::currentDate().toString("yyyy-MM-dd") + "-v" + KfxVersion::currentVersion.version + ".7z";
    QString archiveFilePath = backupDirPath + "/" + archiveFileName;
    QFile archive(archiveFilePath);

    // Make sure archive output file does not exist
    if (archive.exists()) {
        archive.remove();
    }

    // Debug
    qDebug() << "Save backup archive path:" << archiveFilePath;
    qDebug().noquote() << QString("Archiving %1 save(s)").arg(saveFiles.length()).toStdString();

    // Archive all saves
    for (SaveFile *saveFile : saveFiles) {
        qDebug() << "Archiving:" << saveFile->saveName;
        if (Archiver::compressSingleFile(&saveFile->file, archiveFilePath.toStdString()) == false) {
            return false;
        }
    }

    return true;
}
