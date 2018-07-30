#pragma once

#include "ui_finalhe.h"

#include <QtWidgets/QMainWindow>
#include <QString>
#include <QTranslator>
#include <QTimer>
#include <QDir>

class Package;
class VitaConn;

class FinalHE: public QMainWindow {
    Q_OBJECT

public:
    FinalHE(QWidget *parent = nullptr);
    virtual ~FinalHE();

private slots:
    void langChange();
	void onStart();
    void enableStart();
    void trimState(int);
    void setTextMTP(QString);
    void setTextPkg(QString);
    void displayPin(QString, int);
    void clearPin();
    void toggleExpanding();
    void extraItemsChanged(QListWidgetItem*);

private:
    void loadLanguage(const QString &s);

private:
    Ui::FinalHEClass ui;
    QTranslator trans;
    Package *pkg;
    VitaConn *vita;

    bool expanding = false;
};
