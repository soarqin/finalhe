TEMPLATE = subdirs
SUBDIRS = miniz scrypto vitamtp
# SUBDIRS = miniz scrypto vitamtp psvlib

win32-msvc* {
    SUBDIRS += iconv libxml2
    vitamtp.depends = iconv libxml2
#     SUBDIRS += iconv libusb libxml2
#     !CONFIG(static, staticlib) {
#         SUBDIRS += zlib
#     }
}
