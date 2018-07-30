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

#include "worker.hh"

Worker::Worker(void *carryArg, QObject *parent) : QObject(parent), arg(carryArg) {
}

QThread *Worker::start(QObject *host,
                       const std::function<void(void*)> &thFunc,
                       const std::function<void(void*)> &finFunc,
                       void *carryArg) {
    QThread *thread = new QThread;
    Worker *worker = new Worker(carryArg);
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
    worker->moveToThread(thread);
    thread->start();
    return thread;
}
