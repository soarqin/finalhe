#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
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
        std::map<int, MetaInfo*> subMeta;

        void updateSize();
    };
public:
    VitaConn(const QString& baseDir, QObject *obj_parent = 0);
    virtual ~VitaConn();

public slots:
    void process();
    void buildData();

signals:
    void gotAccountId(QString);
    void setStatusText(QString);
    void builtData();

private:
    int recursiveScanRootDirectory(const QString &base_path, const QString &rel_path, int parent_ohfi, int root_ohfi);
    void doConnect();
    void processEvent(vita_event_t *evt);
    void deviceDisconnect();
    MetaInfo *metaAddFile(const QString &basePath, const QString &relName, int ohfi, int ohfiParent, int ohfiRoot, bool isDir);
    void buildMetaData(metadata_t **meta, int ohfiParent, uint32_t index, uint32_t num);

private:
    QString appBaseDir;
    vita_device_t *currDev = nullptr;
    QString onlineId;
    QString accountId;
    int ohfiMax = 256;

    std::map<int, MetaInfo> metaMap;

    QMutex metaMutex;
    QThread *clientThread = nullptr;
    bool running = false;
};
