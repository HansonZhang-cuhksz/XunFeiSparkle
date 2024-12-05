#ifndef UPLOAD_FILE
#define UPLOAD_FILE

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QFile>
#include <QEventLoop>
#include <QObject>

class DocumentUpload : public QObject {
    Q_OBJECT

public:
    DocumentUpload(const QString &appId, const QString &apiSecret, const QString &timestamp, QObject *parent = nullptr)
        : QObject(parent), appId(appId), apiSecret(apiSecret), timestamp(timestamp) {}

    QString getOriginSignature() {
        QByteArray data = appId.toUtf8() + timestamp.toUtf8();
        QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
        return QString(hash.toHex());
    }

    QString getSignature() {
        QString signatureOrigin = getOriginSignature();
        QByteArray key = apiSecret.toUtf8();
        QByteArray message = signatureOrigin.toUtf8();

        int blockSize = 64; // HMAC block size for SHA1
        if (key.size() > blockSize) {
            key = QCryptographicHash::hash(key, QCryptographicHash::Sha1);
        }
        if (key.size() < blockSize) {
            key = key.leftJustified(blockSize, '\0');
        }

        QByteArray oKeyPad(blockSize, 0x5c);
        QByteArray iKeyPad(blockSize, 0x36);

        for (int i = 0; i < blockSize; ++i) {
            oKeyPad[i] = oKeyPad[i] ^ key[i];
            iKeyPad[i] = iKeyPad[i] ^ key[i];
        }

        QByteArray innerHash = QCryptographicHash::hash(iKeyPad + message, QCryptographicHash::Sha1);
        QByteArray hash = QCryptographicHash::hash(oKeyPad + innerHash, QCryptographicHash::Sha1);

        return QString(hash.toBase64());
    }

    QNetworkRequest getHeader() {
        QString signature = getSignature();
        QNetworkRequest request;
        request.setRawHeader("appId", appId.toUtf8());
        request.setRawHeader("timestamp", timestamp.toUtf8());
        request.setRawHeader("signature", signature.toUtf8());
        return request;
    }

    QByteArray uploadDocument(const QString &filePath) {
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        QNetworkRequest request = getHeader();
        request.setUrl(QUrl("https://chatdoc.xfyun.cn/openapi/v1/file/upload"));

        QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QFile *file = new QFile(filePath);
        if (!file->open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open file for reading:" << filePath;
            return QByteArray();
        }

        QHttpPart filePart;
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"" + file->fileName() + "\""));
        filePart.setBodyDevice(file);
        file->setParent(multiPart); // file will be deleted along with multiPart
        multiPart->append(filePart);

        QHttpPart textPart;
        textPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"fileName\""));
        textPart.setBody("beiying.txt");
        multiPart->append(textPart);

        QHttpPart typePart;
        typePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"fileType\""));
        typePart.setBody("wiki");
        multiPart->append(typePart);

        QHttpPart callbackPart;
        callbackPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"callbackUrl\""));
        callbackPart.setBody("your_callbackUrl");
        multiPart->append(callbackPart);

        QNetworkReply *reply = manager->post(request, multiPart);
        multiPart->setParent(reply); // multiPart will be deleted along with reply

        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            //qDebug() << "Response:" << responseData;
            return responseData;
        } else {
            qWarning() << "Error:" << reply->errorString();
            return QByteArray();
        }

        reply->deleteLater();
    }

private:
    QString appId;
    QString apiSecret;
    QString timestamp;
};

#endif
