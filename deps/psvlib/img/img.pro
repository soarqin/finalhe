TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = psvimg

win32-msvc* {
    DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
}
unix:!macx {
    DEFINES += _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE
}
INCLUDEPATH = ../../zlib ../../scrypto
SOURCES += backup.c utils.c psvimg-create.c
HEADERS += backup.h crypto.h endian-utils.h utils.h psvimg-create.h psvimg.h
