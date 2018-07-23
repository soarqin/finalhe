TEMPLATE = lib
CONFIG += staticlib
TARGET = scrypto

SOURCES += aes.c aes_x86.c base64.c sc_crc32.c sc_crc32.h sc_crc32_x86.c sha256.c
HEADERS += aes.h base64.h sha256.h

!win32-msvc* {
    QMAKE_CXXFLAGS += -maes -mssse3 -mpclmul -msse4
}
