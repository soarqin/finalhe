TEMPLATE = subdirs
SUBDIRS = miniz scrypto vitamtp psvimg pkg FinalHE
miniz.subdir = deps/miniz
scrypto.subdir = deps/scrypto
vitamtp.subdir = deps/vitamtp
psvimg.subdir = deps/psvlib/img
pkg.subdir = deps/psvlib/pkg
FinalHE.subdir = src

psvimg.depends = scrypto
pkg.depends = scrypto
FinalHE.depends = vitamtp psvimg pkg miniz scrypto

win32-msvc* {
    SUBDIRS += iconv libxml2 libusb
    iconv.subdir = deps/iconv
    libxml2.subdir = deps/libxml2
    libusb.subdir = deps/libusb
    vitamtp.depends = iconv libxml2 libusb
    SUBDIRS += zlib
    zlib.subdir = deps/zlib
    psvimg.depends += zlib
    pkg.depends += zlib
}
