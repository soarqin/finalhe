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
    Downloader(QObject *obj_parent = 0);
    void doDownload(const QUrl &url, QFile *fileToWrite);
    void doGet(const QUrl &url, void *arg);

signals:
    void downloadProgress(uint64_t curr, uint64_t total);
    void finishedFile(QFile *file);
    void finishedGet(void *arg);

private slots:
    void downloadFinished(QNetworkReply *reply);
    void readyReadFileReply(RequestFile*);
    void readyReadGetReply(RequestGet*);
    void downloadProg(qint64 curr, qint64 total);
#ifndef QT_NO_SSL
    void sslErrors(const QList<QSslError> &errors);
#endif

private:
    QNetworkAccessManager manager;
    QList<Request*> requests;
    void *getArg = nullptr;
};
