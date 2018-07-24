TEMPLATE = lib
CONFIG -= debug_and_release
CONFIG += staticlib
TARGET = libusb

INCLUDEPATH = libusb

SOURCES += \
    libusb/core.c \
    libusb/descriptor.c \
    libusb/hotplug.c \
    libusb/io.c \
    libusb/strerror.c \
    libusb/sync.c \
    
HEADERS += \
    libusb/hotplug.h \
    libusb/libusb.h \
    libusb/libusbi.h \
    libusb/version.h \
    libusb/version_nano.h
win32 {
    SOURCES += \
        libusb/os/poll_windows.c \
        libusb/os/threads_windows.c \
        libusb/os/windows_nt_common.c \
        libusb/os/windows_usbdk.c \
        libusb/os/windows_winusb.c
    HEADERS += \
        libusb/os/poll_windows.h \
        libusb/os/threads_windows.h \
        libusb/os/windows_common.h \
        libusb/os/windows_nt_common.h \
        libusb/os/windows_usbdk.h \
        libusb/os/windows_winusb.h
    DEFINES += OS_WINDOWS
} else {
    SOURCES += \
        libusb/os/poll_posix.c \
        libusb/os/threads_posix.c
    HEADERS += \
        libusb/os/poll_posix.h \
        libusb/os/threads_posix.h
    linux {
        SOURCES += libusb/os/linux_usbfs.c
        HEADERS += libusb/os/linux_usbfs.h
        !DISABLE_UDEV {
            SOURCES += libusb/os/linux_udev.c
        } else {
            SOURCES += libusb/os/linux_netlink.c
        }
        DEFINES += OS_LINUX
    }
    darwin|macx {
        SOURCES += libusb/os/darwin_usb.c
        HEADERS += libusb/os/darwin_usb.h
        DEFINES += OS_DARWIN
    }
    openbsd {
        SOURCES += libusb/os/openbsd_usb.c)
        DEFINES += OS_OPENBSD
    }
    netbsd {
        SOURCES += libusb/os/netbsd_usb.c
        DEFINES += OS_NETBSD
    }
    solaris {
        SOURCES += libusb/os/sunos_usb.c
        HEADERS += libusb/os/sunos_usb.h
        DEFINES += OS_SUNOS
    }
}
