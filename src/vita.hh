#pragma once

#include <QObject>
#include <QString>
#include <stdint.h>

typedef struct vita_device vita_device_t;
typedef struct LIBVitaMTP_event vita_event_t;

class VitaConn : public QObject {
    Q_OBJECT
public:
    VitaConn(const QString& baseDir);
    virtual ~VitaConn();

    void process();
    void buildData();

signals:
    void gotAccountId(QString);

private:
    int recursiveScanRootDirectory(const QString &base_path, const QString &rel_path, int parent_ohfi, int root_ohfi);
    void doConnect();
    void processEvent(vita_event_t *evt);
    void deviceDisconnect();

private:
    QString appBaseDir;
    vita_device_t *currDev = nullptr;
    uint64_t nextTick = 0ULL;
    QString onlineId;
    QString accountId;
    int ohfiMax = 256;
};
