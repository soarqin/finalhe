#pragma once

#include "downloader.hh"

#include <QString>
#include <QObject>

class Package : public QObject {
    Q_OBJECT
public:
    Package(const QString &basePath, QObject *obj_parent = 0);
    virtual ~Package();

    inline void setTrim(bool t) { trimApp = t; }

signals:
    void startDownload();
    void unpackedDemo();
    void unpackedHencore();
    void createdPsvImgs();
    void gotBackupKey();

private:
    void get(const QString &url, QString &result);
    void download(const QString &url, const QString &localFilename, const char *sha256sum);
    void startDownload(const QString &url, const QString &localFilename);
    bool startUnpackDemo(const char *filename);
    bool startUnpackHencore(const char *filename);
    bool verify(const QString &filepath, const char *sha256sum);

public slots:
    void getBackupKey(const QString &aid);

private slots:
    void downloadDemo();
    void downloadHencore();
    void createPsvImgs();
    void downloadFinished(QFile*);
    void fetchFinished(void*);

private:
    bool trimApp = false;
    QString pkgBasePath;
    Downloader downloader;
    QString accountId;
    QString backupKey;
};
