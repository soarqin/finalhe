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

#include "finalhe.hh"

#include "package.hh"
#include "vita.hh"
#include "version.hh"

#include <QLocale>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDebug>
#include <QSettings>

FinalHE::FinalHE(QWidget *parent): QMainWindow(parent) {
    ui.setupUi(this);
    setWindowTitle("Final h-encore v" FINALHE_VERSION_STR);
    setWindowIcon(QIcon(":/main/resources/images/finalhe.png"));
    setFixedSize(600, 400);
    ui.centralWidget->setFixedSize(600, 400);
    ui.extraItems->setFixedSize(200, 400);
    ui.extraItems->hide();
    ui.expandButton->setArrowType(Qt::RightArrow);
    ui.expandButton->setFixedWidth(10);

    ui.textMTP->setStyleSheet("QLabel { color : blue; }");

    ui.progressBar->setMaximum(100);
    QDir baseDir, dir(qApp->applicationDirPath());
#ifdef __APPLE__
    dir.cdUp(); dir.cdUp(); dir.cdUp();
#endif
#ifdef _WIN32
    baseDir = dir;
    if (!baseDir.mkpath("data") || !(baseDir.cd("data"), baseDir.exists()))
#endif
    {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        baseDir.mkpath("data");
        baseDir.cd("data");
    }
    vita = new VitaConn(baseDir.path(), dir.path(), this);
    pkg = new Package(baseDir.path(), dir.path(), this);
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

    bool hasFirmware = false;
    for (int i = 0; i < 2; ++i) {
        bool has = i == 0 ? vita->has365Update() : vita->has368Update();
        if (has) {
            auto *item = new QListWidgetItem(tr("Firmware %1").arg(i == 0 ? "3.65" : "3.68"));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            if (!hasFirmware) {
                auto *item0 = new QListWidgetItem(tr("-- Firmware update --"));
                item0->setFlags(Qt::NoItemFlags);
                ui.extraItems->addItem(item0);
                hasFirmware = true;
                item->setCheckState(Qt::Checked);
                i == 0 ? vita->setUse365Update() : vita->setUse368Update();
            } else
                item->setCheckState(Qt::Unchecked);
            item->setData(Qt::UserRole, i + 1);
            ui.extraItems->addItem(item);
        }
    }
    const auto &extraApps = pkg->getExtraApps();
    bool hasExtra = false;
    for (auto &p: extraApps) {
        if (!hasExtra) {
            auto *item0 = new QListWidgetItem(tr("-- Additional applications --"));
            item0->setFlags(Qt::NoItemFlags);
            ui.extraItems->addItem(item0);
            hasExtra = true;
        }
        auto *item = new QListWidgetItem(p.name);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        ui.extraItems->addItem(item);
        item->setData(Qt::UserRole, p.titleId);
        bool defsel = p.titleId == "VITASHELL";
        item->setCheckState(defsel ? Qt::Checked : Qt::Unchecked);
        if (defsel) pkg->selectExtraApp(p.titleId, true);
    }
    if (hasFirmware || hasExtra)
        ui.expandButton->show();
    else
        ui.expandButton->hide();

    connect(ui.btnStart, SIGNAL(clicked()), this, SLOT(onStart()));
	connect(ui.comboLang, SIGNAL(currentIndexChanged(int)), this, SLOT(langChange()));
    connect(ui.checkTrim, SIGNAL(stateChanged(int)), this, SLOT(trimState(int)));
    connect(ui.expandButton, SIGNAL(clicked()), this, SLOT(toggleExpanding()));
    connect(ui.extraItems, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(extraItemsChanged(QListWidgetItem*)));
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

void FinalHE::fwUpdateMsg() {
    if (vita->getDeviceVersion() < "3.65") {
        ui.textPkg->setText(tr("Fimrware version is not supported by h-encore.") + "\n"
            + tr("Update to %1 before installing h-encore!").arg("3.65/3.68") + "\n"
            + ((vita->has365Update() || vita->has368Update())
            ? tr("On PS Vita:\nSettings -> System Update -> Update by Connecting to a PC")
            : QString()));
    } else if (vita->getDeviceVersion() > "3.65" && vita->getDeviceVersion() < "3.68") {
        if (vita->has368Update()) {
            int count = ui.extraItems->count();
            for (int i = 0; i < count; ++i) {
                auto *eitem = ui.extraItems->item(i);
                if (eitem->flags() == Qt::NoItemFlags) continue;
                bool ok;
                int n = eitem->data(Qt::UserRole).toInt(&ok);
                if (ok) {
                    if (n == 1) {
                        ui.extraItems->removeItemWidget(eitem);
                        delete eitem;
                        count = ui.extraItems->count();
                        --i;
                    } else
                        eitem->setCheckState(n == 2 ? Qt::Checked : Qt::Unchecked);
                }
            }
        }
        ui.textPkg->setText(tr("Fimrware version is not supported by h-encore.") + "\n"
            + tr("Update to %2 before installing h-encore!").arg("3.68") + "\n"
            + (vita->has368Update()
            ? tr("On PS Vita:\nSettings -> System Update -> Update by Connecting to a PC")
            : QString()));
    }
}

void FinalHE::enableStart() {
    fwUpdateMsg();
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

void FinalHE::toggleExpanding() {
    if (expanding) {
        setFixedSize(600, 400);
        ui.extraItems->hide();
        ui.expandButton->setArrowType(Qt::RightArrow);
        expanding = false;
    } else {
        setFixedSize(810, 400);
        ui.extraItems->show();
        ui.expandButton->setArrowType(Qt::LeftArrow);
        expanding = true;
    }
}

void FinalHE::extraItemsChanged(QListWidgetItem *item) {
    QVariant var = item->data(Qt::UserRole);
    bool ok;
    int fwidx = var.toInt(&ok);
    bool checked = item->checkState() == Qt::Checked;
    if (!ok) {
        pkg->selectExtraApp(var.toString(), checked);
    } else {
        if (checked) {
            switch (fwidx) {
            case 1:
                vita->setUse365Update();
                break;
            case 2:
                vita->setUse368Update();
                break;
            default:
                vita->setUseNoUpdate();
                break;
            }
            int count = ui.extraItems->count();
            for (int i = 0; i < count; ++i) {
                auto *eitem = ui.extraItems->item(i);
                if (eitem->flags() == Qt::NoItemFlags || eitem == item) continue;
                bool ok;
                int n = eitem->data(Qt::UserRole).toInt(&ok);
                if (ok && n > 0) {
                    eitem->setCheckState(Qt::Unchecked);
                }
            }
        } else
            vita->setUseNoUpdate();
    }
}

void FinalHE::loadLanguage(const QString &s) {
    qApp->removeTranslator(&trans);
    if (trans.load(s)) {
        qApp->installTranslator(&trans);
    }
    ui.retranslateUi(this);
    ui.comboLang->setItemText(0, trans.isEmpty() ? "English" : trans.translate("base", "English"));
}
