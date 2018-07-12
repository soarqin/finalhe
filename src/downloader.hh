#pragma once

#include <QObject>
#include <QList>
#include <QUrl>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QSslError;

class Downloader : public QObject {
    Q_OBJECT

private:
    enum class RequestType {
        File,
        Get,
    };
    struct Request {
        RequestType type;
        QNetworkReply *reply;
    };
    struct RequestFile : public Request {
        RequestFile() { type = RequestType::File; }
        QFile *targetFile;
    };
    struct RequestGet : public Request {
        RequestGet() { type = RequestType::Get; }
        void *getArg;
    };

public:
    Downloader();
    void doDownload(const QUrl &url, QFile *fileToWrite);
    void doGet(const QUrl &url, void *arg);

signals:
    void finishedFile(QFile*);
    void finishedGet(void *arg);

public slots:
    void downloadFinished(QNetworkReply *reply);
    void readyReadFileReply(RequestFile*);
    void readyReadGetReply(RequestGet*);
#if QT_CONFIG(ssl)
    void sslErrors(const QList<QSslError> &errors);
#endif

private:
    QNetworkAccessManager manager;
    QList<Request*> requests;
    void *getArg = nullptr;
};
