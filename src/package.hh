#pragma once

#include "downloader.hh"

#include <QString>
#include <QObject>
#include <QQueue>
#include <QPair>

class Package : public QObject {
    Q_OBJECT
public:
    Package(const QString &basePath, QObject *obj_parent = 0);
    virtual ~Package();

    inline void setTrim(bool t) { trimApp = t; }
    void tips();

signals:
    void startDownload();
    void noHencoreFull();
    void unpackedDemo();
    void unpackedZip(QString);
    void createdPsvImgs();
    void gotBackupKey();
    void setStatusText(QString);
    void setPercent(int);
    void unpackNext();

private:
    void get(const QString &url, QString &result);
    void download(const QString &url, const QString &localFilename, const char *sha256sum);
    void startDownload(const QString &url, const QString &localFilename);
    void startUnpackDemo(const char *filename);
    void genExtraUnpackList();
    void startUnpackZipsFull();
    bool verify(const QString &filepath, const char *sha256sum);
    void createPsvImgSingleDir(const QString &titleID, const char *singleDir);

public slots:
    void getBackupKey(const QString &aid);
    void finishBuildData();

private slots:
    void doUnpackZip();
    void checkHencoreFull();
    void downloadDemo();
    void startUnpackZips();
    void createPsvImgs(QString);
    void downloadProg(uint64_t, uint64_t);
    void downloadFinished(QFile*);
    void fetchFinished(void*);

private:
    bool trimApp = false;
    QString pkgBasePath;
    Downloader downloader;
    QString accountId;
    QString backupKey;
    QQueue<QPair<QString, QString>> unzipQueue;
};
