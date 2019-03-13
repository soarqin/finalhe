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

#include "vita.hh"

#include "worker.hh"

#include <vitamtp.h>
#include <sha256.h>

#include <QSettings>
#include <QUuid>
#include <QDebug>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDateTime>
#include <QMutex>
#include <QUrl>
#include <QUdpSocket>
#include <QtNetwork/QHostInfo>

#include <cinttypes>

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined Q_OS_ANDROID
#include <sys/vfs.h>
#define statvfs statfs
#else
#include <sys/statvfs.h>
#endif

#define UPDATE_360_SHA256 "8cc2e2666626c4ff8f582bf209473526e825e2a5e38e39b259a8a46e25ef371c"
#define UPDATE_360_SIZE   133676544
#define UPDATE_365_SHA256 "86859b3071681268b6d0beb5ef691da874b6726e85b6f06a1cdf6a0e183e77c6"
#define UPDATE_365_SIZE   133754368
#define UPDATE_368_SHA256 "e39e13bbdb2d9413c3097e4ce23e9ac23f7202cbd35924527b7ad87302a7ba40"
#define UPDATE_368_SIZE   133758464

static inline int getVitaProtocolVersion() {
    return VITAMTP_PROTOCOL_FW_3_30;
}

static bool getDiskSpace(const QString &dir, quint64 *free, quint64 *total) {
#ifdef Q_OS_WIN32

    if (GetDiskFreeSpaceExW(dir.toStdWString().c_str(), (ULARGE_INTEGER *)free, (ULARGE_INTEGER *)total, NULL) != 0) {
        return true;
    }

#else

    struct statvfs stat;

    if (statvfs(dir.toUtf8().data(), &stat) == 0) {
        *total = stat.f_frsize * stat.f_blocks;
        *free = stat.f_frsize * stat.f_bfree;
        return true;
    }

#endif
    return false;
}

static capability_info_t *generate_pc_capability_info() {
    typedef capability_info::capability_info_function tfunction;
    typedef tfunction::capability_info_format tformat;

    // TODO: Actually do this based on QCMA capabilities
    capability_info_t *pc_capabilities = new capability_info_t;
    pc_capabilities->version = "1.0";
    tfunction *functions = new tfunction[3]();
    tformat *game_formats = new tformat[5]();
    game_formats[0].contentType = "vitaApp";
    game_formats[0].next_item = &game_formats[1];
    game_formats[1].contentType = "PSPGame";
    game_formats[1].next_item = &game_formats[2];
    game_formats[2].contentType = "PSPSaveData";
    game_formats[2].next_item = &game_formats[3];
    game_formats[3].contentType = "PSGame";
    game_formats[3].next_item = &game_formats[4];
    game_formats[4].contentType = "PSMApp";
    functions[0].type = "game";
    functions[0].formats = game_formats[0];
    functions[0].next_item = &functions[1];
    functions[1].type = "backup";
    functions[1].next_item = &functions[2];
    functions[2].type = "systemUpdate";
    pc_capabilities->functions = functions[0];
    return pc_capabilities;
}

static void free_pc_capability_info(capability_info_t *info) {
    delete[] & info->functions.formats.next_item[-1];
    delete[] & info->functions.next_item[-1];
    delete info;
}

#define CMA_REQUEST_PORT 9309
static const QString broadcast_reply =
"%1\r\n"
"host-id:%2\r\n"
"host-type:%3\r\n"
"host-name:%4\r\n"
"host-mtp-protocol-version:%5\r\n"
"host-request-port:%6\r\n"
"host-wireless-protocol-version:%7\r\n"
"host-supported-device:PS Vita, PS Vita TV\r\n";
static const char *broadcast_query_start = "SRCH";
static const char *broadcast_query_end = " * HTTP/1.1\r\n";
static const char *broadcast_ok = "HTTP/1.1 200 OK";
static const char *broadcast_unavailable = "HTTP/1.1 503 NG";

