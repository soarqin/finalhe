# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(Qt5LinguistTools)

set(SRC_FILES
    main.cc
	downloader.cc downloader.hh
	finalhe.cc finalhe.hh
	log.cc log.hh
	package.cc package.hh
	vita.cc vita.hh
)

set(UI_FILES
    finalhe.ui
)
set(RES_FILES
    finalhe.rc resource.h finalhe.ico
	finalhe.qrc
	resources/xml/psp2-updatelist.xml
)

set(TRANSLATION_FILES
    translations/zh_CN.ts
    translations/zh_TW.ts
)

qt5_create_translation(TRANSLATION_MESSAGES main.cc ${UI_FILES} ${TRANSLATION_FILES})
qt5_add_translation(TRANSLATION_QM ${TRANSLATION_FILES})

source_group(src FILES ${SRC_FILES})
source_group(ui FILES ${UI_FILES})
source_group(res FILES ${RES_FILES})
source_group(mocs mocs.*)
source_group(translations FILES ${TRANSLATION_FILES} ${TRANSLATION_QM})

qt5_wrap_ui(${CMAKE_CURRENT_BINARY_DIR}/ui_finalhe.h finalhe.ui)

add_definitions(-DUSING_STATIC_LIBICONV -DLIBXML_STATIC)
add_executable(finalhe WIN32 ${SRC_FILES} ${UI_FILES} ${RES_FILES} ${TRANSLATION_FILES} ${TRANSLATION_QM})
target_link_libraries(finalhe Qt5::Widgets Qt5::Network vitamtp vitapkg psvimg miniz scrypto)
if (WIN32)
	target_link_libraries(finalhe shlwapi)
endif ()

add_custom_command(TARGET finalhe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE:Qt5::Network> $<TARGET_FILE_DIR:finalhe>
	COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:finalhe>/lang
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TRANSLATION_QM} $<TARGET_FILE_DIR:finalhe>/lang
	DEPENDS ${TRANSLATION_QM}
)
