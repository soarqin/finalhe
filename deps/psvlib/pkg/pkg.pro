TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = pkg

win32-msvc* {
    DEFINES += _CRT_SECURE_NO_WARNINGS
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
SOURCES += pkg.c pkg_out.c pkg_sys.c pkg_zrif.c
HEADERS += pkg.h pkg_out.h pkg_sys.h pkg_utils.h pkg_zrif.h
