#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <QSemaphore>
#include <cstdint>
#include <ctime>
#include <map>

extern "C" {
typedef struct vita_device vita_device_t;
typedef struct LIBVitaMTP_event vita_event_t;
typedef struct metadata metadata_t;
}

class VitaConn : public QObject {
    Q_OBJECT

public:
    struct MetaInfo {
        int ohfi;
        int ohfiParent;
        int ohfiRoot;
        QString name;
        QString path;
        int type;
        time_t dateTimeCreated;
        time_t dateTimeModified;
        uint64_t size;
        int dataType;

        QString fullPath;
        std::map<int, MetaInfo*> subMeta;

        void updateSize();
    };

public:
    VitaConn(const QString &baseDir, const QString &appDir, QObject *obj_parent = 0);
    virtual ~VitaConn();
    inline bool has365Update() { return !Update365.isEmpty(); }
    inline bool has368Update() { return !Update368.isEmpty(); }
    inline void setUse365Update() { if (!Update365.isEmpty()) useUpdate = 1; }
    inline void setUse368Update() { if (!Update368.isEmpty()) useUpdate = 2; }
    inline void setUseNoUpdate() { if (!Update368.isEmpty()) useUpdate = 0; }

public slots:
    void process();
    void buildData();
    void updateStatus();

signals:
    void gotAccountId(QString);
    void setStatusText(QString);
    void builtData();
    void receivedPin(QString, int);
    void completedPin();

private:
    int recursiveScanRootDirectory(const QString &base_path, const QString &rel_path, int parent_ohfi, int root_ohfi);
    void doConnect();
    void processEvent(vita_event_t *evt);
    void deviceDisconnect();
    MetaInfo *metaAddFile(const QString &basePath, const QString &relName, int ohfi, int ohfiParent, int ohfiRoot, bool isDir);
    void buildMetaData(metadata_t **meta, int ohfiParent, uint32_t index, uint32_t num);

private:
    QString pkgBaseDir, appBaseDir;
    vita_device_t *currDev = nullptr, *wirelessDev = nullptr;
    QString onlineId;
    QString accountId;
    QString deviceVersion;
    int ohfiMax = 256;

    std::map<int, MetaInfo> metaMap;

    QMutex metaMutex, connMutex;
    QThread *clientThread = nullptr;
    QThread *wirelessThread = nullptr;
    QSemaphore semaClient, semaWireless;

    QString Update365, Update368;
    int useUpdate = 0;

    bool running = false;
};
