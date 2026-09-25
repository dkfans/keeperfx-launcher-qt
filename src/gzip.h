#pragma once

#include <QByteArray>
#include <QDebug>

#include <zlib.h>

namespace GZip {

    /**
     * Compresses input data using gzip.
     *
     * @param input Raw input data
     * @param level Compression level (default Z_BEST_COMPRESSION)
     * @return Gzipped QByteArray, or empty if compression failed
     */
    inline QByteArray compress(const QByteArray &input, int level = Z_BEST_COMPRESSION)
    {
        if (input.isEmpty()) {
            qWarning() << "GZip::Compress: input is empty";
            return {};
        }

        // Preallocate output buffer to maximum possible compressed size
        QByteArray output;
        output.resize(deflateBound(nullptr, input.size()));
        output.fill('\0'); // ensure memory is initialized

        z_stream strm{};
        strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
        strm.avail_in = static_cast<uInt>(input.size());

        strm.next_out = reinterpret_cast<Bytef*>(output.data());
        strm.avail_out = static_cast<uInt>(output.size());

        // Initialize gzip
        int ret = deflateInit2(&strm,
            level,
            Z_DEFLATED,
            15 + 16, // gzip header
            8,       // memLevel
            Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) {
            qWarning() << "GZip::Compress: deflateInit2 failed";
            return {};
        }

        // Loop until stream ends
        do {
            ret = deflate(&strm, Z_FINISH);
        } while (ret == Z_OK);

        deflateEnd(&strm);

        if (ret != Z_STREAM_END) {
            qWarning() << "GZip::Compress: deflate failed with code" << ret;
            return {};
        }

        // Resize output to actual compressed size
        output.resize(strm.total_out);

        // --- Debug logging ---
        auto formatSize = [](qint64 bytes, double &value, QString &unit) {
            if (bytes >= 1024LL * 1024LL) {
                value = bytes / (1024.0 * 1024.0);
                unit = "MiB";
            } else {
                value = bytes / 1024.0;
                unit = "KiB";
            }
        };

        double origValue = 0.0, compValue = 0.0;
        QString origUnit, compUnit;

        formatSize(input.size(), origValue, origUnit);
        formatSize(output.size(), compValue, compUnit);

        double ratioPercent = (origValue > 0.0)
                                  ? (output.size() * 100.0 / input.size())
                                  : 0.0;

        qDebug().noquote()
            << QString("GZip::Compress: %1%3 -> %2%4 (%5%)") // clazy:exclude=qstring-arg
                   .arg(origValue, 0, 'f', 1)
                   .arg(compValue, 0, 'f', 1)
                   .arg(origUnit)
                   .arg(compUnit)
                   .arg(ratioPercent, 0, 'f', 1);

        return output;
    }

}
