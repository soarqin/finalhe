if (NOT MSVC)
    add_definitions(-maes -mssse3 -mpclmul -msse4)
endif ()
add_library(scrypto
    aes.c aes.h aesni.c
    base64.c base64.h
    sc_crc32.c sc_crc32.h sc_crc32_fast.c
    sha256.c sha256.h)

target_include_directories(scrypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
