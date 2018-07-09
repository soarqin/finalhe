#include "finalhe.hh"

#include "log.hh"

#include <QLocale>
#include <QDir>

FinalHE::FinalHE(QWidget *parent): QMainWindow(parent), eventTimer(this) {
    ui.setupUi(this);
    logSetFunc(std::bind(&QPlainTextEdit::appendPlainText, ui.logBrowser, std::placeholders::_1));
    QDir dir(qApp->applicationDirPath());
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
	connect(ui.btnConnect, SIGNAL(clicked()), this, SLOT(onConnect()));
	connect(ui.comboLang, SIGNAL(currentIndexChanged(int)), this, SLOT(langChange()));
}

void FinalHE::langChange() {
    QVariant var = ui.comboLang->currentData();
    QString compPath = var.toString();
    loadLanguage(compPath);
}

FinalHE::~FinalHE() {
}

void FinalHE::onConnect() {
}

void FinalHE::eventTimerUpdate() {
    vita.process();
}

void FinalHE::loadLanguage(const QString &s) {
    qApp->removeTranslator(&trans);
    if (trans.load(s)) {
        qApp->installTranslator(&trans);
    }
    ui.retranslateUi(this);
    ui.comboLang->setItemText(0, trans.isEmpty() ? "English" : trans.translate("base", "English"));
}
