#pragma once

#include <functional>
#include <QString>

extern std::function<void(const QString&)> loggerFunc;

void logSetFunc(std::function<void(const QString &)> func);

#define LOG loggerFunc
