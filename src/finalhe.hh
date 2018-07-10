#pragma once

#include "ui_finalhe.h"
#include "vita.hh"

#include <QtWidgets/QMainWindow>
#include <QTranslator>
#include <QTimer>
#include <QDir>

class FinalHE: public QMainWindow {
    Q_OBJECT

public:
    FinalHE(QWidget *parent = nullptr);
    virtual ~FinalHE();

private slots:
    void langChange();
	void onStart();
	void eventTimerUpdate();

private:
    void loadLanguage(const QString &s);
    void downloadPackage();
    void startDownloadPackage();
    void startUnpackPackage();

private:
    Ui::FinalHEClass ui;
    QTranslator trans;
    QTimer eventTimer;
    VitaConn *vita;
    QDir baseDir;
};
