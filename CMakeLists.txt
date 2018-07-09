cmake_minimum_required(VERSION 3.0)

project(finalhe C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

if (WIN32)
    add_subdirectory(deps/iconv)
    add_subdirectory(deps/zlib)
    add_subdirectory(deps/libusb)
    add_subdirectory(deps/libxml2)
    add_subdirectory(deps/vitamtp)
    set_target_properties(iconv PROPERTIES FOLDER deps)
    set_target_properties(zlibstatic PROPERTIES FOLDER deps)
    set_target_properties(libusb PROPERTIES FOLDER deps)
    set_target_properties(xml2 PROPERTIES FOLDER deps)
    set_target_properties(vitamtp PROPERTIES FOLDER deps)
else ()
    find_package(vitamtp)
endif ()

add_subdirectory(src)
