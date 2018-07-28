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
    resources/raw/h-encore.zip
)

set(TRANSLATION_FILES
    translations/cs_CZ.ts
    translations/es_ES.ts
    translations/it_IT.ts
    translations/ja_JP.ts
    translations/tr_TR.ts
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
if (MSVC)
    add_custom_command(TARGET FinalHE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE:Qt5::Network> $<TARGET_FILE_DIR:FinalHE>
        DEPENDS ${TRANSLATION_QM}
    )
endif ()
add_custom_command(TARGET FinalHE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:FinalHE>/language
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TRANSLATION_QM} $<TARGET_FILE_DIR:FinalHE>/language
    DEPENDS ${TRANSLATION_QM}
)
target_link_libraries(FinalHE vitamtp vitapkg psvimg miniz scrypto)
if (WIN32)
    target_link_libraries(FinalHE shlwapi psapi)
endif ()
