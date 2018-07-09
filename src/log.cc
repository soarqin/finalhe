#include "log.hh"

std::function<void(const QString&)> loggerFunc;

void logSetFunc(std::function<void(const QString&)> func) {
    loggerFunc = func;
}
