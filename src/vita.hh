#pragma once

#include <QString>
#include <stdint.h>

typedef struct vita_device vita_device_t;
typedef struct LIBVitaMTP_event vita_event_t;

class VitaConn {
public:
    VitaConn(const QString& baseDir);
    virtual ~VitaConn();

    void process();

private:
    void doConnect();
    void processEvent(vita_event_t *evt);
    void deviceDisconnect();

private:
    QString appBaseDir;
    vita_device_t *currDev = nullptr;
    uint64_t nextTick = 0ULL;
    QString onlineId;

};
