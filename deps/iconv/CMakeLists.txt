add_definitions(-DUSING_STATIC_LIBICONV)
if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif ()
if (MSVC)
	add_definitions(-wd4018)
endif ()
add_library(iconv STATIC
    libiconv/iconv.c
    libiconv/localcharset.c
    libiconv/relocatable.c)
target_include_directories(iconv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
