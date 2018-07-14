#include "downloader.hh"

#include <cstdio>

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
        fprintf(stderr, "SSL error: %s\n", qPrintable(error.errorString()));
    Q_UNUSED(sslErrors);
}
#endif

void Downloader::readyReadFileReply(RequestFile *rf) {
    rf->targetFile->write(rf->reply->readAll());
}

void Downloader::readyReadGetReply(RequestGet *rg) {
    ((QString*)rg->getArg)->append(rg->reply->readAll());
}

void Downloader::downloadFinished(QNetworkReply *reply) {
    QUrl url = reply->url();
    if (reply->error()) {
        fprintf(stderr, "Fetch from %s failed: %s\n",
            url.toEncoded().constData(),
            qPrintable(reply->errorString()));
    } else {
        printf("Fetch from %s succeeded\n",
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
