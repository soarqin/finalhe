TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = miniz

SOURCES += miniz.c miniz_zip.c miniz_tinfl.c miniz_tdef.c
HEADERS += miniz.h miniz_zip.h miniz_tinfl.h miniz_tdef.h
