/*
 *  Final h-encore, Copyright (C) 2018  Soar Qin
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "downloader.hh"

#include <cstdio>
#include <QDebug>

Downloader::Downloader(QObject *obj_parent): QObject(obj_parent) {
//    manager.setRedirectPolicy(QNetworkRequest::RedirectPolicy::NoLessSafeRedirectPolicy);
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
#ifndef QT_NO_SSL
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
#ifndef QT_NO_SSL
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
        SLOT(sslErrors(QList<QSslError>)));
#endif
}

#ifndef QT_NO_SSL
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
