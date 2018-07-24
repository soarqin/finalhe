TEMPLATE += app
CONFIG -= debug_and_release
QT += gui widgets network
TARGET = FinalHE

CODECFORSRC = UTF-8

win32-msvc* {
    DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
}
DEFINES += USING_STATIC_LIBICONV LIBXML_STATIC
INCLUDEPATH = ../deps/vitamtp ../deps/psvlib/img ../deps/psvlib/pkg ../deps/miniz ../deps/scrypto
SOURCES += main.cc downloader.cc finalhe.cc package.cc vita.cc worker.cc
HEADERS += downloader.hh finalhe.hh package.hh vita.hh worker.hh
LIBS += \
    -L../deps/vitamtp -lvitamtp \
    -L../deps/psvlib/img -lpsvimg \
    -L../deps/psvlib/pkg -lpkg \
    -L../deps/miniz -lminiz \
    -L../deps/scrypto -lscrypto

win32-msvc* {
    LIBS += \
        -L../deps/libusb -llibusb \
        -L../deps/libxml2 -llibxml2 \
        -L../deps/iconv -liconv
    !CONFIG(static) {
        LIBS += -L../deps/zlib -lzlib
    }
} else {
    QT_CONFIG -= no-pkg-config
    CONFIG += link_pkgconfig
    PKGCONFIG += libusb-1.0 libxml-2.0
    !CONFIG(static) {
        PKGCONFIG += zlib
    }
    !linux {
        PKGCONFIG += libiconv
    }
}

win32 {
    LIBS += -lshlwapi -lpsapi
    RC_FILE += finalhe.rc
    OTHER_FILES += finalhe.rc resource.h
}

FORMS += finalhe.ui
OTHER_FILES += finalhe.ico resources/xml/psp2-updatelist.xml
RESOURCES += finalhe.qrc
TRANSLATIONS += translations/zh_CN.ts translations/zh_TW.ts
