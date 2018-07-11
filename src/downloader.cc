#include <QtCore>
#include <QtNetwork>

#include <cstdio>

class QSslError;

using namespace std;

class Downloader : public QObject {
    Q_OBJECT
    QNetworkAccessManager manager;
    QNetworkReply *currentReply = nullptr;
    QFile *targetFile = nullptr;

public:
    Downloader();
    void doDownload(const QUrl &url, QFile *fileToWrite);
    static bool isHttpRedirect(QNetworkReply *reply);

public slots:
    void downloadFinished(QNetworkReply *reply);
    void readyReadReply();
    void sslErrors(const QList<QSslError> &errors);
};

Downloader::Downloader() {
    manager.setRedirectPolicy(QNetworkRequest::RedirectPolicy::NoLessSafeRedirectPolicy);
    connect(&manager, SIGNAL(finished(QNetworkReply*)),
        SLOT(downloadFinished(QNetworkReply*)));
}

void Downloader::doDownload(const QUrl &url, QFile *fileToWrite) {
    targetFile = fileToWrite;
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);
    currentReply = reply;

    connect(reply, SIGNAL(readyRead()), SLOT(readyReadReply()));

#if QT_CONFIG(ssl)
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
        SLOT(sslErrors(QList<QSslError>)));
#endif
}

void Downloader::sslErrors(const QList<QSslError> &sslErrors) {
#if QT_CONFIG(ssl)
    for (const QSslError &error : sslErrors)
        fprintf(stderr, "SSL error: %s\n", qPrintable(error.errorString()));
#else
    Q_UNUSED(sslErrors);
#endif
}

void Downloader::readyReadReply() {
    targetFile->write(currentReply->readAll());
}

void Downloader::downloadFinished(QNetworkReply *reply) {
    QUrl url = reply->url();
    if (reply->error()) {
        fprintf(stderr, "Download of %s failed: %s\n",
            url.toEncoded().constData(),
            qPrintable(reply->errorString()));
    } else {
        printf("Download of %s succeeded\n",
            url.toEncoded().constData());
    }

    reply->deleteLater();
    currentReply = nullptr;
}

#include "downloader.moc"
