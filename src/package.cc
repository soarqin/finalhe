#include "package.hh"

#include "worker.hh"

#include <sha256.h>
#include <pkg.h>
#include <psvimg-create.h>
#include <miniz_zip.h>

#include <QDir>
#include <QDebug>

const char *HENCORE_URL = "https://github.com/TheOfficialFloW/h-encore/releases/download/v1.0/h-encore.zip";
const char *HENCORE_FILE = "h-encore.zip";
const char *HENCORE_SHA256 = "65a5eee6654bb7889e4c0543d09fa4e9eced53c76b25d23e12d23e7a28846b0a";

const char *BSPKG_URL = "http://ares.dl.playstation.net/cdn/JP0741/PCSG90096_00/xGMrXOkORxWRyqzLMihZPqsXAbAXLzvAdJFqtPJLAZTgOcqJobxQAhLNbgiFydVlcmVOrpZKklOYxizQCRpiLfjeROuWivGXfwgkq.pkg";
const char *BSPKG_FILE = "BitterSmile.pkg";
const char *BSPKG_SHA256 = "280a734a0b40eedac2b3aad36d506cd4ab1a38cd069407e514387a49d81b9302";

Package::Package(const QString &basePath, QObject *obj_parent): QObject(obj_parent), downloader(obj_parent) {
    pkgBasePath = basePath;
    connect(&downloader, SIGNAL(finishedFile(QFile*)), this, SLOT(downloadFinished(QFile*)));
    connect(&downloader, SIGNAL(finishedGet(void*)), this, SLOT(fetchFinished(void*)));
    connect(&downloader, SIGNAL(downloadProgress(uint64_t, uint64_t)), this, SLOT(downloadProg(uint64_t, uint64_t)));
    connect(this, SIGNAL(startDownload()), SLOT(downloadDemo()));
    connect(this, SIGNAL(unpackedDemo()), SLOT(downloadHencore()));
    connect(this, SIGNAL(unpackedHencore()), SLOT(createPsvImgs()));
}

Package::~Package() {
}

void Package::tips() {
    emit setStatusText(tr("Launch Content Manager on PS Vita and connect to computer."));
}

void Package::get(const QString &url, QString &result) {
    downloader.doGet(url, &result);
}

void Package::download(const QString &url, const QString &localFilename, const char *sha256sum) {
    QDir dir(pkgBasePath);
    QString filename = dir.filePath(localFilename);
    if (QFile::exists(filename)) {
        static int n = 0;
        Worker::start(this, [this, filename, sha256sum](void *arg) {
            *(int*)arg = 0;
            if (verify(filename, sha256sum)) {
                *(int*)arg = 1;
                return;
            }
            qDebug("Removing old file.");
            bool succ = false;
            for (int i = 0; i < 5; ++i) {
                if (QFile::remove(filename)) {
                    succ = true;
                    break;
                }
                for (int j = 0; j < 20; ++j) {
                    QThread::msleep(100);
                }
            }
            if (!succ) {
                qDebug("Failed to remove old file, operation aborted.");
                *(int*)arg = 2;
                return;
            }
        }, [this, url, filename](void *arg) {
            switch (*(int*)arg) {
            case 0:
                startDownload(url, filename);
                break;
            case 1: {
                QFileInfo fi(filename);
                if (fi.fileName() == BSPKG_FILE)
                    startUnpackDemo(BSPKG_FILE);
                else if (fi.fileName() == HENCORE_FILE)
                    startUnpackHencore(HENCORE_FILE);
                break;
            }
            }
        }, &n);
    }
}

void Package::startDownload(const QString &url, const QString & localFilename) {
    qDebug("Start downloading from %s...", url.toUtf8().constData());
    emit setStatusText(tr("Downloading %1").arg(QFileInfo(localFilename).fileName()));
    QFile *f = new QFile(localFilename);
    f->open(QFile::WriteOnly | QFile::Truncate);
    downloader.doDownload(url, f);
}

static uint64_t demoSize = 0;

void _output_progress_init(void *arg, uint64_t size) {
    demoSize = size;
}

void _output_progress(void *arg, uint64_t progress) {
    emit ((Package*)arg)->setPercent((int)(progress * 100ULL / demoSize));
}

