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
    void tips();

signals:
    void startDownload();
    void noHencoreFull();
    void unpackedDemo();
    void unpackedHencore();
    void createdPsvImgs();
    void gotBackupKey();
    void setStatusText(QString);
    void setPercent(int);

private:
    void get(const QString &url, QString &result);
    void download(const QString &url, const QString &localFilename, const char *sha256sum);
    void startDownload(const QString &url, const QString &localFilename);
    void startUnpackDemo(const char *filename);
    void startUnpackHencore(const char *filename);
    bool verify(const QString &filepath, const char *sha256sum);

public slots:
    void getBackupKey(const QString &aid);
    void finishBuildData();

private slots:
    void checkHencoreFull();
    void downloadDemo();
    void downloadHencore();
    void createPsvImgs();
    void downloadProg(uint64_t, uint64_t);
    void downloadFinished(QFile*);
    void fetchFinished(void*);

private:
    bool trimApp = false;
    QString pkgBasePath;
    Downloader downloader;
    QString accountId;
    QString backupKey;
};
