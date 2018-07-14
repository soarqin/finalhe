#include "worker.hh"

Worker::Worker(void *carryArg, QObject *parent) : QObject(parent), arg(carryArg) {
}

QThread *Worker::start(QObject *host, const std::function<void(void*)> &thFunc, const std::function<void(void*)> &finFunc, void *carryArg) {
    QThread *thread = new QThread;
    Worker *worker = new Worker(carryArg);
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, [worker, carryArg, thFunc]() {
        thFunc(carryArg);
        emit worker->finished();
    });
    connect(worker, &Worker::finished, host, [carryArg, finFunc]() {
        finFunc(carryArg);
    });
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    thread->start();
    return thread;
}