bool Package::startUnpackDemo(const char *filename) {
    static bool succ = false;
    Worker::start(this, [this, filename](void *arg) {
        *(bool*)arg = false;
        qDebug("Unpacking %s...", filename);
        emit setStatusText(tr("Unpacking %1").arg(filename));
        pkg_disable_output();
        pkg_set_func(nullptr, nullptr, _output_progress_init, _output_progress, this);
        QString oldDir = QDir::currentPath();
        QDir curr(pkgBasePath);
        if (curr.cd("h-encore")) {
            curr.removeRecursively();
            curr.cdUp();
        }
        curr.mkpath("h-encore");
        QDir::setCurrent(pkgBasePath);
        int res = pkg_dec(filename, "h-encore/", nullptr);
        if (res < 0) {
            qWarning("Failed to unpack %s!", filename);
            emit setStatusText(tr("Failed to unpack %1").arg(filename));
        } else {
            curr.cd("h-encore");
            curr.cd("app");
            curr.rename("PCSG90096", "ux0_temp_game_PCSG90096_app_PCSG90096");
            *(bool*)arg = true;
        }
        QDir::setCurrent(oldDir);
    }, [this](void *arg) {
        if (*(bool*)arg) {
            qDebug("Done.");
            emit unpackedDemo();
        }
    }, &succ);
    return true;
}

bool Package::startUnpackHencore(const char *filename) {
    static bool succ = false;
    Worker::start(this, [this, filename](void *arg) {
        emit setPercent(0);
        *(bool*)arg = false;
        qDebug("Decompressing %s...", filename);
        emit setStatusText(tr("Decompressing %1").arg(filename));
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
            qWarning("Unable to decompress %s!", filename);
            emit setStatusText(tr("Failed to decompress %1").arg(filename));
            return;
        }
        uint32_t num = mz_zip_reader_get_num_files(&arc);
        for (uint32_t i = 0; i < num; ++i) {
            emit setPercent(i * 100 / num);
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
        }
        mz_zip_reader_end(&arc);
        file.close();
        QFile::copy(pkgBasePath + "/h-encore/app/ux0_temp_game_PCSG90096_app_PCSG90096/sce_sys/package/temp.bin",
            pkgBasePath + "/h-encore/license/ux0_temp_game_PCSG90096_license_app_PCSG90096/6488b73b912a753a492e2714e9b38bc7.rif");
        emit setPercent(100);
        *(bool*)arg = true;
    }, [this](void *arg) {
        if (*(bool*)arg) {
            qDebug("Done.");
            emit unpackedHencore();
        }
    }, &succ);
    return true;
}

bool Package::verify(const QString &filepath, const char *sha256sum) {
    QFile file(filepath);
    if (file.open(QFile::ReadOnly)) {
        qDebug("Verifying sha256sum for %s...", filepath.toUtf8().constData());
        emit setStatusText(tr("Verifying %1").arg(QFileInfo(filepath).fileName()));
        sha256_context ctx;
        sha256_init(&ctx);
        sha256_starts(&ctx);
        size_t bufsize = 2 * 1024 * 1024;
        char *buf;
        do {
            bufsize >>= 1;
            buf = new char[bufsize];
        } while (buf == nullptr);
        qint64 fsz = file.size();
        qint64 curr = 0;
        qint64 rsz;
        emit setPercent(0);
        while ((rsz = file.read(buf, bufsize)) > 0) {
            sha256_update(&ctx, (const uint8_t*)buf, rsz);
            curr += rsz;
            emit setPercent(curr * 100LL / fsz);
        }
        delete[] buf;
        file.close();
        uint8_t sum[32];
        sha256_final(&ctx, sum);
        QByteArray array((const char*)sum, 32);
        if (QString(array.toHex()) == sha256sum) {
            qDebug("sha256sum correct.");
            return true;
        } else {
            qWarning("sha256sum mismatch.");
        }
    }
    return false;
}

void Package::finishBuildData() {
    emit setPercent(100);
    emit setStatusText(tr("Everything is ready, now follow below steps on your PS Vita:\n"
    "1. Launch Content Manager and connect to your computer.\n"
    "2. Select \"PC -> PS Vita System\" -> \"Applications\" -> \"PS Vita\".\n"
    "3. Transfer \"h-encore\" to your PS Vita.\n"
    "4. Run \"h-encore\" and... Yay, that's it!"));
}

