if (MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif ()
add_library(psvimg
    backup.c backup.h
	crypto.h
	endian-utils.h
    utils.c utils.h
    psvimg-create.c psvimg-create.h
	psvimg.h)

target_include_directories(psvimg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(psvimg scrypto)
if (WIN32)
    target_link_libraries(psvimg zlibstatic)
else ()
    target_link_libraries(psvimg ZLIB::ZLIB)
endif ()
