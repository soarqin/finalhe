#include "package.hh"

#include "log.hh"

#include <sha256.h>
#include <pkg.h>

#include <QDir>
#include <QCoreApplication>
#include <QThread>

Package::Package(const QString &basePath) {
    pkgBasePath = basePath;
}

Package::~Package() {
}

const char *BSPKG_URL = "http://ares.dl.playstation.net/cdn/JP0741/PCSG90096_00/xGMrXOkORxWRyqzLMihZPqsXAbAXLzvAdJFqtPJLAZTgOcqJobxQAhLNbgiFydVlcmVOrpZKklOYxizQCRpiLfjeROuWivGXfwgkq.pkg";
const char *BSPKG_FILE = "BitterSmile.pkg";
const char *BSPKG_SHA256 = "280a734a0b40eedac2b3aad36d506cd4ab1a38cd069407e514387a49d81b9302";

const char *HENCORE_URL = "https://github.com/TheOfficialFloW/h-encore/releases/download/v1.0/h-encore.zip";
const char *HENCORE_FILE = "h-encore.zip";
const char *HENCORE_SHA256 = "65a5eee6654bb7889e4c0543d09fa4e9eced53c76b25d23e12d23e7a28846b0a";

void Package::downloadDemo() {
    if (download(BSPKG_URL, BSPKG_FILE, BSPKG_SHA256)) {
        startUnpackPackage(BSPKG_FILE);
    }
}

void Package::downloadHencore() {
    if (download(HENCORE_URL, HENCORE_FILE, HENCORE_SHA256)) {
        startUnpackHencore(HENCORE_FILE);
    }
}

bool Package::download(const QString & url, const QString & localFilename, const char *sha256sum) {
    QDir dir(pkgBasePath);
    QString filename = dir.filePath(localFilename);
    QFile file(filename);
    if (file.open(QFile::ReadOnly)) {
        LOG(QString("Found %1, verifying sha256sum...").arg(localFilename));
        QCoreApplication::processEvents();
        sha256_context ctx;
        sha256_init(&ctx);
        sha256_starts(&ctx);
        size_t bufsize = 32 * 1024 * 1024;
        char *buf;
        do {
            bufsize >>= 1;
            buf = new char[bufsize];
        } while (buf == nullptr);
        qint64 rsz;
        while ((rsz = file.read(buf, bufsize)) > 0) {
            sha256_update(&ctx, (const uint8_t*)buf, rsz);
            QCoreApplication::processEvents();
        }
        delete[] buf;
        uint8_t sum[32];
        sha256_final(&ctx, sum);
        QByteArray array((const char*)sum, 32);
        if (QString(array.toHex()) != sha256sum) {
            LOG("sha256sum mismatch, removing file.");
            file.close();
            bool succ = false;
            for (int i = 0; i < 5; ++i) {
                if (file.remove()) {
                    succ = true;
                    break;
                }
                for (int j = 0; j < 20; ++j) {
                    QThread::msleep(100);
                    QCoreApplication::processEvents();
                }
            }
            if (!succ) {
                LOG("Failed to remove old file, operation aborted.");
                return false;
            }
        } else {
            LOG("sha256sum correct.");
            return true;
        }
    }
    return startDownload(url, localFilename);
}

bool Package::startDownload(const QString & url, const QString & localFilename) {
    LOG(QString("Start downloading from %1...").arg(url));
    return true;
}

bool Package::startUnpackPackage(const char *filename) {
    LOG("Unpacking package...");
    pkg_disable_output();
    QString oldDir = QDir::currentPath();
    QDir::setCurrent(pkgBasePath);
    pkg_dec(filename, nullptr);
    QDir::setCurrent(oldDir);
    return true;
}

bool Package::startUnpackHencore(const char *filename) {
    return true;
}
