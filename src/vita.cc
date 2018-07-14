#include "vita.hh"

#include "worker.hh"

#include <vitamtp.h>
#include <QDateTime>
#include <QTemporaryDir>
#include <QUrl>
#include <QFileInfo>
#include <QtNetwork/QHostInfo>
#include <QDebug>

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

#define PTP_EC_VITA_RequestSendNumOfObject 0xC104
#define PTP_EC_VITA_RequestSendObjectMetadata 0xC105
#define PTP_EC_VITA_RequestSendObject 0xC107
#define PTP_EC_VITA_RequestCancelTask 0xC108
#define PTP_EC_VITA_RequestSendHttpObjectFromURL 0xC10B
#define PTP_EC_VITA_Unknown1 0xC10D
#define PTP_EC_VITA_RequestSendObjectStatus 0xC10F
#define PTP_EC_VITA_RequestSendObjectThumb 0xC110
#define PTP_EC_VITA_RequestDeleteObject 0xC111
#define PTP_EC_VITA_RequestGetSettingInfo 0xC112
#define PTP_EC_VITA_RequestSendHttpObjectPropFromURL 0xC113
#define PTP_EC_VITA_RequestSendPartOfObject 0xC115
#define PTP_EC_VITA_RequestOperateObject 0xC117
#define PTP_EC_VITA_RequestGetPartOfObject 0xC118
#define PTP_EC_VITA_RequestSendStorageSize 0xC119
#define PTP_EC_VITA_RequestCheckExistance 0xC120
#define PTP_EC_VITA_RequestGetTreatObject 0xC122
#define PTP_EC_VITA_RequestSendCopyConfirmationInfo 0xC123
#define PTP_EC_VITA_RequestSendObjectMetadataItems 0xC124
#define PTP_EC_VITA_RequestSendNPAccountInfo 0xC125
#define PTP_EC_VITA_RequestTerminate 0xC126

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

VitaConn::VitaConn(const QString& baseDir, QObject *obj_parent): QObject(obj_parent) {
    appBaseDir = baseDir;
    VitaMTP_Init();
    VitaMTP_USB_Init();
}

VitaConn::~VitaConn() {
    if (clientThread != nullptr) {
        running = false;
        clientThread->wait();
        clientThread = nullptr;
    }
}

void VitaConn::process() {
    running = true;
    Worker::start(this, [this](void*) {
        while (running) {
            if (currDev == nullptr) {
                doConnect();
                if (currDev == nullptr) {
                    QThread::sleep(2);
                }
                continue;
            }
            vita_event_t evt;
            int res = VitaMTP_Read_Event(currDev, &evt);
            if (res < 0) {
                qDebug("Disconnected from %s", VitaMTP_Get_Identification(currDev));
                deviceDisconnect();
                continue;
            }
            processEvent(&evt);
        }
        deviceDisconnect();
        VitaMTP_USB_Exit();
        VitaMTP_Cleanup();
    }, [this](void*) {
        running = false;
        clientThread = nullptr;
    });
}

void VitaConn::buildData() {
    QMutexLocker locker(&metaMutex);
    QDir dir(appBaseDir);
    dir.cd("h-encore");
    int ohfi = ohfiMax++;
    auto *thisMeta = metaAddFile(dir.path(), "PCSG90096", ohfi, VITA_OHFI_VITAAPP, VITA_OHFI_VITAAPP, true);
    recursiveScanRootDirectory(dir.path(), "PCSG90096", ohfi, VITA_OHFI_VITAAPP);
    thisMeta->updateSize();
}

int VitaConn::recursiveScanRootDirectory(const QString &base_path, const QString &rel_path, int parent_ohfi, int root_ohfi) {
    int total_objects = 0;

    QDir dir(base_path + "/" + rel_path);
    dir.setSorting(QDir::Name);
    QFileInfoList qsl = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

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
    if (currDev != nullptr) return;
    currDev = VitaMTP_Get_First_USB_Vita();
    if (currDev == nullptr) return;
    vita_info_t info;
    if (VitaMTP_GetVitaInfo(currDev, &info) != PTP_RC_OK) {
        qWarning("Cannot get Vita MTP information.");
        deviceDisconnect();
        return;
    }
    onlineId = info.onlineId;
    const initiator_info_t *iinfo = VitaMTP_Data_Initiator_New(QHostInfo::localHostName().toUtf8().data(), VITAMTP_PROTOCOL_FW_3_30);
    if (VitaMTP_SendInitiatorInfo(currDev, (initiator_info_t *)iinfo) != PTP_RC_OK) {
        VitaMTP_Data_Free_Initiator(iinfo);
        qWarning("Cannot send host information.");
        deviceDisconnect();
        return;
    }
    VitaMTP_Data_Free_Initiator(iinfo);
    if (info.protocolVersion >= VITAMTP_PROTOCOL_FW_2_10) {
        // Get the device's capabilities
        capability_info_t *vita_capabilities;

        if (VitaMTP_GetVitaCapabilityInfo(currDev, &vita_capabilities) != PTP_RC_OK) {
            qWarning("Failed to get capability information from Vita.");
            deviceDisconnect();
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
            deviceDisconnect();
            return;
        }

        free_pc_capability_info(pc_capabilities);
    }

    // Finally, we tell the Vita we are connected
    if (VitaMTP_SendHostStatus(currDev, VITA_HOST_STATUS_Connected) != PTP_RC_OK) {
        qWarning("Cannot send host status.");
        deviceDisconnect();
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

        bool ignorefile = false;
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
        } else {
            /*
            qDebug("Reading from local file");
            data = file.readAll();

            if (basename == "psp2-updatelist.xml" && !ignorefile) {
                messageSent(tr("The PSVita has requested an update check, sending local xml file and ignoring version settings"));
            } else {
                QString versiontype = settings.value("versiontype", "zero").toString();
                QString customVersion = settings.value("customversion", "00.000.000").toString();

                // verify that the update file is really the 3.60 pup
                // to prevent people updating to the wrong version and lose henkaku.
                if (ignorexml && basename == "PSP2UPDAT.PUP" &&
                    (versiontype == "henkaku" ||
                    (versiontype == "custom" &&
                        customVersion == "03.600.000"))) {
                    QCryptographicHash crypto(QCryptographicHash::Sha256);
                    crypto.addData(data);
                    QString result = crypto.result().toHex();

                    if (result != hash360) {
                        qWarning("3.60 PUP SHA256 mismatch");
                        qWarning("> Actual:   %s", qPrintable(result));
                        qWarning("> Expected: %s", qPrintable(hash360));
                        // notify the user
                        messageSent(tr("The XML version is set to 3.60 but the PUP file hash doesn't match, cancel the update if you don't want this"));
                    }
                }
            }
            */
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
        emit gotAccountId(accountId);

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
        QDir dir(appBaseDir);
        dir.cd("h-encore");
        QString fullPath = dir.path() + "/" + ite->second.path;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Cannot read %s", fullPath);
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

        if (!getDiskSpace(appBaseDir, &free, &total)) {
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
    VitaMTP_USB_Reset(currDev);
    VitaMTP_Release_Device(currDev);
    currDev = nullptr;
}

VitaConn::MetaInfo *VitaConn::metaAddFile(const QString &basePath, const QString & relName, int ohfi, int ohfiParent, int ohfiRoot, bool isDir) {
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
    metaMap[ohfiParent].subMeta[ohfi] = &minfo;
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
