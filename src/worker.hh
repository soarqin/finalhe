#pragma once

#include <QThread>
#include <functional>

class Worker : public QObject {
    Q_OBJECT
public:
    Worker(void *carryArg = nullptr, QObject *parent = nullptr);
    static QThread *start(QObject *host,
                          const std::function<void(void*)> &thFunc,
                          const std::function<void(void*)> &finFunc = [](void*){},
                          void *carryArg = nullptr);

public:
    void *arg;

signals:
    void finished();
};
