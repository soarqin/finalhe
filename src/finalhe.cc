#include "finalhe.hh"

#include "package.hh"
#include "vita.hh"

#include <QLocale>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDebug>
#include <QSettings>

FinalHE::FinalHE(QWidget *parent): QMainWindow(parent) {
    ui.setupUi(this);
    setWindowTitle("Final h-encore " FINALHE_VERSION_STR);
    setWindowIcon(QIcon(":/main/resources/images/finalhe.png"));
    setFixedSize(600, 400);

    ui.textMTP->setStyleSheet("QLabel { color : blue; }");

    ui.progressBar->setMaximum(100);
    QDir baseDir, dir(qApp->applicationDirPath());
#ifdef _WIN32
    baseDir = dir;
    if (!baseDir.mkpath("data") || !(baseDir.cd("data"), baseDir.exists()))
#endif
    {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        baseDir.mkpath("data");
	baseDir.cd("data");
    }
    vita = new VitaConn(baseDir.path(), this);
    pkg = new Package(baseDir.path(), this);
    int useSysLang = 0;
    QSettings settings;
    QString langLoad = settings.value("language").toString();
    ui.comboLang->addItem(trans.isEmpty() ? "English" : trans.translate("base", "English"));
    if (dir.cd("language")) {
        QStringList ll = dir.entryList({"*.qm"}, QDir::Filter::Files, QDir::SortFlag::IgnoreCase);
        for (auto &p: ll) {
            QString compPath = dir.filePath(p);
            QTranslator transl;
            if (transl.load(compPath)) {
                int index = ui.comboLang->count();
                ui.comboLang->addItem(transl.translate("base", "LANGUAGE"), compPath);
                if (useSysLang != 0) continue;
                if (langLoad.isNull()) {
                    if (QFileInfo(p).baseName() == QLocale::system().name()) {
                        useSysLang = index;
                    }
                } else if (compPath == langLoad) {
                    useSysLang = index;
                }
            }
        }
    }

    pkg->setTrim(ui.checkTrim->checkState() == Qt::Checked);

    connect(ui.btnStart, SIGNAL(clicked()), this, SLOT(onStart()));
	connect(ui.comboLang, SIGNAL(currentIndexChanged(int)), this, SLOT(langChange()));
    connect(ui.checkTrim, SIGNAL(stateChanged(int)), this, SLOT(trimState(int)));
    connect(vita, &VitaConn::gotAccountId, this, [this](QString accountId) { pkg->getBackupKey(accountId); });
    connect(vita, SIGNAL(receivedPin(QString, int)), this, SLOT(displayPin(QString, int)));
    connect(vita, SIGNAL(completedPin()), this, SLOT(clearPin()));
    connect(pkg, SIGNAL(gotBackupKey()), this, SLOT(enableStart()));
    connect(pkg, SIGNAL(createdPsvImgs()), vita, SLOT(buildData()));
    connect(vita, SIGNAL(builtData()), pkg, SLOT(finishBuildData()));

    connect(vita, SIGNAL(setStatusText(QString)), this, SLOT(setTextMTP(QString)));
    connect(pkg, SIGNAL(setStatusText(QString)), this, SLOT(setTextPkg(QString)));
    connect(pkg, SIGNAL(setPercent(int)), ui.progressBar, SLOT(setValue(int)));

    vita->process();
    pkg->tips();

    if (useSysLang) {
        ui.comboLang->setCurrentIndex(useSysLang);
        langChange();
    }
}

FinalHE::~FinalHE() {
    delete pkg;
    delete vita;
}

void FinalHE::langChange() {
    QVariant var = ui.comboLang->currentData();
    QString compPath = var.toString();
    loadLanguage(compPath);
    pkg->tips();
    vita->updateStatus();
    QSettings().setValue("language", compPath);
}

void FinalHE::onStart() {
    ui.btnStart->setEnabled(false);
    emit pkg->startDownload();
}

void FinalHE::enableStart() {
    ui.btnStart->setEnabled(true);
}

void FinalHE::trimState(int state) {
    pkg->setTrim(state == Qt::Checked);
}

void FinalHE::setTextMTP(QString txt) {
    ui.textMTP->setText(txt);
}

void FinalHE::setTextPkg(QString txt) {
    ui.textPkg->setText(txt);
}

void FinalHE::displayPin(QString onlineId, int pin) {
    ui.textPkg->setText(tr("Registering device: %1\nInput this PIN on PS Vita: %2").arg(onlineId).arg(pin, 8, 10, QChar('0')));
}

void FinalHE::clearPin() {
    ui.textPkg->setText(tr("Registered device."));
}

void FinalHE::loadLanguage(const QString &s) {
    qApp->removeTranslator(&trans);
    if (trans.load(s)) {
        qApp->installTranslator(&trans);
    }
    ui.retranslateUi(this);
    ui.comboLang->setItemText(0, trans.isEmpty() ? "English" : trans.translate("base", "English"));
}
