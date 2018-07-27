TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = psvimg

win32-msvc* {
    DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
    INCLUDEPATH += ../../zlib
} else {
    QT_CONFIG -= no-pkg-config
    CONFIG += link_pkgconfig
    PKGCONFIG += zlib
}
unix:!macx {
    DEFINES += _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE
}
INCLUDEPATH += ../../scrypto
SOURCES += backup.c utils.c psvimg-create.c
HEADERS += backup.h crypto.h endian-utils.h utils.h psvimg-create.h psvimg.h
