/*
 *  Final h-encore, Copyright (C) 2018  Soar Qin
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    bool checkFwUpdate();
    void loadLanguage(const QString &s);

private:
    Ui::FinalHEClass ui;
    QTranslator trans;
    Package *pkg;
    VitaConn *vita;

    bool expanding = false;
};
