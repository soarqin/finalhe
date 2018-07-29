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
        LIBS += -L../deps/zlib -lzstatic
    }
} else {
    QT_CONFIG -= no-pkg-config
    CONFIG += link_pkgconfig
    PKGCONFIG += libusb-1.0 libxml-2.0
    !CONFIG(static) {
        PKGCONFIG += zlib
    }
    !linux {
        LIBS += -liconv
    }
}

win32 {
    RC_FILE += finalhe.rc
    HEADERS += resource.h
    LIBS += -ladvapi32 -lshlwapi -lws2_32
}

FORMS += finalhe.ui
OTHER_FILES += \
    finalhe.ico \
    resources/xml/psp2-updatelist.xml \
    resources/raw/h-encore.zip
RESOURCES += finalhe.qrc
TRANSLATIONS += \
    # en_US is only used as translation template, no need to be included in release
    translations/en_US.ts \
    translations/cs_CZ.ts \
    translations/es_ES.ts \
    translations/it_IT.ts \
    translations/ja_JP.ts \
    translations/ru_RU.ts \
    translations/tr_TR.ts \
    translations/zh_CN.ts \
    translations/zh_TW.ts

lupdate.commands = lupdate $$_PRO_FILE_
lrelease.commands = lrelease $$_PRO_FILE_
lrelease.depends = lupdate
lcopy.commands = $(COPY_DIR) $$shell_path($$_PRO_FILE_PWD_/translations) language && $(DEL_FILE) $$shell_path(language/*.ts) $$shell_path(language/en_US.qm)
lcopy.depends = lrelease
QMAKE_EXTRA_TARGETS += lupdate lrelease lcopy
