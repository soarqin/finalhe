TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = zstatic

DEFINES += HAVE_SYS_TYPES_H HAVE_STDINT_H HAVE_STDDEF_H NO_FSEEKO

win32-msvc* {
    DEFINES += _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE
} else {
    DEFINES += _LARGEFILE64_SOURCE=1
}

HEADERS += \
    zconf.h \
    zlib.h \
    crc32.h \
    deflate.h \
    gzguts.h \
    inffast.h \
    inffixed.h \
    inflate.h \
    inftrees.h \
    trees.h \
    zutil.h

SOURCES += \
    adler32.c \
    compress.c \
    crc32.c \
    deflate.c \
    gzclose.c \
    gzlib.c \
    gzread.c \
    gzwrite.c \
    inflate.c \
    infback.c \
    inftrees.c \
    inffast.c \
    trees.c \
    uncompr.c \
    zutil.c