void Package::getBackupKey(const QString &aid) {
    if (accountId != aid) {
        accountId = aid;
        get(QString("http://cma.henkaku.xyz/?aid=%1").arg(aid), backupKey);
        qDebug("Fetching backup key from cma.henkaku.xyz...");
        emit setStatusText(tr("Fetching backup key from cma.henkaku.xyz"));
    }
}

void Package::downloadDemo() {
    download(BSPKG_URL, BSPKG_FILE, BSPKG_SHA256);
}

void Package::downloadHencore() {
    download(HENCORE_URL, HENCORE_FILE, HENCORE_SHA256);
}

void Package::createPsvImgs() {
    Worker::start(this, [this](void*) {
        QString oldDir = QDir::currentPath();
        QDir curr(pkgBasePath);
        curr.cd("h-encore");
        QDir::setCurrent(curr.path());
        if (trimApp) {
            qDebug("Trimming package...");
            emit setStatusText(tr("Trimming package"));
            curr.cd("app");
            curr.cd("ux0_temp_game_PCSG90096_app_PCSG90096");
            curr.cd("resource");
            curr.cd("movie");
            curr.removeRecursively();
            curr.cdUp();
            curr.cd("sound");
            curr.removeRecursively();
            curr.cdUp();
            curr.cd("text");
            curr.cd("01");
            curr.removeRecursively();
            curr.cdUp();
            curr.cdUp();
            curr.cd("image");
            curr.cd("bg");
            curr.removeRecursively();
            curr.cdUp();
            curr.cd("ev");
            curr.removeRecursively();
            curr.cdUp();
            curr.cd("icon");
            curr.removeRecursively();
            curr.cdUp();
            curr.cd("stitle");
            curr.removeRecursively();
            curr.cdUp();
            curr.cd("tachie");
            curr.removeRecursively();
            qDebug("Done trimming.");
        }
        qDebug("Creating psvimg's...");
        emit setStatusText(tr("Createing psvimg's"));
        emit setPercent(0);
        psvimg_create("app", "PCSG90096/app", backupKey.toUtf8().constData(), "app", 0);
        emit setPercent(90);
        psvimg_create("appmeta", "PCSG90096/appmeta", backupKey.toUtf8().constData(), "appmeta", 0);
        emit setPercent(93);
        psvimg_create("license", "PCSG90096/license", backupKey.toUtf8().constData(), "license", 0);
        emit setPercent(96);
        psvimg_create("savedata", "PCSG90096/savedata", backupKey.toUtf8().constData(), "savedata", 0);
        emit setPercent(99);
        QDir::setCurrent(oldDir);
    }, [this](void*) {
        qDebug("Done.");
        emit createdPsvImgs();
    });
}

void Package::downloadProg(uint64_t curr, uint64_t total) {
    emit setPercent((int)(curr * 100ULL / total));
}

void Package::fetchFinished(void *arg) {
    QString *str = (QString*)arg;
    int index = str->indexOf("</b>: ");
    if (index < 0) {
        str->clear();
        qWarning("Cannot get backup key from your AID");
        emit setStatusText(tr("Cannot get backup key from your AID.\nPlease check your network connection!"));
        return;
    }
    emit setStatusText(tr("Fetched backup key.\nClick button to START!"));
    qDebug("Done.");
    *str = str->mid(index + 6, 64);
    emit gotBackupKey();
}

void Package::downloadFinished(QFile *f) {
    QString filepath = f->fileName();
    QFileInfo fi(filepath);
    delete f;
    static int n = 0;
    if (fi.fileName() == HENCORE_FILE) {
        Worker::start(this, [this, filepath](void *arg) {
            if (verify(filepath, HENCORE_SHA256)) 
                *(int*)arg = 1;
            else
                *(int*)arg = 0;
        }, [this](void *arg) {
            if (*(int*)arg)
                startUnpackHencore(HENCORE_FILE);
        });
    } else if (fi.fileName() == BSPKG_FILE) {
        Worker::start(this, [this, filepath](void *arg) {
            if (verify(filepath, BSPKG_SHA256))
                *(int*)arg = 1;
            else
                *(int*)arg = 0;
        }, [this](void *arg) {
            if (*(int*)arg)
                startUnpackDemo(BSPKG_FILE);
        });
    }
}
