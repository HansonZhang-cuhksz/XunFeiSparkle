#include <QCoreApplication>
#include <QTimer>

#include "uploadFile.hpp"
#include "askLLM.hpp"
#include "callAI.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <cassert>

const QString appId = "fe374d6f";
const QString apiSecret = "YzE5YjkyNDRlNDVmMDA4NjUwMjVmOWJl";
const QString originUrl = "wss://chatdoc.xfyun.cn/openapi/chat";

void sleep(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

class Workspace{
public:
    Workspace(std::string workspaceName) {
        name = workspaceName;
    };

    void addFile(std::string filePath, std::string fileName) {
        assert(files.find(fileName) == files.end());

        QString curTime = QString::number(QDateTime::currentSecsSinceEpoch());
        DocumentUpload uploader(appId, apiSecret, curTime);
        QByteArray responseData = uploader.uploadDocument(QString::fromStdString(filePath));
        if (responseData.isEmpty()) {
            qWarning() << "No response received.";
        }

        QString jsonString = QString::fromUtf8(responseData);
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << parseError.errorString();
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        std::string fileID = jsonObj["data"].toObject()["fileId"].toString().toStdString();

        files[fileName] = fileID;
        sleep(1000);
    }

    void deleteFile(std::string fileName) {
        assert(files.find(fileName) != files.end());
        files.erase(fileName);
    }

    std::string ask(std::string msg) {
        std::vector<std::string> fileIDs;
        for (const auto& pair : files) {
            fileIDs.push_back(pair.second);
        }

        DocumentQAndA qAndA(appId, apiSecret, originUrl);
        qAndA.fileIDsInput = fileIDs;
        qAndA.msg = msg;
        qAndA.messages = memoryJson;

        qAndA.start();

        if(qAndA.responseDone) {
            QString resp = qAndA.connectedContent;
            qDebug() << "Resp is: " << resp;
        }

        QString resp = qAndA.connectedContent;
        qDebug() << "Resp is: " << resp;
        return resp.toStdString();
    }

    std::string name;
    std::unordered_map<std::string, std::string> files;
    QJsonArray memoryJson;
};

std::unordered_map<std::string, Workspace*> workspaces;

void create_workspace(std::string workspaceName) {
    Workspace* workspacePtr = new Workspace(workspaceName);
    workspaces[workspaceName] = workspacePtr;
}

void delete_workspace(std::string workspaceName) {
    assert(workspaces.find(workspaceName) != workspaces.end());
    delete workspaces[workspaceName];
    workspaces.erase(workspaceName);
}

void add_file(std::string workspaceName, std::string filePath, std::string fileName) {
    assert(workspaces.find(workspaceName) != workspaces.end());
    workspaces[workspaceName]->addFile(filePath, fileName);
}

void delete_file(std::string workspaceName, std::string fileName) {
    assert(workspaces.find(workspaceName) != workspaces.end());
    workspaces[workspaceName]->deleteFile(fileName);
}

/*std::string request(std::string workspaceName, std::string msg) {
    assert(workspaces.find(workspaceName) != workspaces.end());
    return workspaces[workspaceName]->ask(msg);
}*/

void clear_memory(std::string workspaceName) {
    assert(workspaces.find(workspaceName) != workspaces.end());
    workspaces[workspaceName]->memoryJson = QJsonArray();
}

std::string request(std::string workspaceName, std::string msg){
    assert(workspaces.find(workspaceName) != workspaces.end());

    std::vector<std::string> fileIDs;
    for(auto pair : workspaces[workspaceName]->files) {
        qDebug() << pair.second;
        fileIDs.push_back(pair.second);
    }

    DocumentQAndA qAndA(appId, apiSecret, originUrl);
    qAndA.fileIDsInput = fileIDs;
    qAndA.msg = msg;

    qAndA.start();

    return qAndA.connectedContent.toStdString();
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    create_workspace("Test Workspace");
    add_file("Test Workspace", "C:\\Users\\shuhan\\Downloads\\beiying.txt", "beiying");

    sleep(1000);

    std::string resp = request("Test Workspace", "What did the father bought at the station?");
    qDebug() << "Resp is: " << resp;

    return a.exec();
}
