TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = iconv

INCLUDEPATH = include
DEFINES += USING_STATIC_LIBICONV
win32 {
    DEFINES += _CRT_SECURE_NO_WARNINGS
    win32-msvc* {
        QMAKE_CXXFLAGS += /wd4018
    }
}
SOURCES += libiconv/iconv.c libiconv/localcharset.c libiconv/relocatable.c
HEADERS += include/iconv.h
