#pragma once

#include <QString>

class Package {
public:
    Package(const QString &basePath);
    virtual ~Package();

    void downloadDemo();
    void downloadHencore();

private:
    bool download(const QString &url, const QString &localFilename, const char *sha256sum);
    bool startDownload(const QString &url, const QString &localFilename);
    bool startUnpackPackage(const char *filename);
    bool startUnpackHencore(const char *filename);

private:
    QString pkgBasePath;
};