class UdpBroadcast : public QObject {
    Q_OBJECT
public:
    explicit UdpBroadcast(QObject *parent = 0): QObject(parent) {
        QSettings settings;
        // generate a GUID if doesn't exist yet in settings
        uuid = settings.value("guid").toString();
        if (uuid.isEmpty()) {
            uuid = QUuid::createUuid().toString().mid(1, 36);
            settings.setValue("guid", uuid);
        }

        hostname = settings.value("hostName", QHostInfo::localHostName()).toString();
        setAvailable();

        socket = new QUdpSocket(this);
        connect(socket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        QHostAddress host_address(QHostAddress::Any);
#else
        QHostAddress host_address(QHostAddress::AnyIPv4);
#endif

        if (!socket->bind(host_address, CMA_REQUEST_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            qCritical() << "Failed to bind address for UDP broadcast";
        }
    }

private:
    void replyBroadcast(const QByteArray &datagram);

    QMutex mutex;
    QString uuid;
    QByteArray reply;
    QString hostname;
    QUdpSocket *socket;

public slots:
    void setAvailable() {
        QMutexLocker locker(&mutex);
        int protocol_version = ::getVitaProtocolVersion();

        reply.clear();
        reply.insert(0, broadcast_reply
            .arg(broadcast_ok, uuid, "win", hostname)
            .arg(protocol_version, 8, 10, QChar('0'))
            .arg(CMA_REQUEST_PORT)
            .arg(VITAMTP_WIRELESS_MAX_VERSION, 8, 10, QChar('0')));
        reply.append('\0');
    }
    void setUnavailable() {
        QMutexLocker locker(&mutex);
        int protocol_version = ::getVitaProtocolVersion();

        reply.clear();
        reply.insert(0, broadcast_reply
            .arg(broadcast_unavailable, uuid, "win", hostname)
            .arg(protocol_version, 8, 10, QChar('0'))
            .arg(CMA_REQUEST_PORT)
            .arg(VITAMTP_WIRELESS_MAX_VERSION, 8, 10, QChar('0')));
        reply.append('\0');
    }

private slots:
    void readPendingDatagrams() {
        if (socket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(socket->pendingDatagramSize());

            QHostAddress cma_sender;
            quint16 senderPort;

            socket->readDatagram(datagram.data(), datagram.size(), &cma_sender, &senderPort);

            if (datagram.startsWith(broadcast_query_start) && datagram.contains(broadcast_query_end)) {
                QMutexLocker locker(&mutex);
                socket->writeDatagram(reply, cma_sender, senderPort);
            } else {
                qWarning("Unknown request: %.*s\n", datagram.length(), datagram.constData());
            }
        }
    }
};


static void copyMetadata(metadata_t *meta, VitaConn::MetaInfo *mi) {
    memset(meta, 0, sizeof(metadata_t));
    meta->ohfiParent = mi->ohfiParent;
    meta->ohfi = mi->ohfi;
    meta->name = strdup(mi->name.toUtf8().data());
    meta->path = strdup(mi->path.toUtf8().data());
    meta->type = mi->type;
    meta->dateTimeCreated = mi->dateTimeCreated;
    meta->size = mi->size;
    meta->dataType = (enum DataType)mi->dataType;
}

static void freeMetadata(metadata_t *meta) {
    free(meta->name);
    free(meta->path);
}

void VitaConn::MetaInfo::updateSize() {
    size = 0;
    for (auto &p : subMeta) {
        size += p.second->size;
    }
}

static VitaConn *this_object = nullptr;
static QString tempOnlineId;

VitaConn::VitaConn(const QString &baseDir, const QString &appDir, QObject *obj_parent): QObject(obj_parent) {
    pkgBaseDir = baseDir;
    appBaseDir = appDir;

    QDir dir(appDir);
    QFileInfoList qsl = dir.entryInfoList({ "*.PUP" }, QDir::Files, QDir::Name);
    foreach(const QFileInfo &info, qsl) {
        QString fullPath = dir.filePath(info.fileName());
        QFile file(fullPath);
        if (file.open(QFile::ReadOnly)) {
            if (file.size() != UPDATE_360_SIZE && file.size() != UPDATE_365_SIZE && file.size() != UPDATE_368_SIZE) {
                file.close();
                continue;
            }
            char header[8];
            file.read(header, 8);
            if (memcmp(header, "SCEUF\x00\x00\x01", 8) != 0) {
                file.close();
                continue;
            }
            file.seek(0);
            qDebug("Verifying sha256sum for %s...", qUtf8Printable(fullPath));
            sha256_context ctx;
            sha256_init(&ctx);
            sha256_starts(&ctx);
            size_t bufsize = 2 * 1024 * 1024;
            char *buf;
            do {
                bufsize >>= 1;
                buf = new char[bufsize];
            } while (buf == nullptr);
            qint64 rsz;
            while ((rsz = file.read(buf, bufsize)) > 0)
                sha256_update(&ctx, (const uint8_t*)buf, rsz);
            delete[] buf;
            file.close();
            uint8_t sum[32];
            sha256_final(&ctx, sum);
            QString res = QByteArray((const char*)sum, 32).toHex();
            if (res == UPDATE_360_SHA256) {
                Update360 = fullPath;
                qDebug("Found 3.60 update: %s", qUtf8Printable(fullPath));
            } else if (res == UPDATE_365_SHA256) {
                Update365 = fullPath;
                qDebug("Found 3.65 update: %s", qUtf8Printable(fullPath));
            } else if (res == UPDATE_368_SHA256) {
                Update368 = fullPath;
                qDebug("Found 3.68 update: %s", qUtf8Printable(fullPath));
            }
        }
    }

    VitaMTP_Init();
    VitaMTP_USB_Init();
    this_object = this;
}

VitaConn::~VitaConn() {
    running = false;
    if (wirelessThread != nullptr) {
        VitaMTP_Cancel_Get_Wireless_Vita();
        semaWireless.acquire();
    }
    semaClient.acquire();
    VitaMTP_USB_Exit();
    VitaMTP_Cleanup();
}

void registrationComplete() {
    qDebug("Registration completed");
    emit this_object->completedPin();
}

int deviceRegistered(const char *deviceid) {
    qDebug("Got connection request from %s", deviceid);
    return 1;
}

int generatePin(wireless_vita_info_t *info, int *p_err) {
    qDebug("Registration request from %s (MAC: %s)", info->name, info->mac_addr);

    tempOnlineId = info->name;

    QString staticPin = QSettings().value("staticPin").toString();

    int pin;

    if (!staticPin.isNull() && staticPin.length() == 8) {
        bool ok;
        pin = staticPin.toInt(&ok);

        if (!ok) {
            pin = rand() % 10000 * 10000 | rand() % 10000;
        }
    } else {
        pin = rand() % 10000 * 10000 | rand() % 10000;
    }

    QTextStream out(stdout);
    out << "Your registration PIN for " << info->name << " is: ";
    out.setFieldWidth(8);
    out.setPadChar('0');
    out << pin << endl;

    qDebug("PIN: %08i", pin);

    *p_err = 0;
    emit this_object->receivedPin(info->name, pin);
    return pin;
}

void VitaConn::process() {
    running = true;
    UdpBroadcast *broadcast = new UdpBroadcast(parent());
    clientThread = Worker::start(this, [this, broadcast](void*) {
        QTime now = QTime::currentTime();
        qsrand(now.msec());
        while (running) {
            connMutex.lock();
            if (currDev == nullptr) {
                updateStatus();
                if (wirelessDev != nullptr) {
                    currDev = wirelessDev;
                    wirelessDev = nullptr;
                } else
                    currDev = VitaMTP_Get_First_USB_Vita();
                if (currDev == nullptr) {
                    connMutex.unlock();
                    if (running) QThread::sleep(2);
                    continue;
                }
                connMutex.unlock();
                broadcast->setUnavailable();
                doConnect();
                broadcast->setAvailable();
                continue;
            } else {
                connMutex.unlock();
            }
            vita_event_t evt;
            int res = VitaMTP_Read_Event(currDev, &evt);
            if (res < 0) {
                qDebug("Disconnected from %s", VitaMTP_Get_Identification(currDev));
                deviceDisconnect();
                lastDeviceVersion.clear();
                lastAccountId.clear();
                continue;
            }
            processEvent(&evt);
        }
        deviceDisconnect();
        semaClient.release();
    });
    wirelessThread = Worker::start(this, [this, broadcast](void*) {
        static wireless_host_info_t host = { NULL, NULL, NULL, CMA_REQUEST_PORT };
        while (running) {
            connMutex.lock();
            if (currDev != nullptr) {
                if (wirelessDev != nullptr && wirelessDev != currDev) {
                    VitaMTP_Release_Device(wirelessDev);
                    wirelessDev = nullptr;
                }
                connMutex.unlock();
                QThread::sleep(2);
                continue;
            }
            connMutex.unlock();
            vita_device_t *newDev = VitaMTP_Get_First_Wireless_Vita(&host, 0, deviceRegistered, generatePin, registrationComplete);
            if (newDev == nullptr) {
                if (running) QThread::sleep(2);
                continue;
            }
            QMutexLocker locker(&connMutex);
            wirelessDev = newDev;
        }
        broadcast->deleteLater();
        semaWireless.release();
    });
}

void VitaConn::buildData() {
    QMutexLocker locker(&metaMutex);
    QDir dir(pkgBaseDir);
    dir.cd("h-encore");
    int ohfi = ohfiMax++;
    auto *thisMeta = metaAddFile(dir.path(), "PCSG90096", ohfi, VITA_OHFI_VITAAPP, VITA_OHFI_VITAAPP, true);
    recursiveScanRootDirectory(dir.path(), "PCSG90096", ohfi, VITA_OHFI_VITAAPP);
    thisMeta->updateSize();
    dir = pkgBaseDir;
    if (dir.cdUp() && dir.cd("extra")) {
        QFileInfoList qsl = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach(const QFileInfo &info, qsl) {
            ohfi = ohfiMax++;
            auto *thisMeta = metaAddFile(dir.path(), info.fileName(), ohfi, VITA_OHFI_VITAAPP, VITA_OHFI_VITAAPP, true);
            recursiveScanRootDirectory(dir.path(), info.fileName(), ohfi, VITA_OHFI_VITAAPP);
            thisMeta->updateSize();
        }
    }
    emit builtData();
}

void VitaConn::updateStatus() {
    if (currDev == nullptr) {
        emit setStatusText(tr("Waiting for connection to PS Vita..."));
    } else {
        if (accountId.isEmpty()) {
            emit setStatusText(tr("Connected to PS Vita") + QString(" @%1: [%2]. ").arg(deviceVersion).arg(onlineId) + tr("Waiting for account ID"));
        } else {
            emit setStatusText(tr("Connected to PS Vita") + QString(" @%1: [%2](%3)").arg(deviceVersion).arg(onlineId).arg(accountId));
        }
    }
}

int VitaConn::recursiveScanRootDirectory(const QString &base_path, const QString &rel_path, int parent_ohfi, int root_ohfi) {
    int total_objects = 0;

    QDir dir(base_path + "/" + rel_path);
    QFileInfoList qsl = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);

    foreach(const QFileInfo &info, qsl) {
        QString rel_name = rel_path.isNull() ? info.fileName() : rel_path + "/" + info.fileName();

        int ohfi = ohfiMax++;

        if (info.isDir()) {
            auto *thisMeta = metaAddFile(base_path, rel_name, ohfi, parent_ohfi, root_ohfi, true);

            int inserted = recursiveScanRootDirectory(base_path, rel_name, ohfi, root_ohfi);
            if (inserted < 0) {
                return -1;
            }

            total_objects += inserted;
            thisMeta->updateSize();
        } else if (info.isFile()) {
            metaAddFile(base_path, rel_name, ohfi, parent_ohfi, root_ohfi, false);
            total_objects++;
        }
    }

    return total_objects;
}

void VitaConn::doConnect() {
    vita_info_t info;
    if (VitaMTP_GetVitaInfo(currDev, &info) != PTP_RC_OK) {
        qWarning("Cannot get Vita MTP information.");
        deviceDisconnect();
        return;
    }
    if (VitaMTP_Get_Device_Type(currDev) == VitaDeviceUSB)
        onlineId = info.onlineId;
    else
        onlineId = tempOnlineId;
    deviceVersion = info.responderVersion;
    updateStatus();
    const initiator_info_t *iinfo = VitaMTP_Data_Initiator_New(QHostInfo::localHostName().toUtf8().data(), ::getVitaProtocolVersion());
    if (VitaMTP_SendInitiatorInfo(currDev, (initiator_info_t *)iinfo) != PTP_RC_OK) {
        VitaMTP_Data_Free_Initiator(iinfo);
        qWarning("Cannot send host information.");
        VitaMTP_USB_Reset(currDev);
        return;
    }
    VitaMTP_Data_Free_Initiator(iinfo);
    if (info.protocolVersion >= VITAMTP_PROTOCOL_FW_2_10) {
        // Get the device's capabilities
        capability_info_t *vita_capabilities;

        if (VitaMTP_GetVitaCapabilityInfo(currDev, &vita_capabilities) != PTP_RC_OK) {
            qWarning("Failed to get capability information from Vita.");
            VitaMTP_USB_Reset(currDev);
            return;
        }

        // TODO: vitamtp needs to send the full metadata info to know the expected format
        // of thumbnails, for example. Until then lets discard the received info.

        VitaMTP_Data_Free_Capability(vita_capabilities);
        // Send the host's capabilities
        capability_info_t *pc_capabilities = generate_pc_capability_info();

        if (VitaMTP_SendPCCapabilityInfo(currDev, pc_capabilities) != PTP_RC_OK) {
            qWarning("Failed to send capability information to Vita.");
            free_pc_capability_info(pc_capabilities);
            VitaMTP_USB_Reset(currDev);
            return;
        }

        free_pc_capability_info(pc_capabilities);
    }

    // Finally, we tell the Vita we are connected
    if (VitaMTP_SendHostStatus(currDev, VITA_HOST_STATUS_Connected) != PTP_RC_OK) {
        qWarning("Cannot send host status.");
        VitaMTP_USB_Reset(currDev);
        return;
    }

    qDebug("Connected to %s", VitaMTP_Get_Identification(currDev));
}

void VitaConn::processEvent(vita_event_t *evt) {
    QMutexLocker locker(&metaMutex);
    uint16_t code = evt->Code;
    uint32_t eventId = evt->Param1;
    qDebug("Event received, code: 0x%04x, id: %u", code, eventId);
    switch (code) {
    case PTP_EC_VITA_RequestTerminate: {
        break;
    }
    case PTP_EC_VITA_RequestSendNumOfObject: {
        int ohfi = evt->Param2;
        auto ite = metaMap.find(ohfi);
        int items = ite == metaMap.end() ? -1 : (int)ite->second.subMeta.size();

        if (VitaMTP_SendNumOfObject(currDev, eventId, items) != PTP_RC_OK) {
            qWarning("Error occurred receiving object count for OHFI parent %d", ohfi);
        } else {
            qDebug("Returned count of %d objects for OHFI parent %d", items, ohfi);
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }
        break;
    }
    case PTP_EC_VITA_RequestSendObjectMetadata: {
        browse_info_t browse;
        if (VitaMTP_GetBrowseInfo(currDev, eventId, &browse) != PTP_RC_OK) {
            qWarning("GetBrowseInfo failed");
            return;
        }

        metadata_t *meta = NULL;
        buildMetaData(&meta, browse.ohfiParent, browse.index, browse.numObjects);
        if (VitaMTP_SendObjectMetadata(currDev, eventId, meta) != PTP_RC_OK) {  // send all objects with OHFI parent
            qWarning("Sending metadata for OHFI parent %d failed", browse.ohfiParent);
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_VITA_Invalid_OHFI);
        } else {
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }
        while (meta) {
            metadata_t *current = meta;
            meta = meta->next_metadata;
            freeMetadata(current);
            delete current;
        }
        break;
    }
    case PTP_EC_VITA_RequestSendObjectStatus: {
        object_status_t objectstatus;

        if (VitaMTP_SendObjectStatus(currDev, eventId, &objectstatus) != PTP_RC_OK) {
            qWarning("Failed to get information for object status.");
            return;
        }
        qDebug("Checking for path %s under ohfi %d", objectstatus.title, objectstatus.ohfiRoot);
        bool found = false;
        for (auto &p : metaMap) {
            if (p.second.path == objectstatus.title) {
                found = true;
                metadata_t metadata;
                copyMetadata(&metadata, &p.second);
                qDebug("Sending metadata for OHFI %d", p.second.ohfi);
                if (VitaMTP_SendObjectMetadata(currDev, eventId, &metadata) != PTP_RC_OK) {
                    qWarning("Error sending metadata for %d", p.second.ohfi);
                } else {
                    VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
                }
                freeMetadata(&metadata);
                break;
            }
        }
        free(objectstatus.title);
        if (!found) {
            qDebug("Object %s not in database (OHFI: %d). Sending OK response for non-existence", objectstatus.title, objectstatus.ohfiRoot);
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }
        break;
    }
    case PTP_EC_VITA_RequestSendObject: {
        break;
    }
    case PTP_EC_VITA_RequestCancelTask: {
        quint32 eventIdToCancel = evt->Param2;
        qDebug("Canceling event %d", eventIdToCancel);
        quint16 ret = VitaMTP_CancelTask(currDev, eventIdToCancel);

        if (ret == PTP_RC_OK) {
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }
        break;
    }
    case PTP_EC_VITA_RequestSendHttpObjectFromURL: {
        char *url;
        if (VitaMTP_GetUrl(currDev, eventId, &url) != PTP_RC_OK) {
            qWarning("Failed to receive URL");
            return;
        }

        QString basename = QFileInfo(QUrl(url).path()).fileName();

        QByteArray data;

        if (basename == "psp2-updatelist.xml") {
            qDebug("Found request for update list. Sending embedded xml file");
            QFile res(":/main/resources/xml/psp2-updatelist.xml");
            res.open(QIODevice::ReadOnly);
            data = res.readAll();

            // fetch country code from url
            QString countryCode;
            QStringList parts = QUrl(url).path().split('/');
            if (parts.size() >= 2) {
                parts.removeLast();
                countryCode = parts.last();
                qDebug("Detected country code from URL: %s", countryCode.toUtf8().constData());

                if (countryCode != "us") {
                    QString regionTag = QString("<region id=\"%1\">").arg(countryCode);
                    data.replace("<region id=\"us\">", qPrintable(regionTag));
                }
            } else {
                qDebug("No country code found in URL, defaulting to \"us\"");
            }
            switch (useUpdate) {
            case 1:
                if (deviceVersion < "3.65") {
                    data.replace("00.000.000", "03.650.000");
                    data.replace("0.00", "3.65");
                }
                break;
            case 2:
                if (deviceVersion < "3.68") {
                    data.replace("00.000.000", "03.680.000");
                    data.replace("0.00", "3.68");
                }
                break;
            }
        } else if (basename == "PSP2UPDAT.PUP") {
            QFile file;
            switch (useUpdate) {
            case 1:
                file.setFileName(Update360);
                break;
            case 2:
                file.setFileName(Update365);
                break;
            case 3:
                file.setFileName(Update368);
                break;
            }
            if (file.open(QIODevice::ReadOnly)) {
                data = file.readAll();
                file.close();
            }
        }
        qDebug("Sending %d bytes of data for HTTP request %s", data.size(), url);

        if (VitaMTP_SendHttpObjectFromURL(currDev, eventId, data.data(), data.size()) != PTP_RC_OK) {
            qWarning("Failed to send HTTP object");
        } else {
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }

        free(url);
        break;
    }
    case PTP_EC_VITA_RequestGetSettingInfo: {
        settings_info_t *settingsinfo;
        if (VitaMTP_GetSettingInfo(currDev, eventId, &settingsinfo) != PTP_RC_OK) {
            qWarning("Failed to get setting info from Vita.");
            return;
        }

        qDebug("Current account id: %s", settingsinfo->current_account.accountId);
        accountId = settingsinfo->current_account.accountId;
        updateStatus();
        if (accountId != lastAccountId || deviceVersion != lastDeviceVersion) {
            lastAccountId = accountId;
            lastDeviceVersion = deviceVersion;
            emit gotAccountId(accountId);
        }

        VitaMTP_Data_Free_Settings(settingsinfo);
        VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        break;
    }
    case PTP_EC_VITA_RequestSendPartOfObject: {
        send_part_init_t part_init;
        if (VitaMTP_SendPartOfObjectInit(currDev, eventId, &part_init) != PTP_RC_OK) {
            qWarning("Cannot get information on object to send");
            return;
        }
        auto ite = metaMap.find(part_init.ohfi);
        if (ite == metaMap.end()) {
            qWarning("Cannot find object for OHFI %d", part_init.ohfi);
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_VITA_Invalid_Context);
            return;
        }
        QString fullPath = ite->second.fullPath;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot read " << fullPath;
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_VITA_Not_Exist_Object);
            return;
        }
        file.seek(part_init.offset);
        QByteArray data = file.read(part_init.size);
        qDebug("Sending %s at file offset %" PRIu64 " for %" PRIu64 " bytes", fullPath.toUtf8().constData(), part_init.offset, part_init.size);

        if (VitaMTP_SendPartOfObject(currDev, eventId, (unsigned char *)data.data(), data.size()) != PTP_RC_OK) {
            qWarning("Failed to send part of object OHFI %d", part_init.ohfi);
        } else {
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }
        break;
    }
    case PTP_EC_VITA_RequestSendStorageSize: {
        int ohfi = evt->Param2;

        quint64 total;
        quint64 free;

        if (!getDiskSpace(pkgBaseDir, &free, &total)) {
            qWarning("Cannot get disk space");
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_VITA_Invalid_Permission);
            return;
        }

        qDebug("Storage stats for drive containing OHFI %d, free: %" PRIu64 ", total: %" PRIu64, ohfi, free, total);

        if (VitaMTP_SendStorageSize(currDev, eventId, total, free) != PTP_RC_OK) {
            qWarning("Send storage size failed");
        } else {
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }
        break;
    }
    case PTP_EC_VITA_RequestSendObjectMetadataItems: {
        uint32_t ohfi;
        if (VitaMTP_SendObjectMetadataItems(currDev, eventId, &ohfi) != PTP_RC_OK) {
            qWarning("Cannot get OHFI for retreving metadata");
            return;
        }
        metadata_t metadata;

        auto ite = metaMap.find(ohfi);
        if (ite == metaMap.end()) {
            qWarning("Cannot find OHFI %d in database", ohfi);
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_VITA_Invalid_OHFI);
            return;
        }
        copyMetadata(&metadata, &ite->second);
        qDebug("Sending metadata for OHFI %d (%s)", ohfi, metadata.path);

        quint16 ret = VitaMTP_SendObjectMetadata(currDev, eventId, &metadata);
        if (ret != PTP_RC_OK) {
            qWarning("Error sending metadata. Code: 0x%04x", ret);
        } else {
            VitaMTP_ReportResult(currDev, eventId, PTP_RC_OK);
        }
        break;
    }
    default:
        qWarning("Unimplemented event code: 0x%04x", code);
        break;
    }
}

