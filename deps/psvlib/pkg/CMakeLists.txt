if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif ()
add_library(vitapkg
    pkg.c pkg.h
    pkg_out.c pkg_out.h
    pkg_sys.c pkg_sys.h
    pkg_utils.h
    pkg_zrif.c pkg_zrif.h)
target_include_directories(vitapkg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vitapkg zlibstatic scrypto)
