project(codeutils)

########### install target ###############

set(kdevcodeutils_PART_SRCS
    codeutilsplugin.cpp
)

include_directories(CMAKE_CURRENT_BINARY_DIR)

add_library(kdevcodeutils MODULE ${kdevcodeutils_PART_SRCS})

target_link_libraries(kdevcodeutils
    KF5::Parts
    KF5::TextEditor
    ${KDEVPLATFORM_INTERFACES_LIBRARIES}
    ${KDEVPLATFORM_PROJECT_LIBRARIES}
    ${KDEVPLATFORM_UTIL_LIBRARIES}
    ${KDEVPLATFORM_LANGUAGE_LIBRARIES}
)

add_subdirectory(doc_templates)
add_subdirectory(tests)

install(TARGETS kdevcodeutils DESTINATION ${PLUGIN_INSTALL_DIR}/kdevplatform/${KDEV_PLUGIN_VERSION} )


########### install files ###############

configure_file(kdevcodeutils.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdevcodeutils.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdevcodeutils.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
install( FILES kdevcodeutils.rc DESTINATION ${KXMLGUI_INSTALL_DIR}/kdevcodeutils )
