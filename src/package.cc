#include "package.hh"

#include "log.hh"

#include <sha256.h>
#include <pkg.h>
#include <psvimg-create.h>
#include <miniz_zip.h>

#include <QCoreApplication>
#include <QDir>
#include <QThread>
#include <QMessageBox>

const char *HENCORE_URL = "https://github.com/TheOfficialFloW/h-encore/releases/download/v1.0/h-encore.zip";
const char *HENCORE_FILE = "h-encore.zip";
const char *HENCORE_SHA256 = "65a5eee6654bb7889e4c0543d09fa4e9eced53c76b25d23e12d23e7a28846b0a";

const char *BSPKG_URL = "http://ares.dl.playstation.net/cdn/JP0741/PCSG90096_00/xGMrXOkORxWRyqzLMihZPqsXAbAXLzvAdJFqtPJLAZTgOcqJobxQAhLNbgiFydVlcmVOrpZKklOYxizQCRpiLfjeROuWivGXfwgkq.pkg";
const char *BSPKG_FILE = "BitterSmile.pkg";
const char *BSPKG_SHA256 = "280a734a0b40eedac2b3aad36d506cd4ab1a38cd069407e514387a49d81b9302";

Package::Package(const QString &basePath) {
    pkgBasePath = basePath;
    connect(&downloader, SIGNAL(finishedFile(QFile*)), SLOT(downloadFinished(QFile*)));
    connect(&downloader, SIGNAL(finishedGet(void*)), SLOT(fetchFinished(void*)));
    connect(this, SIGNAL(startDownload()), SLOT(downloadDemo()));
    connect(this, SIGNAL(unpackedDemo()), SLOT(downloadHencore()));
    connect(this, SIGNAL(unpackedHencore()), SLOT(createPsvImgs()));
}

Package::~Package() {
}

void Package::get(const QString &url, QString &result) {
    downloader.doGet(url, &result);
}

bool Package::download(const QString & url, const QString & localFilename, const char *sha256sum) {
    QDir dir(pkgBasePath);
    QString filename = dir.filePath(localFilename);
    if (QFile::exists(filename)) {
        if (verify(filename, sha256sum)) return true;
        LOG(tr("Removing old file."));
        bool succ = false;
        for (int i = 0; i < 5; ++i) {
            if (QFile::remove(filename)) {
                succ = true;
                break;
            }
            for (int j = 0; j < 20; ++j) {
                QThread::msleep(100);
                QCoreApplication::processEvents();
            }
        }
        if (!succ) {
            LOG(tr("Failed to remove old file, operation aborted."));
            return false;
        }
    }
    startDownload(url, filename);
    return false;
}

void Package::startDownload(const QString &url, const QString & localFilename) {
    LOG(tr("Start downloading from %1...").arg(url));
    QFile *f = new QFile(localFilename);
    f->open(QFile::WriteOnly | QFile::Truncate);
    downloader.doDownload(url, f);
}

bool Package::startUnpackDemo(const char *filename) {
    LOG(tr("Unpacking %1...").arg(filename));
    pkg_disable_output();
    QString oldDir = QDir::currentPath();
    QDir curr(pkgBasePath);
    if (curr.cd("h-encore")) {
        curr.removeRecursively();
        curr.cdUp();
    }
    curr.mkpath("h-encore");
    QDir::setCurrent(pkgBasePath);
    pkg_dec(filename, "h-encore/", nullptr);
    curr.cd("h-encore");
    curr.cd("app");
    curr.rename("PCSG90096", "ux0_temp_game_PCSG90096_app_PCSG90096");
    LOG(tr("Done."));
    QDir::setCurrent(oldDir);
    emit unpackedDemo();
    return true;
}

