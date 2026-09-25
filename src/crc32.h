#pragma once

#include <QFile>
#include <QByteArray>
#include <QString>
#include <QDebug>

#include <zlib.h>

namespace CRC32 {

    inline QString calculate(const QString &filePath) {

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open file:" << filePath;
            return QString();
        }

        quint32 crc = crc32(0L, Z_NULL, 0);  // Initialize CRC
        const int bufferSize = 64 * 1024;    // Read in 64 KB chunks
        QByteArray buffer(bufferSize, Qt::Uninitialized);

        while (!file.atEnd()) {
            qint64 bytesRead = file.read(buffer.data(), bufferSize);
            if (bytesRead > 0) {
                crc = crc32(crc, reinterpret_cast<const Bytef *>(buffer.constData()), static_cast<uInt>(bytesRead));
            }
        }

        // Convert to hex string, ensuring it's 8 characters long (zero-padded)
        return QString("%1").arg(crc, 8, 16, QLatin1Char('0'));
    }

}
