find_package(Threads)

if (MINGW)
    add_definitions(-mno-ms-bitfields)
endif ()
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS)
else ()
    add_definitions(-Wall -Wextra -Wno-unused-parameter -Wno-parentheses)
endif ()
add_definitions(-DHAVE_ICONV -DHAVE_LIMITS_H -DPTP_USB_SUPPORT -DPTP_IP_SUPPORT -DUSING_STATIC_LIBICONV -DLIBXML_STATIC)
set (COMMON_SRC
    vitamtp.h
    datautils.c
    device.c
    ptp.c
    usb.c
    vitamtp.c
    wireless.c)
if (WIN32)
    set (OS_SRC
        asprintf.c
        socketpair.c)
endif ()
add_library(vitamtp STATIC ${COMMON_SRC} ${OS_SRC})
target_include_directories(vitamtp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vitamtp libusb xml2 iconv Threads::Threads)
if (WIN32)
    target_link_libraries(vitamtp ws2_32)
endif ()
