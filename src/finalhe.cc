#include "finalhe.hh"

#include "package.hh"
#include "vita.hh"

#include <QLocale>
#include <QDir>
#include <QMessageBox>
#include <QDebug>

FinalHE::FinalHE(QWidget *parent): QMainWindow(parent) {
    ui.setupUi(this);
    setFixedSize(600, 400);

    ui.textMTP->setStyleSheet("QLabel { color : blue; }");

    ui.progressBar->setMaximum(100);
    QDir dir(qApp->applicationDirPath());
    QDir baseDir(dir);
    if (!baseDir.cd("data")) {
        if (!baseDir.mkdir("data") || !baseDir.cd("data")) {
            QMessageBox::critical(this, tr("ERROR"), tr("You don't have write permission to this folder! Exit now."));
            QCoreApplication::quit();
            return;
        }
    }
    vita = new VitaConn(baseDir.path(), this);
    pkg = new Package(baseDir.path(), this);
    if (dir.cd("language")) {
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

    pkg->setTrim(ui.checkTrim->checkState() == Qt::Checked);

    connect(ui.btnStart, SIGNAL(clicked()), this, SLOT(onStart()));
	connect(ui.comboLang, SIGNAL(currentIndexChanged(int)), this, SLOT(langChange()));
    connect(ui.checkTrim, SIGNAL(stateChanged(int)), this, SLOT(trimState(int)));
    connect(vita, &VitaConn::gotAccountId, this, [this](QString &accountId) { pkg->getBackupKey(accountId); });
    connect(pkg, SIGNAL(gotBackupKey()), this, SLOT(enableStart()));
    connect(pkg, SIGNAL(createdPsvImgs()), vita, SLOT(buildData()));
    connect(vita, SIGNAL(builtData()), pkg, SLOT(finishBuildData()));

    connect(vita, SIGNAL(setStatusText(QString)), this, SLOT(setTextMTP(QString)));
    connect(pkg, SIGNAL(setStatusText(QString)), this, SLOT(setTextPkg(QString)));
    connect(pkg, SIGNAL(setPercent(int)), ui.progressBar, SLOT(setValue(int)));

    vita->process();
    pkg->tips();
}

FinalHE::~FinalHE() {
    delete pkg;
    delete vita;
}

void FinalHE::langChange() {
    QVariant var = ui.comboLang->currentData();
    QString compPath = var.toString();
    loadLanguage(compPath);
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

void FinalHE::loadLanguage(const QString &s) {
    qApp->removeTranslator(&trans);
    if (trans.load(s)) {
        qApp->installTranslator(&trans);
    }
    ui.retranslateUi(this);
    ui.comboLang->setItemText(0, trans.isEmpty() ? "English" : trans.translate("base", "English"));
}
