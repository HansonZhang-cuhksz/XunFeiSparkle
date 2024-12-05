#ifndef ASK_LLM
#define ASK_LLM

#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QEventLoop>
#include <QObject>

class DocumentQAndA : public QObject {
    Q_OBJECT

public:
    DocumentQAndA(const QString &appId, const QString &apiSecret, const QString &originUrl, QObject *parent = nullptr)
        : QObject(parent), appId(appId), apiSecret(apiSecret), originUrl(originUrl) {
        timestamp = QString::number(QDateTime::currentSecsSinceEpoch());
    }

    QString getOriginSignature() {
        QByteArray data = appId.toUtf8() + timestamp.toUtf8();
        QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
        return QString(hash.toHex());
    }

    QString getSignature() {
        QString signatureOrigin = getOriginSignature();
        QByteArray key = apiSecret.toUtf8();
        QByteArray message = signatureOrigin.toUtf8();

        int blockSize = 64;
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

    QString getUrl() {
        QString signature = getSignature();
        return originUrl + "?" + "appId=" + appId + "&timestamp=" + timestamp + "&signature=" + signature;
    }

    QJsonObject getBody() {
        QJsonObject chatExtends;
        chatExtends["wikiPromptTpl"] = "请将以下内容作为已知信息：\n<wikicontent>\n请根据以上内容回答用户的问题。\n问题:<wikiquestion>\n回答:";
        chatExtends["wikiFilterScore"] = 0.83;
        chatExtends["temperature"] = 0.5;

        QJsonArray fileIds;
        for (const std::string &fileID : fileIDsInput) {
            fileIds.append(QString::fromStdString(fileID));
        }

        QJsonObject message;
        message["role"] = "user";
        message["content"] = msg.c_str();
        messages.append(message);

        QJsonObject data;
        data["chatExtends"] = chatExtends;
        data["fileIds"] = fileIds;
        data["messages"] = messages;

        return data;
    }

private slots:
    void onConnected() {
        QJsonDocument doc(getBody());
        QString jsonString = doc.toJson(QJsonDocument::Compact);
        webSocket.sendTextMessage(jsonString);
    }

    void onTextMessageReceived(QString message) {
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        QJsonObject obj = doc.object();
        int code = obj["code"].toInt();
        if (code != 0) {
            webSocket.close();
        } else {
            QString content = obj["content"].toString();
            int status = obj["status"].toInt();
            connectedContent.append(content);
            if (status == 2) {
                webSocket.close();
            }
        }
    }

    void onError(QAbstractSocket::SocketError error) {
        qWarning() << "WebSocket error:" << error;
    }

    void onClosed() {
        responseDone = true;
        eventLoop.quit();
    }

public:
    void start() {
        responseDone = false;
        QString url = getUrl();
        connect(&webSocket, &QWebSocket::connected, this, &DocumentQAndA::onConnected);
        connect(&webSocket, &QWebSocket::textMessageReceived, this, &DocumentQAndA::onTextMessageReceived);
        connect(&webSocket, &QWebSocket::errorOccurred, this, &DocumentQAndA::onError);
        connect(&webSocket, &QWebSocket::disconnected, this, &DocumentQAndA::onClosed);
        webSocket.open(QUrl(url));

        eventLoop.exec();
    }

    QString connectedContent;
    bool responseDone;

    std::vector<std::string> fileIDsInput;
    std::string msg;

    QJsonArray messages;

private:
    QString appId;
    QString apiSecret;
    QString timestamp;
    QString originUrl;
    QWebSocket webSocket;
    QEventLoop eventLoop;
};

#endif
