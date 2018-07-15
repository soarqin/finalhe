#include "downloader.hh"

#include <cstdio>
#include <QDebug>

Downloader::Downloader(QObject *obj_parent): QObject(obj_parent) {
    manager.setRedirectPolicy(QNetworkRequest::RedirectPolicy::NoLessSafeRedirectPolicy);
    connect(&manager, SIGNAL(finished(QNetworkReply*)),
        SLOT(downloadFinished(QNetworkReply*)));
}

void Downloader::doDownload(const QUrl &url, QFile *fileToWrite) {
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);
    RequestFile *rf = new RequestFile;
    rf->reply = reply;
    rf->targetFile = fileToWrite;
    requests.push_back(rf);

    connect(reply, &QNetworkReply::readyRead, this, [this, rf] { readyReadFileReply(rf); });
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProg(qint64, qint64)));
#if QT_CONFIG(ssl)
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
        SLOT(sslErrors(QList<QSslError>)));
#endif
}

void Downloader::doGet(const QUrl &url, void * arg) {
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);
    RequestGet *rg = new RequestGet;
    rg->reply = reply;
    rg->getArg = arg;
    requests.push_back(rg);

    connect(reply, &QNetworkReply::readyRead, this, [this, rg] { readyReadGetReply(rg); });
#if QT_CONFIG(ssl)
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
        SLOT(sslErrors(QList<QSslError>)));
#endif
}

#if QT_CONFIG(ssl)
void Downloader::sslErrors(const QList<QSslError> &sslErrors) {
    for (const QSslError &error : sslErrors)
        qWarning("SSL error: %s", error.errorString().toUtf8().constData());
    Q_UNUSED(sslErrors);
}
#endif

void Downloader::readyReadFileReply(RequestFile *rf) {
    rf->targetFile->write(rf->reply->readAll());
}

void Downloader::readyReadGetReply(RequestGet *rg) {
    ((QString*)rg->getArg)->append(rg->reply->readAll());
}

void Downloader::downloadProg(qint64 curr, qint64 total) {
    emit downloadProgress(curr, total);
}

void Downloader::downloadFinished(QNetworkReply *reply) {
    QUrl url = reply->url();
    if (reply->error()) {
        qWarning("Fetch from %s failed: %s",
            url.toEncoded().constData(),
            reply->errorString().toUtf8().constData());
    } else {
        qDebug("Fetch from %s succeeded",
            url.toEncoded().constData());
    }

    for (int i = 0; i < requests.size(); ++i) {
        if (requests[i]->reply == reply) {
            switch (requests[i]->type) {
            case RequestType::File:
                emit finishedFile(((RequestFile*)requests[i])->targetFile);
                break;
            case RequestType::Get:
                emit finishedGet(((RequestGet*)requests[i])->getArg);
                break;
            }
            delete requests[i];
            requests.removeAt(i);
            break;
        }
    }
    reply->deleteLater();
}