bool Package::startUnpackHencore(const char *filename) {
    LOG(tr("Unpacking %1...").arg(filename));
    QDir dir(pkgBasePath);
    QFile file(dir.filePath(filename));
    file.open(QFile::ReadOnly);
    mz_zip_archive arc;
    mz_zip_zero_struct(&arc);
    arc.m_pIO_opaque = &file;
    arc.m_pRead = [](void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n)->size_t {
        QFile *f = (QFile*)pOpaque;
        if (!f->seek(file_ofs)) return 0;
        return f->read((char*)pBuf, n);
    };
    if (!mz_zip_reader_init(&arc, file.size(), 0)) {
        file.close();
        LOG(tr("Unable to decompress %1").arg(filename));
        return false;
    }
    uint32_t num = mz_zip_reader_get_num_files(&arc);
    for (uint32_t i = 0; i < num; ++i) {
        char zfilename[1024];
        mz_zip_reader_get_filename(&arc, i, zfilename, 1024);
        if (mz_zip_reader_is_file_a_directory(&arc, i)) {
            dir.mkpath(zfilename);
        } else {
            QFile wfile(dir.filePath(zfilename));
            wfile.open(QFile::WriteOnly | QFile::Truncate);
            mz_zip_reader_extract_to_callback(&arc, i,
                [](void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n)->size_t {
                    QFile *f = (QFile*)pOpaque;
                    return f->write((const char*)pBuf, n);
                }, &wfile, 0);
            wfile.close();
        }
        LOG(zfilename);
    }
    mz_zip_reader_end(&arc);
    file.close();
    QFile::copy(pkgBasePath + "/h-encore/app/ux0_temp_game_PCSG90096_app_PCSG90096/sce_sys/package/temp.bin",
        pkgBasePath + "/h-encore/license/ux0_temp_game_PCSG90096_license_app_PCSG90096/6488b73b912a753a492e2714e9b38bc7.rif");
    LOG(tr("Done."));
    emit unpackedHencore();
    return true;
}

bool Package::verify(const QString & filepath, const char * sha256sum) {
    QFile file(filepath);
    if (file.open(QFile::ReadOnly)) {
        LOG(tr("Verifying sha256sum for %1...").arg(filepath));
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
        file.close();
        uint8_t sum[32];
        sha256_final(&ctx, sum);
        QByteArray array((const char*)sum, 32);
        if (QString(array.toHex()) == sha256sum) {
            LOG(tr("sha256sum correct."));
            return true;
        } else {
            LOG(tr("sha256sum mismatch."));
        }
    }
    return false;
}

void Package::getBackupKey(const QString &aid) {
    get(QString("http://cma.henkaku.xyz/?aid=%1").arg(aid), backupKey);
}

void Package::downloadDemo() {
    if (download(BSPKG_URL, BSPKG_FILE, BSPKG_SHA256))
        startUnpackDemo(BSPKG_FILE);
}

void Package::downloadHencore() {
    if (download(HENCORE_URL, HENCORE_FILE, HENCORE_SHA256))
        startUnpackHencore(HENCORE_FILE);
}

bool Package::createPsvImgs() {
    /*
    psvimg-create -n app -K YOUR_KEY app PCSG90096/app
    psvimg-create -n appmeta -K YOUR_KEY appmeta PCSG90096/appmeta
    psvimg-create -n license -K YOUR_KEY license PCSG90096/license
    psvimg-create -n savedata -K YOUR_KEY savedata PCSG90096/savedata
    */
    QString oldDir = QDir::currentPath();
    QDir curr(pkgBasePath);
    curr.cd("h-encore");
    QDir::setCurrent(curr.path());
    psvimg_create("app", "PCSG90096/app", backupKey.toUtf8().constData(), "app", 0);
    psvimg_create("appmeta", "PCSG90096/appmeta", backupKey.toUtf8().constData(), "appmeta", 0);
    psvimg_create("license", "PCSG90096/license", backupKey.toUtf8().constData(), "license", 0);
    psvimg_create("savedata", "PCSG90096/savedata", backupKey.toUtf8().constData(), "savedata", 0);
    QDir::setCurrent(oldDir);
    return false;
}

void Package::fetchFinished(void *arg) {
    QString *str = (QString*)arg;
    int index = str->indexOf("</b>: ");
    if (index < 0) {
        str->clear();
        LOG(tr("Cannot get backup key from your AID, please check your network connection!"));
        return;
    }
    *str = str->mid(index + 6, 64);
    emit gotBackupKey();
}

void Package::downloadFinished(QFile *f) {
    QString filepath = f->fileName();
    QFileInfo fi(filepath);
    delete f;
    if (fi.fileName() == HENCORE_FILE) {
        if (verify(filepath, HENCORE_SHA256)) {
            startUnpackHencore(HENCORE_FILE);
        }
    } else if (fi.fileName() == BSPKG_FILE) {
        if (verify(filepath, BSPKG_SHA256)) {
            startUnpackDemo(BSPKG_FILE);
        }
    }
}
