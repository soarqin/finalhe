add_library(scrypto
    aes.c aes.h aes_x86.c
    base64.c base64.h
    sc_crc32.c sc_crc32.h sc_crc32_x86.c
    sha256.c sha256.h)

target_include_directories(scrypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
