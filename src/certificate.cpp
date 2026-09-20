#include "certificate.h"

#include <QUrl>
#include <QList>
#include <QSslCertificate>
#include <QSslError>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <stdexcept>

#ifdef Q_OS_WINDOWS
    #include <windows.h>
    #include <wincrypt.h>
    #include <softpub.h>
#endif

QList<QSslCertificate> Certificate::certificateList;

void Certificate::loadAll()
{
    QList<QSslCertificate> certList;

    // Certificates to load
    QStringList certFiles = {
        ":/res/cert/release_certificate_2025.cer",
        ":/res/cert/test_certificate.cer",
    };

    for (const QString &certFilePath : certFiles) {
        QFile certFile(certFilePath);
        if (certFile.open(QIODevice::ReadOnly)) {
            QByteArray certData = certFile.readAll();

            // Try loading as DER or PEM. QSslCertificate handles both, but let's parse it securely.
            QSslCertificate cert(certData);

            if (cert.isNull()) {
                // Try explicitly as DER if PEM failed
                cert = QSslCertificate(certData, QSsl::Der);
            }

            if (!cert.isNull()) {
                certList.append(cert);
                qDebug() << "Loaded certificate:" << cert.subjectDisplayName();
            } else {
                qWarning() << "Failed to parse certificate content:" << certFilePath;
            }
        } else {
            qWarning() << "Failed to open certificate file:" << certFilePath;
            throw std::runtime_error("Failed to load certificates");
        }
    }

    Certificate::certificateList = certList;
}

bool Certificate::verify(QFile &file)
{
    // Load certificates if they are not loaded yet
    if (Certificate::certificateList.isEmpty()) {
        Certificate::loadAll();
    }

    // Make sure we have a certificate to use
    if (Certificate::certificateList.isEmpty()) {
        qWarning() << "No certificates for verification";
        return false;
    }

    // Make sure we have an absolute path for the Windows API
    QFileInfo fileInfo(file);
    QString absolutePath = fileInfo.absoluteFilePath();
    qDebug() << "Verifying:" << absolutePath;

#ifdef Q_OS_WINDOWS
    std::wstring wFilePath = QDir::toNativeSeparators(absolutePath).toStdWString();

    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;
    DWORD dwEncoding = 0;
    DWORD dwContentType = 0;
    DWORD dwFormatType = 0;

    BOOL res = CryptQueryObject(
        CERT_QUERY_OBJECT_FILE,
        wFilePath.c_str(),
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_ALL,
        0,
        &dwEncoding,
        &dwContentType,
        &dwFormatType,
        &hStore,
        &hMsg,
        nullptr
        );

    if (!res) {
        DWORD err = GetLastError();
        qWarning() << "CryptQueryObject failed with code:" << err << "for file:" << absolutePath;
        return false;
    }

    bool foundMatch = false;

    if (hStore) {
        PCCERT_CONTEXT pCertContext = nullptr;

        while ((pCertContext = CertEnumCertificatesInStore(hStore, pCertContext)) != nullptr) {

            QByteArray certData(reinterpret_cast<const char*>(pCertContext->pbCertEncoded), static_cast<int>(pCertContext->cbCertEncoded));
            QSslCertificate signedCertificate(certData, QSsl::Der);

            if (signedCertificate.isNull()) {
                qWarning() << "Extracted certificate is null/invalid.";
                continue;
            }

            // Get the SHA-256 digest of the extracted certificate
            QByteArray extractedDigest = signedCertificate.digest(QCryptographicHash::Sha256);

            // Compare using digests rather than `==` to avoid parser strictness issues
            for (const QSslCertificate &cert : std::as_const(Certificate::certificateList)) {
                if (extractedDigest == cert.digest(QCryptographicHash::Sha256)) {
                    qInfo() << "Certificate verified successfully:" << absolutePath;
                    foundMatch = true;
                    break;
                }
            }

            if (foundMatch) {
                CertFreeCertificateContext(pCertContext);
                break;
            }
        }
        CertCloseStore(hStore, 0);
    } else {
        qWarning() << "No certificate store found in the executable.";
    }

    if (hMsg) {
        CryptMsgClose(hMsg);
    }

    if (!foundMatch) {
        qWarning() << "No matching certificate found in file:" << absolutePath;
    }

    return foundMatch;

#else
    Q_UNUSED(file);
    qDebug() << "Certificate extraction is only supported on Windows.";
    return false;
#endif
}

bool Certificate::verify(QString filePath)
{
    QFile file(filePath);
    return Certificate::verify(file);
}

bool Certificate::verify(QUrl fileUrl)
{
    QFile file(fileUrl.toLocalFile());
    return Certificate::verify(file);
}