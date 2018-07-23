TEMPLATE = lib
CONFIG += staticlib
TARGET = libxml2

DEFINES += LIBXML_STATIC
INCLUDEPATH = include
win32-msvc* {
    DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
    DEFINES += USING_STATIC_LIBICONV
    INCLUDEPATH += ../iconv/include ../zlib
} else {
    QT_CONFIG -= no-pkg-config
    CONFIG += link_pkgconfig
    PKGCONFIG += zlib
    !linux {
        LIBS += -liconv
    }
}
SOURCES += \
    buf.c \
    c14n.c \
    catalog.c \
    chvalid.c \
    debugXML.c \
    dict.c \
    DOCBparser.c \
    encoding.c \
    entities.c \
    error.c \
    globals.c \
    hash.c \
    HTMLparser.c \
    HTMLtree.c \
    legacy.c \
    list.c \
    nanoftp.c \
    nanohttp.c \
    parser.c \
    parserInternals.c \
    pattern.c \
    relaxng.c \
    SAX2.c \
    SAX.c \
    schematron.c \
    threads.c \
    tree.c \
    uri.c \
    valid.c \
    xinclude.c \
    xlink.c \
    xmlIO.c \
    xmlmemory.c \
    xmlreader.c \
    xmlregexp.c \
    xmlmodule.c \
    xmlsave.c \
    xmlschemas.c \
    xmlschemastypes.c \
    xmlunicode.c \
    xmlwriter.c \
    xpath.c \
    xpointer.c \
    xmlstring.c
