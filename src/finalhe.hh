#pragma once

#include "ui_finalhe.h"
#include "package.hh"
#include "vita.hh"

#include <QtWidgets/QMainWindow>
#include <QString>
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
    void enableStart();

signals:
    void appendLog(const QString &text);

private:
    void loadLanguage(const QString &s);

private:
    Ui::FinalHEClass ui;
    QTranslator trans;
    QTimer eventTimer;
    Package *pkg;
    VitaConn *vita;
};
