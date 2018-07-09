#pragma once

#include "ui_finalhe.h"
#include "vita.hh"

#include <QtWidgets/QMainWindow>
#include <QTranslator>
#include <QTimer>

class FinalHE: public QMainWindow {
    Q_OBJECT

public:
    FinalHE(QWidget *parent = nullptr);
    virtual ~FinalHE();

private slots:
    void langChange();
	void onConnect();
	void eventTimerUpdate();

private:
    void loadLanguage(const QString &s);

private:
    Ui::FinalHEClass ui;
    QTranslator trans;
    QTimer eventTimer;
    VitaConn vita;
};
