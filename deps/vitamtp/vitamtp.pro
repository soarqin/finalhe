TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = vitamtp

win32-g++ {
    QMAKE_CFLAGS += -mno-ms-bitfields
}
win32-msvc* {
    DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS
    DEFINES += USING_STATIC_LIBICONV LIBXML_STATIC
    INCLUDEPATH = ../iconv/include ../libxml2/include ../libusb/libusb
} else {
    QT_CONFIG -= no-pkg-config
    CONFIG += link_pkgconfig
    PKGCONFIG += libusb-1.0 libxml-2.0
    !linux {
        LIBS += -liconv
    }
    QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-parentheses
}
unix:!macx {
    DEFINES += _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE
}
DEFINES += HAVE_ICONV HAVE_LIMITS_H PTP_USB_SUPPORT PTP_IP_SUPPORT
SOURCES = datautils.c device.c ptp.c usb.c vitamtp.c wireless.c
HEADERS = vitamtp.h
win32 {
    SOURCES += asprintf.c socketpair.c
}
