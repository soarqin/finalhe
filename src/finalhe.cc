#include "finalhe.hh"

#include "log.hh"

#include <sha256.h>
#include <pkg.h>

#include <QLocale>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QMessageBox>

FinalHE::FinalHE(QWidget *parent): QMainWindow(parent), eventTimer(this) {
    ui.setupUi(this);
    logSetFunc(std::bind(&QPlainTextEdit::appendPlainText, ui.logBrowser, std::placeholders::_1));
    QDir dir(qApp->applicationDirPath());
    baseDir = dir;
    if (!baseDir.cd("data")) {
        if (!baseDir.mkdir("data") || !baseDir.cd("data")) {
            QMessageBox::critical(this, tr("Error"), tr("You don't have write permission to this folder! Exit now."));
            QCoreApplication::quit();
            return;
        }
    }
    vita = new VitaConn(dir.path());
    if (dir.cd("lang")) {
        QStringList ll = dir.entryList({"*.qm"}, QDir::Filter::Files, QDir::SortFlag::IgnoreCase);
        ui.comboLang->addItem(trans.isEmpty() ? "English" : trans.translate("base", "English"));
        for (auto &p: ll) {
            QString compPath = dir.filePath(p);
            QTranslator transl;
            if (transl.load(compPath)) {
                ui.comboLang->addItem(transl.translate("base", "LANGUAGE"), compPath);
            }
        }
    }
    eventTimer.start(20);
    connect(&eventTimer, SIGNAL(timeout()), this, SLOT(eventTimerUpdate()));
	connect(ui.btnStart, SIGNAL(clicked()), this, SLOT(onStart()));
	connect(ui.comboLang, SIGNAL(currentIndexChanged(int)), this, SLOT(langChange()));
}

void FinalHE::langChange() {
    QVariant var = ui.comboLang->currentData();
    QString compPath = var.toString();
    loadLanguage(compPath);
}

FinalHE::~FinalHE() {
    delete vita;
}

void FinalHE::onStart() {
    downloadPackage();
}

void FinalHE::eventTimerUpdate() {
    vita->process();
}

void FinalHE::loadLanguage(const QString &s) {
    qApp->removeTranslator(&trans);
    if (trans.load(s)) {
        qApp->installTranslator(&trans);
    }
    ui.retranslateUi(this);
    ui.comboLang->setItemText(0, trans.isEmpty() ? "English" : trans.translate("base", "English"));
}

const char *BSPKG_SHA256 = "280a734a0b40eedac2b3aad36d506cd4ab1a38cd069407e514387a49d81b9302";

void FinalHE::downloadPackage() {
    QDir dir(qApp->applicationDirPath());
    dir.cd("data");
    QString filename = dir.filePath("BitterSmile.pkg");
    QFile file(filename);
    if (file.open(QFile::ReadOnly)) {
        LOG("Found Bitter Smile package file, verifying file sha256sum...");
        QCoreApplication::processEvents();
        SHA256_CTX ctx;
        sha256_init(&ctx);
        char *buf = new char[4 * 1024 * 1024];
        qint64 rsz;
        while ((rsz = file.read(buf, 4 * 1024 * 1024)) > 0) {
            sha256_update(&ctx, (const uint8_t*)buf, rsz);
            QCoreApplication::processEvents();
        }
        delete[] buf;
        uint8_t sum[32];
        sha256_final(&ctx, sum);
        QByteArray array((const char*)sum, 32);
        if (QString(array.toHex()) != BSPKG_SHA256) {
            LOG("sha256sum mismatch, removing package.");
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
                LOG("Failed to remove old package, operation aborted.");
                return;
            }
        } else {
            LOG("sha256sum correct.");
            startUnpackPackage();
            return;
        }
    }
    startDownloadPackage();
}

void FinalHE::startDownloadPackage() {
    LOG("Start downloading package...");
}

void FinalHE::startUnpackPackage() {
    LOG("Unpacking package...");
    pkg_disable_output();
    QDir::setCurrent(baseDir.path());
    pkg_dec("BitterSmile.pkg", nullptr);
}
