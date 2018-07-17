cmake_minimum_required(VERSION 3.0)

project(finalhe C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

if (MSVC)
    option (MSVC_STATIC_RUNTIME "Link to MSVC static runtime libraries" OFF)
    if (MSVC_STATIC_RUNTIME)
        foreach(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if (${flag_var} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif ()
        endforeach(flag_var)
    endif ()
endif ()

if (WIN32)
    add_subdirectory(deps/iconv)
    add_subdirectory(deps/zlib)
    add_subdirectory(deps/miniz)
    add_subdirectory(deps/scrypto)
    add_subdirectory(deps/libusb)
    add_subdirectory(deps/libxml2)
    set_target_properties(iconv PROPERTIES FOLDER deps)
    set_target_properties(zlibstatic PROPERTIES FOLDER deps)
    set_target_properties(miniz PROPERTIES FOLDER deps)
    set_target_properties(scrypto PROPERTIES FOLDER deps)
    set_target_properties(libusb PROPERTIES FOLDER deps)
    set_target_properties(xml2 PROPERTIES FOLDER deps)
endif ()

add_subdirectory(deps/vitamtp)
add_subdirectory(deps/psvlib/pkg)
add_subdirectory(deps/psvlib/img)
set_target_properties(vitamtp PROPERTIES FOLDER vitalibs)
set_target_properties(vitapkg PROPERTIES FOLDER vitalibs)
set_target_properties(psvimg PROPERTIES FOLDER vitalibs)
add_subdirectory(src)