void VitaConn::deviceDisconnect() {
    if (currDev == nullptr) return;
    VitaMTP_SendHostStatus(currDev, VITA_HOST_STATUS_EndConnection);
    VitaMTP_Release_Device(currDev);
    connMutex.lock();
    currDev = nullptr;
    connMutex.unlock();
    onlineId.clear();
    accountId.clear();
}

VitaConn::MetaInfo *VitaConn::metaAddFile(const QString &basePath, const QString &relName, int ohfi, int ohfiParent, int ohfiRoot, bool isDir) {
    QFileInfo info(basePath, relName);
    MetaInfo &minfo = metaMap[ohfi];
    minfo.ohfi = ohfi;
    minfo.ohfiParent = ohfiParent;
    minfo.ohfiRoot = ohfiRoot;
    minfo.path = relName;
    minfo.name = info.fileName();
    minfo.type = VITA_DIR_TYPE_MASK_REGULAR;
    minfo.dateTimeModified = info.lastModified().toUTC().toTime_t();
    if (isDir) {
        minfo.size = 0;
        minfo.dataType = Folder;
    } else {
        minfo.dateTimeCreated = info.created().toUTC().toTime_t();
        minfo.size = info.size();
        minfo.dataType = App | File;
    }
    minfo.fullPath = info.absoluteFilePath();
    metaMap[ohfiParent].subMeta[ohfi] = &minfo;
    qDebug() << "Added" << info.fileName() << "as ohfi" << ohfi;
    return &minfo;
}

void VitaConn::buildMetaData(metadata_t **meta, int ohfiParent, uint32_t index, uint32_t num) {
    auto ite = metaMap.find(ohfiParent);
    if (ite == metaMap.end()) return;
    metadata_t *last = nullptr;
    for (auto &p : ite->second.subMeta) {
        if (index > 0) { --index; continue; }
        if (num == 0) break;
        metadata_t *curr = new metadata_t;
        copyMetadata(curr, p.second);
        if (last == nullptr) {
            last = curr;
            *meta = curr;
        } else {
            last->next_metadata = curr;
            last = curr;
        }
        --num;
    }
}

#include "vita.moc"
