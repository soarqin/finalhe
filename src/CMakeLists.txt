# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif ()
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5LinguistTools REQUIRED)
find_package(Qt5PrintSupport)

set(SRC_FILES
    main.cc
    downloader.cc downloader.hh
    finalhe.cc finalhe.hh
    package.cc package.hh
    vita.cc vita.hh
    worker.cc worker.hh
)

set(UI_FILES
    finalhe.ui
)
set(RES_FILES
    finalhe.rc resource.h finalhe.ico
    resources/xml/psp2-updatelist.xml
)

set(TRANSLATION_FILES
    translations/zh_CN.ts
    translations/zh_TW.ts
)
qt5_add_resources(FINALHE_RCC_SRC finalhe.qrc)

qt5_create_translation(TRANSLATION_MESSAGES main.cc finalhe.cc package.cc vita.cc ${UI_FILES} ${TRANSLATION_FILES})
qt5_add_translation(TRANSLATION_QM ${TRANSLATION_FILES})

source_group(src FILES ${SRC_FILES})
source_group(ui FILES ${UI_FILES})
source_group(res FILES ${RES_FILES} ${FINALHE_RCC_SRC} finalhe.qrc)
source_group(mocs mocs.*)
source_group(translations FILES ${TRANSLATION_FILES} ${TRANSLATION_QM})

qt5_wrap_ui(${CMAKE_CURRENT_BINARY_DIR}/ui_finalhe.h finalhe.ui)

add_definitions(-DUSING_STATIC_LIBICONV -DLIBXML_STATIC)
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif ()
add_executable(FinalHE WIN32 ${SRC_FILES} ${UI_FILES} ${RES_FILES} ${FINALHE_RCC_SRC} ${TRANSLATION_FILES} ${TRANSLATION_QM})
target_link_libraries(FinalHE Qt5::Widgets Qt5::Network)
get_target_property(qt5_target_type Qt5::Core TYPE)
if (MSVC)
    if (qt5_target_type STREQUAL "STATIC_LIBRARY")
        get_target_property(qt5_target_path Qt5::Core LOCATION)
        get_filename_component(qt5_target_dir "${qt5_target_path}" DIRECTORY)
        target_link_libraries(FinalHE debug ${qt5_target_dir}/Qt5PlatformSupportd.lib
                                      debug ${qt5_target_dir}/qtfreetyped.lib
                                      debug ${qt5_target_dir}/qtharfbuzzngd.lib
                                      debug ${qt5_target_dir}/qtpcred.lib
                                      optimized ${qt5_target_dir}/Qt5PlatformSupport.lib
                                      optimized ${qt5_target_dir}/qtfreetype.lib
                                      optimized ${qt5_target_dir}/qtharfbuzzng.lib
                                      optimized ${qt5_target_dir}/qtpcre.lib)
        target_link_libraries(FinalHE ${qt5_target_dir}/ssleay32.lib ${qt5_target_dir}/libeay32.lib)
        target_link_libraries(FinalHE Qt5::QWindowsIntegrationPlugin Imm32 Winmm Iphlpapi Crypt32 OpenGL32)
    else ()
        add_custom_command(TARGET FinalHE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE:Qt5::Network> $<TARGET_FILE_DIR:FinalHE>
            DEPENDS ${TRANSLATION_QM}
        )
    endif ()
endif ()
add_custom_command(TARGET FinalHE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:FinalHE>/language
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TRANSLATION_QM} $<TARGET_FILE_DIR:FinalHE>/language
    DEPENDS ${TRANSLATION_QM}
)

target_link_libraries(FinalHE vitamtp vitapkg psvimg miniz scrypto)
if (APPLE)
    if (qt5_target_type STREQUAL "STATIC_LIBRARY")
        get_target_property(qt5_target_path Qt5::Core LOCATION)
        get_filename_component(qt5_target_dir "${qt5_target_path}" DIRECTORY)
        link_directories(${qt5_target_dir})
        target_link_libraries(FinalHE Qt5::QCocoaIntegrationPlugin Qt5::PrintSupport
                                      ${qt5_target_dir}/libQt5AccessibilitySupport.a
                                      ${qt5_target_dir}/libQt5ThemeSupport.a
                                      ${qt5_target_dir}/libQt5ClipboardSupport.a
                                      ${qt5_target_dir}/libQt5GraphicsSupport.a
                                      ${qt5_target_dir}/libQt5FontDatabaseSupport.a
                                      ${qt5_target_dir}/libqtfreetype.a
                                      ${qt5_target_dir}/libqtharfbuzz.a
                                      ${qt5_target_dir}/libqtlibpng.a
                                      ${qt5_target_dir}/libqtpcre2.a
                                      cups)
        target_link_libraries(FinalHE "-framework DiskArbitration -framework IOKit -framework Foundation -framework CoreServices -framework AppKit -framework Security -framework ApplicationServices -framework CoreFoundation -framework CoreGraphics -framework OpenGL -framework AGL -framework Carbon -framework QuartzCore -framework SystemConfiguration")
    endif ()
endif ()
if (WIN32)
    target_link_libraries(FinalHE shlwapi psapi)
endif ()
