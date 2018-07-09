set(COMMON_SRC
    libusb/core.c
    libusb/descriptor.c
    libusb/hotplug.c libusb/hotplug.h
    libusb/io.c
    libusb/libusb.h
    libusb/libusbi.h
    libusb/strerror.c
    libusb/sync.c
    libusb/version.h
    libusb/version_nano.h)
if (WIN32)
    set(POLL_THREAD_SRC
        libusb/os/poll_windows.c libusb/os/poll_windows.h
        libusb/os/threads_windows.c libusb/os/threads_windows.h)
    set(OS_SRC
        libusb/os/windows_common.h
        libusb/os/windows_nt_common.c libusb/os/windows_nt_common.h
        libusb/os/windows_usbdk.c libusb/os/windows_usbdk.h
        libusb/os/windows_winusb.c libusb/os/windows_winusb.h)
    add_definitions(-DOS_WINDOWS)
else ()
    set(POLL_THREAD_SRC
        libusb/os/poll_posix.h libusb/os/poll_posix.c
        libusb/os/threads_posix.h libusb/os/threads_posix.c)
    if (LINUX)
        option(USE_UDEV "Use udev in Linux" ON)
        set(OS_SRC
            libusb/os/linux_usbfs.h libusb/os/linux_usbfs.c)
        if (USE_UDEV)
            set(OS_SRC ${OS_SRC} libusb/os/linux_udev.c)
        else ()
            set(OS_SRC ${OS_SRC} libusb/os/linux_netlink.c)
        endif ()
        add_definitions(-DOS_LINUX)
    endif ()
    if (APPLE)
        set(OS_SRC
            libusb/os/darwin_usb.h libusb/os/darwin_usb.c)
        add_definitions(-DOS_DARWIN)
    endif ()
    if (${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
        set(OS_SRC
            libusb/os/openbsd_usb.c)
        add_definitions(-DOS_OPENBSD)
    endif ()
    if (${CMAKE_SYSTEM_NAME} STREQUAL "NetBSD")
        set(OS_SRC
            libusb/os/netbsd_usb.c)
        add_definitions(-DOS_NETBSD)
    endif ()
    if (${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
        set(OS_SRC
            libusb/os/sunos_usb.c libusb/os/sunos_usb.h)
        add_definitions(-DOS_SUNOS)
    endif ()
endif ()
add_library(libusb STATIC ${COMMON_SRC} ${POLL_THREAD_SRC} ${OS_SRC})
target_include_directories(libusb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libusb)
